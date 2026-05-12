#include "LoadCell.hpp"
#include <iostream>
#include <unistd.h> // for usleep


// Constructor to initialize the load cell with the specified GPIO pin and handle.
LoadCell::LoadCell(int dout_pin, int sck_pin,int gpio_handle)
    : dout_pin_(dout_pin), 
    sck_pin_(sck_pin), 
    gpio_handle_(gpio_handle), 
    offset_(0), 
    scale_(1.0f) 
    {}

// Destructor to clean up resources.
LoadCell::~LoadCell() {
    stopThread(); // Ensure the reading thread is stopped when the LoadCell object is destroyed.
    lgGpioFree(gpio_handle_, dout_pin_); // Free the GPIO pin used for data.
    lgGpioFree(gpio_handle_, sck_pin_); // Free the GPIO pin used for clock.

}


bool LoadCell::init() {
    // Request the GPIO pins for the load cell.
    if (lgGpioClaimInput(gpio_handle_, 0,dout_pin_) < 0) {
        std::cerr << "[LoadCell] Failed to request DOUT pin\n";
        return false;
    }
    // Set the clock pin as output and initialize it to low.
    if (lgGpioClaimOutput(gpio_handle_, 0, sck_pin_,0) < 0) {
        std::cerr << "[LoadCell] Failed to request SCK pin\n";
        lgGpioFree(gpio_handle_, dout_pin_); // Free the previously requested DOUT pin.
        return false;
    }
    // Set the clock pin to low initially.
   std::cout << "[LoadCell] Load cell initialized successfully - Dout_pin: "
   <<dout_pin_<<" SCK Pin: "<<sck_pin_<<"\n";

    return true;
}


// Wait for DOUT LOW then, clock out 24 bits
float LoadCell::readBlocking() {
    // Wait for DOUT to go LOW - means data ready
    int timeout = 500;
    while (lgGpioRead(gpio_handle_, dout_pin_) == 1) {
        if (--timeout == 0) return -1.0f;  // timeout
        usleep(1000);  // wait 1ms
    }

    // Clock out 24 bits
    int32_t raw = 0;
    for (int i = 0; i < 24; i++) {
        lgGpioWrite(gpio_handle_, sck_pin_, 1);  // SCK HIGH
        usleep(1);
        int bit = lgGpioRead(gpio_handle_, dout_pin_);
        lgGpioWrite(gpio_handle_, sck_pin_, 0);  // SCK LOW
        usleep(1);
        raw = (raw << 1) | bit;
    }

    // 25th pulse — sets gain 128 channel A
    lgGpioWrite(gpio_handle_, sck_pin_, 1); // SCK HIGH
    usleep(1); 
    lgGpioWrite(gpio_handle_, sck_pin_, 0); // SCK LOW
    usleep(1);

    // Convert to signed 24-bit
    if (raw & 0x800000) raw |= 0xFF000000;

    // Apply tare and scale
    float weight = (raw - offset_) / scale_;
    return weight;
}

// Tare the load cell, setting the current weight as the zero point.
// Zero the scale - take 10 readings and store median as offset
void LoadCell::tare() {
    int32_t sum = 0;
    int count = 0;
    for (int i = 0; i < 10; i++) {
        // Temporarily zero offset to get raw reading
        int32_t saved = offset_;
        offset_ = 0;
        float r = readBlocking();
        offset_ = saved;
        if (r > -1.0f) {
            sum += static_cast<int32_t>(r);
            count++;
        }
        usleep(100000);
    }
    if (count > 0) {
        offset_ = sum / count;
        std::cout << "[LoadCell] Tare Complete Offset=" << offset_ << "\n";
    } else {
        std::cerr << "[LoadCell] Tare failed - No Valid Readings\n";
    }
}



// Get latest weight.
float LoadCell::getWeight() const {
    return latest_weight_.load(std::memory_order_acquire); // Return the latest weight reading from the load cell.
}


// Thread - continuously reads weight in background
void LoadCell::startThread() {
    thread_running_ = true;
    thread_ = std::thread([this]() {
        while (thread_running_) {
            float w = readBlocking();
            if (w > -1.0f) {
                latest_weight_.store(w, std::memory_order_release);
            }
        }
    });
    std::cout << "[LoadCell] Thread started\n";
}

// Stop the reading thread and clean up resources.
void LoadCell::stopThread() {
    thread_running_ = false;
    if (thread_.joinable()) thread_.join();
}