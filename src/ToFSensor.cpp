#include "ToFSensor.hpp"
#include <iostream>
#include <string>

// Constructor to initialize the ToF sensor 
// with the specified I2C bus and address.
ToFSensor::ToFSensor(int bus, int address)
    : bus_(bus)
    , address_(address)
    , fd_(-1)
    , sv_(0)
{}

// Destructor - just close the file descriptor
// Never write to sensor here - puts it to sleep
ToFSensor::~ToFSensor() {
    stopThread();
    if (fd_ >= 0) close(fd_);
}

// Write one byte to a register
void ToFSensor::wr8(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    ::write(fd_, buf, 2);
}

// Read one byte from a register
uint8_t ToFSensor::rd8(uint8_t reg) {
    ::write(fd_, &reg, 1);
    uint8_t val = 0;
    ::read(fd_, &val, 1);
    return val;
}

// Read two bytes from a register 
// combine them into a 16-bit value.
uint16_t ToFSensor::rd16(uint8_t reg) {
    ::write(fd_, &reg, 1);
    uint8_t buf[2] = {};
    ::read(fd_, buf, 2);
    return static_cast<uint16_t>((buf[0] << 8) | buf[1]);
}


//////////////Sensor Initialization.

 bool ToFSensor::open() {
    // Open the I2C bus
    std::string dev = "/dev/i2c-" + std::to_string(bus_);
    fd_ = ::open(dev.c_str(), O_RDWR);
    if (fd_ < 0) {
        std::cerr << "[ToF] Cannot open " << dev << "\n";
        return false;
    }

    // Set the I2C slave address
    if (ioctl(fd_, I2C_SLAVE, address_) < 0) {
        std::cerr << "[ToF] Cannot set slave address\n";
        return false;
    }

    // Wake sensor from any previous sleep state
    uint8_t wake[2] = {0x80, 0x00};
    ::write(fd_, wake, 2);
    usleep(10000);  // 10ms settle

    // Check model ID
    // VL53L0X Returns 0xEE
    uint8_t id = rd8(0xC0);
    if (id != 0xEE) {
        std::cerr << "[ToF] Wrong model ID: 0x" << std::hex << (int)id << "\n";
        return false;
    }
    std::cout << "[ToF] VL53L0X detected  rev=0x"
              << std::hex << (int)rd8(0xC2) << std::dec << "\n";

    // Run initialisation sequence
    if (!init()) {
        std::cerr << "[ToF] Init sequence failed\n";
        return false;
    }

    std::cout << "[ToF] Ready on " << dev << "\n";
    return true;
}

////

bool ToFSensor::init() {
    // Store stop variable - needed for single-shot ranging later
    wr8(0x88, 0x00);
    wr8(0x80, 0x01); wr8(0xFF, 0x01); wr8(0x00, 0x00);
    sv_ = rd8(0x91);
    wr8(0x00, 0x01); wr8(0xFF, 0x00); wr8(0x80, 0x00);

    // ST required tuning values 
    static const uint8_t tuning[][2] = {
        {0xFF,0x01},{0x00,0x00},{0xFF,0x00},{0x09,0x00},
        {0x10,0x00},{0x11,0x00},{0x24,0x01},{0x25,0xFF},
        {0x75,0x00},{0xFF,0x01},{0x4E,0x2C},{0x48,0x00},
        {0x30,0x20},{0xFF,0x00},{0x30,0x09},{0x54,0x00},
        {0x31,0x04},{0x32,0x03},{0x40,0x83},{0x46,0x25},
        {0x60,0x00},{0x27,0x00},{0x50,0x06},{0x51,0x00},
        {0x52,0x96},{0x56,0x08},{0x57,0x30},{0x61,0x00},
        {0x62,0x00},{0x64,0x00},{0x65,0x00},{0x66,0xA0},
        {0xFF,0x01},{0x22,0x32},{0x47,0x14},{0x49,0xFF},
        {0x4A,0x00},{0xFF,0x00},{0x7A,0x0A},{0x7B,0x00},
        {0x78,0x21},{0xFF,0x01},{0x23,0x34},{0x42,0x00},
        {0x44,0xFF},{0x45,0x26},{0x46,0x05},{0x40,0x40},
        {0x0E,0x06},{0x20,0x1A},{0x43,0x40},{0xFF,0x00},
        {0x34,0x03},{0x35,0x44},{0xFF,0x01},{0x31,0x04},
        {0x4B,0x09},{0x4C,0x05},{0x4D,0x04},{0xFF,0x00},
        {0x44,0x00},{0x45,0x20},{0x47,0x08},{0x48,0x28},
        {0x67,0x00},{0x70,0x04},{0x71,0x01},{0x72,0xFE},
        {0x76,0x00},{0x77,0x00},{0xFF,0x01},{0x0D,0x01},
        {0xFF,0x00},{0x80,0x01},{0x01,0xF8},
        {0xFF,0x01},{0x8E,0x01},{0x00,0x01},
        {0xFF,0x00},{0x80,0x00}
    };
    for (auto& p : tuning) wr8(p[0], p[1]);

    // Configure interrupt
    wr8(0x0A, 0x04);
    wr8(0x84, rd8(0x84) & ~0x10);
    wr8(0x0B, 0x01);
    wr8(0x01, 0xE8);

    // VHV calibration
    wr8(0x01, 0x01);
    wr8(0x00, 0x01 | 0x40);
    usleep(10000);
    for (int t = 0; t < 2000 && (rd8(0x13) & 7) == 0; t++) usleep(1000);
    if ((rd8(0x13) & 7) == 0) {
        std::cerr << "[ToF] VHV calibration timeout\n";
        return false;
    }
    wr8(0x0B, 0x01); wr8(0x00, 0x00);
    usleep(10000);

    // Phase calibration
    wr8(0x01, 0x02);
    wr8(0x00, 0x01);
    usleep(10000);
    for (int t = 0; t < 2000 && (rd8(0x13) & 7) == 0; t++) usleep(1000);
    if ((rd8(0x13) & 7) == 0) {
        std::cerr << "[ToF] Phase calibration timeout\n";
        return false;
    }
    wr8(0x0B, 0x01); wr8(0x00, 0x00);
    usleep(10000);

    wr8(0x01, 0xE8);
    return true;
}


