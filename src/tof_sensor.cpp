#include "tof_sensor.hpp"
#include "thread_utils.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <iostream>
#include <numeric>
#include <vector>

static constexpr uint16_t REG_SOFT_RESET        = 0x0000;
static constexpr uint16_t REG_SYSTEM_MODE_START = 0x0087;
static constexpr uint16_t REG_RESULT_RANGE_MM   = 0x0096;
static constexpr uint16_t REG_RESULT_STATUS     = 0x0089;
static constexpr uint16_t REG_INT_CLEAR         = 0x0086;

ToFSensor::ToFSensor(const TofConfig& cfg)
    : cfg_(cfg), baseline_cm_(cfg.baseline_cm) {}

ToFSensor::~ToFSensor() {
    stopThread();
    if (i2c_fd_ >= 0) {
        writeReg(REG_SYSTEM_MODE_START, 0x00);
        close(i2c_fd_);
    }
}

bool ToFSensor::init() {
    std::string bus = "/dev/i2c-" + std::to_string(cfg_.i2c_bus);
    i2c_fd_ = open(bus.c_str(), O_RDWR);
    if (i2c_fd_ < 0) {
        std::cerr << "[ToF] Cannot open " << bus << "\n";
        return false;
    }
    if (ioctl(i2c_fd_, I2C_SLAVE, cfg_.i2c_address) < 0) {
        std::cerr << "[ToF] Cannot set I2C address\n";
        return false;
    }
    writeReg(REG_SOFT_RESET, 0x00);
    ThreadUtils::nsDelay(10'000'000);  // 10ms reset hold
    writeReg(REG_SOFT_RESET, 0x01);
    ThreadUtils::nsDelay(100'000'000); // 100ms boot time
    startRanging();
    std::cout << "[ToF] VL53L1X ready on " << bus << "\n";
    return true;
}

bool ToFSensor::startRanging() {
    writeReg(REG_SYSTEM_MODE_START, 0x40);
    ThreadUtils::nsDelay(50'000'000);  // 50ms first measurement
    return true;
}

bool ToFSensor::waitDataReady(int timeout_ms) {
    for (int i = 0; i < timeout_ms / 2; ++i) {
        if (readReg8(REG_RESULT_STATUS) & 0x01) return true;
        ThreadUtils::nsDelay(2'000'000);  // 2ms
    }
    return false;
}

float ToFSensor::readDistanceBlocking() {
    if (!waitDataReady(50)) return -1.0f;
    uint16_t raw_mm = readReg16(REG_RESULT_RANGE_MM);
    writeReg(REG_INT_CLEAR, 0x01);
    float cm = static_cast<float>(raw_mm) / 10.0f;
    return (cm < 0.0f || cm > 200.0f) ? -1.0f : cm;
}

bool ToFSensor::calibrateBaseline(int samples) {
    std::cout << "[ToF] Calibrating baseline (" << samples
              << " samples) — no container present...\n";
    ThreadUtils::nsDelay(1'000'000'000LL);  // 1s settle
    std::vector<float> readings;
    for (int i = 0; i < samples; ++i) {
        float d = readDistanceBlocking();
        if (d > 0.0f) readings.push_back(d);
        ThreadUtils::nsDelay((long)cfg_.poll_interval_ms * 1'000'000LL);
    }
    if (readings.empty()) { std::cerr << "[ToF] Baseline failed\n"; return false; }
    baseline_cm_ = std::accumulate(readings.begin(), readings.end(), 0.0f)
                   / static_cast<float>(readings.size());
    std::cout << "[ToF] Baseline = " << baseline_cm_ << " cm\n";
    return true;
}

// ─── Sensor thread ────────────────────────────────────────────────────────────

void ToFSensor::startThread(int rt_priority) {
    running_.store(true, std::memory_order_release);
    thread_ = std::thread(&ToFSensor::sensorLoop, this);
    ThreadUtils::setThreadName(thread_, "sf_tof");
    // v6 fix 1: fatal=true — abort if RT scheduling fails
    ThreadUtils::setRealtimePriority(thread_, rt_priority, /*fatal=*/true);
    ThreadUtils::setCpuAffinity(thread_, 2);
    std::cout << "[ToF] Sensor thread started (SCHED_FIFO pri=" << rt_priority << ")\n";
}

void ToFSensor::stopThread() {
    running_.store(false, std::memory_order_release);
    if (thread_.joinable()) thread_.join();
}

void ToFSensor::sensorLoop() {
    // v6 fix 6: pre-fault stack before entering RT loop
    ThreadUtils::prefaultStack();

    using namespace std::chrono;
    auto interval  = milliseconds(cfg_.poll_interval_ms);
    auto next_tick = steady_clock::now() + interval;

    while (running_.load(std::memory_order_acquire)) {
        float dist = readDistanceBlocking();

        ToFReading reading;
        reading.distance_cm = (dist > 0.0f) ? dist : latest_.readLatest().distance_cm;
        reading.valid       = (dist > 0.0f);
        reading.timestamp   = steady_clock::now();
        latest_.write(reading);

        ThreadUtils::sleepUntil(next_tick);
        next_tick += interval;
    }
}

ToFReading ToFSensor::getLatest() const { return latest_.readLatest(); }

ContainerProfile ToFSensor::scanContainer() {
    // Called from WorkerThread — blocking is acceptable here
    ContainerProfile profile{};
    profile.baseline_cm = baseline_cm_;

    std::vector<float> readings;
    for (int i = 0; i < 5; ++i) {
        float d = readDistanceBlocking();
        if (d > 0.0f) readings.push_back(d);
        ThreadUtils::nsDelay((long)cfg_.poll_interval_ms * 1'000'000LL);
    }
    if (readings.empty()) return profile;

    float avg  = std::accumulate(readings.begin(), readings.end(), 0.0f)
                 / static_cast<float>(readings.size());
    float drop = baseline_cm_ - avg;
    if (drop < cfg_.rim_detect_delta) return profile;

    profile.rim_distance_cm = avg;
    profile.inner_depth_cm  = drop;
    profile.fill_target_cm  = avg + cfg_.safety_margin_cm;
    profile.valid           = true;
    return profile;
}

bool ToFSensor::writeReg(uint16_t reg, uint8_t val) {
    uint8_t buf[3] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), val };
    return ::write(i2c_fd_, buf, 3) == 3;
}
uint8_t ToFSensor::readReg8(uint16_t reg) {
    uint8_t addr[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    ::write(i2c_fd_, addr, 2);
    uint8_t val = 0; ::read(i2c_fd_, &val, 1); return val;
}
uint16_t ToFSensor::readReg16(uint16_t reg) {
    uint8_t addr[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    ::write(i2c_fd_, addr, 2);
    uint8_t buf[2] = {0, 0}; ::read(i2c_fd_, buf, 2);
    return (uint16_t)((buf[0] << 8) | buf[1]);
}
