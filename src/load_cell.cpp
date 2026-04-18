#include "load_cell.hpp"
#include "thread_utils.hpp"
#include <lgpio.h>
#include <iostream>
#include <vector>
#include <numeric>
#include <cmath>
#include <time.h>

LoadCell::LoadCell(const LoadCellConfig& cfg, int gpio_handle)
    : cfg_(cfg), gpio_handle_(gpio_handle), cal_factor_(cfg.calibration_factor) {}

LoadCell::~LoadCell() { stopThread(); }

bool LoadCell::init() {
    if (lgGpioClaimInput(gpio_handle_,  0, cfg_.data_pin)  < 0 ||
        lgGpioClaimOutput(gpio_handle_, 0, cfg_.clock_pin, 0) < 0) {
        std::cerr << "[LoadCell] GPIO claim failed\n";
        return false;
    }
    // Power-cycle HX711: pull SCK high > 60µs triggers power-down,
    // then bring low to wake. Gives a known clean state at startup.
    lgGpioWrite(gpio_handle_, cfg_.clock_pin, 1);
    ThreadUtils::nsDelay(100'000);   // 100µs — well above 60µs power-down threshold
    lgGpioWrite(gpio_handle_, cfg_.clock_pin, 0);
    ThreadUtils::nsDelay(400'000'000); // 400ms settling time per HX711 datasheet

    std::cout << "[LoadCell] HX711 ready DOUT=" << cfg_.data_pin
              << " SCK=" << cfg_.clock_pin << "\n";
    return true;
}

bool LoadCell::isHX711Ready() const {
    // HX711 pulls DOUT LOW when conversion is complete and data is ready
    return lgGpioRead(gpio_handle_, cfg_.data_pin) == 0;
}

long LoadCell::readRaw() {
    // ── Wait for DOUT LOW (conversion complete) ──
    // Timeout after 100ms — HX711 conversion rate is 10–80 Hz
    int retries = 500;
    while (!isHX711Ready() && retries-- > 0) {
        ThreadUtils::nsDelay(200'000);  // 200µs between polls
    }
    if (retries <= 0) {
        std::cerr << "[LoadCell] DOUT timeout — HX711 not responding\n";
        return tare_offset_;
    }

    long value = 0;

    // ── Clock in 24 bits MSB first ────────────────────────────────────────
    //   t1 (PD_SCK HIGH time):  min 200ns, typical 1µs
    //   t2 (PD_SCK LOW time):   min 200ns, typical 1µs
    //   t3 (DOUT valid after PD_SCK falling edge): max 100ns
    for (int i = 0; i < 24; ++i) {
        lgGpioWrite(gpio_handle_, cfg_.clock_pin, 1);
        ThreadUtils::nsDelay(300);    // Hold SCK HIGH ≥ 200ns
        int bit = lgGpioRead(gpio_handle_, cfg_.data_pin);
        lgGpioWrite(gpio_handle_, cfg_.clock_pin, 0);
        ThreadUtils::nsDelay(300);    // Hold SCK LOW ≥ 200ns
        value = (value << 1) | bit;
    }

    // ── 25th clock pulse: sets gain=128, channel A ────────────────────────
    lgGpioWrite(gpio_handle_, cfg_.clock_pin, 1);
    ThreadUtils::nsDelay(300);
    lgGpioWrite(gpio_handle_, cfg_.clock_pin, 0);
    ThreadUtils::nsDelay(300);

    // ── Two's complement: 24-bit signed → long ────────────────────────────
    if (value & 0x800000) value |= ~0xFFFFFF;
    return value;
}

long LoadCell::averageSamples(int n) {
    std::vector<long> v;
    v.reserve(n);
    for (int i = 0; i < n; ++i) v.push_back(readRaw());
    long sum = 0;
    for (auto x : v) sum += x;
    return sum / static_cast<long>(n);
}

bool LoadCell::tare(int samples) {
    // Called from WorkerThread — blocking is fine here
    std::cout << "[LoadCell] Taring (" << samples << " samples)...\n";
    tare_offset_ = averageSamples(samples);
    last_weight_  = 0.0f;
    stable_count_ = 0;
    std::cout << "[LoadCell] Tare offset = " << tare_offset_ << "\n";
    return true;
}

bool LoadCell::isStable(float current) {
    float delta = std::fabs(current - last_weight_);
    if (delta < cfg_.weight_delta_g) stable_count_++;
    else                             stable_count_ = 0;
    last_weight_ = current;
    return (stable_count_ >= cfg_.stable_cycles) && (current > cfg_.present_threshold_g);
}

// ─── Load Cell Thread ─────────────────────────────────────────────────────────

void LoadCell::startThread(int rt_priority) {
    running_.store(true, std::memory_order_release);
    thread_ = std::thread(&LoadCell::weightLoop, this);
    ThreadUtils::setThreadName(thread_, "sf_loadcell");
    ThreadUtils::setRealtimePriority(thread_, rt_priority, /*fatal=*/true);
    ThreadUtils::setCpuAffinity(thread_, 3);
    std::cout << "[LoadCell] Thread started (RT priority=" << rt_priority << ")\n";
}

void LoadCell::stopThread() {
    running_.store(false, std::memory_order_release);
    if (thread_.joinable()) thread_.join();
}

void LoadCell::weightLoop() {
    ThreadUtils::prefaultStack();

    using namespace std::chrono;
    auto interval  = milliseconds(cfg_.poll_interval_ms);
    auto next_tick = steady_clock::now() + interval;

    while (running_.load(std::memory_order_acquire)) {
        long  raw    = readRaw();
        float weight = static_cast<float>(raw - tare_offset_) / cal_factor_;
        if (weight < 0.0f) weight = 0.0f;

        WeightReading reading;
        reading.weight_g  = weight;
        reading.stable    = isStable(weight);
        reading.valid     = true;
        reading.timestamp = steady_clock::now();
        latest_.write(reading);

        ThreadUtils::sleepUntil(next_tick);
        next_tick += interval;
    }
}

WeightReading LoadCell::getLatest() const { return latest_.readLatest(); }
bool LoadCell::isContainerPresent() const {
    return latest_.readLatest().weight_g > cfg_.present_threshold_g;
}