/// ToF measurement///

// Single blocking measurement 
// Returns distance in cm, -1 on error
float ToFSensor::measure() {
    std::lock_guard<std::mutex> lock(mtx_); // Ensure thread safety if called from multiple threads.
    // Trigger single-shot ranging
    wr8(0x80, 0x01); wr8(0xFF, 0x01); wr8(0x00, 0x00);
    wr8(0x91, sv_);
    wr8(0x00, 0x01); wr8(0xFF, 0x00); wr8(0x80, 0x00);
    wr8(0x00, 0x01);

    // Wait for sensor to accept.
    for (int t = 0; t < 200 && (rd8(0x00) & 1); t++) usleep(1000);

    // Wait for result ready
    for (int t = 0; t < 300 && (rd8(0x13) & 7) == 0; t++) usleep(1000);

    // Read raw range result
    uint16_t raw = rd16(0x1E);

    // Clear interrupt
    wr8(0x0B, 0x01);

    // Validate result
    if (raw >= 8190) return -1.0f;
    float cm = raw / 10.0f;
    if (cm < 1.0f || cm > 120.0f) return -1.0f;
    return cm;
}

// Take N measurements and return the median
// Median is more reliable than average.
float ToFSensor::baseline(int n) {
    std::vector<float> readings;
    for (int i = 0; i < n; i++) {
        float d = measure();
        if (d > 0) readings.push_back(d);
        usleep(260000);  // 260ms between measurements
    }
    if (readings.empty()) return -1.0f;
    std::sort(readings.begin(), readings.end());
    return readings[readings.size() / 2];
}

float ToFSensor::baseline(int n, const std::atomic<bool>& run_flag) {
    std::vector<float> readings;
    for (int i = 0; i < n; i++) {
        if (!run_flag) return -1.0f;
        float d = measure();
        if (d > 0) readings.push_back(d);
        usleep(260000);
    }
    if (readings.empty()) return -1.0f;
    std::sort(readings.begin(), readings.end());
    return readings[readings.size() / 2];
}

// Start a background thread that continuously measures distance and updates the value.
void ToFSensor::startThread() { 
    thread_running_ = true;
    thread_ = std::thread([this]() {
        while (thread_running_) {
            float d = measure();  // blocks its own thread
            latest_distance_.store(d, std::memory_order_release);
        }
    });
}
// Stop the background thread and wait for it to finish.
void ToFSensor::stopThread() {
    thread_running_ = false;
    if (thread_.joinable()) thread_.join();
}

// Get the latest distance measurement from the background thread.
float ToFSensor::getLatest() const {
    return latest_distance_.load(std::memory_order_acquire);
}