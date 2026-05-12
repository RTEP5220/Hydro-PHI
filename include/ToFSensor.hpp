#pragma once
#include <cstdint>    // uint8_t, uint16_t
#include <vector>     // for baseline median
#include <algorithm>  // for std::sort
#include <fcntl.h>    // for open()
#include <unistd.h>   // for read(), write(), close()
#include <sys/ioctl.h>       // for ioctl()
#include <linux/i2c-dev.h>   // for I2C_SLAVE
#include <mutex>      // for std::mutex

#include <thread> 
#include <atomic>


class ToFSensor {
public:
    ToFSensor(int bus, int address);
    ~ToFSensor();

    bool open();           // open I2C, check sensor, run init
    float measure();       // one blocking measurement, returns cm
    float baseline(int n); // take n samples, return median
    float baseline(int n, const std::atomic<bool>& run_flag); // same Baseline-but with stop flag.

    void startThread();

void stopThread();

// Get Latest Distance.
float getLatest() const;

private:
    int bus_;
    int address_;
    int fd_;       // file descriptor
    uint8_t sv_;   // stop variable (needed for ranging)
    std::mutex mtx_; // mutex for thread safety if needed
    bool init();
    // I2C helpers
    // Write one byte to a register and read one or two bytes from a register.
    void    wr8 (uint8_t reg, uint8_t  val);
    uint8_t rd8 (uint8_t reg);
    uint16_t rd16(uint8_t reg);

// Handling Non-Blocking 
std::thread thread_;
std::atomic<float> latest_distance_{-1.0f};
std::atomic<bool> thread_running_{false};


};