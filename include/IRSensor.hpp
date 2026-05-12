#pragma once

#include <atomic>    // for std::atomic<bool>-thread-safe flag
#include <lgpio.h>   // for GPIO access
#include <functional>  // for std::function 
#include <chrono>    // for debouncing logic

class IRSensor {
public: 
    // Type definition for the callback function that will be called 
    // when the sensor state changes.
     using EventCallback = std::function<void(bool)>;
    // Constructor to initialize the IR sensor with the specified GPIO pin
    IRSensor(int pin, int gpio_handle, bool active_low) : pin_(pin),
    handle_(gpio_handle), active_low_(active_low), present_(false) {
      
   }
    ~IRSensor() { // Destructor to clean up resources
       lgGpioFree(handle_, pin_); // Free the GPIO handle and pin.
    }

    bool init(EventCallback cb) {
        cb_ = cb; // Store the callback function.
       // Initialize the GPIO pin for input and set up the alert for both edges
    if (lgGpioClaimAlert(handle_, 0, LG_BOTH_EDGES, pin_, -1) < 0) {
            return false;
    }
       
    // Register callback function for GPIO alerts.
        lgGpioSetAlertsFunc(handle_, pin_, callback, this);

        // Read initial state - current state.
        int level = lgGpioRead(handle_, pin_);
        present_.store(active_low_ ? (level == 0) : (level == 1));

        return true;

    }
    bool isPresent() const {
         // Return the current state of the sensor in a thread-safe manner.
         // if the sensor is active low, a level of 0 indicates the container is present.
          return present_.load(std::memory_order_acquire);
    }

private:    

int pin_; // GPIO pin number for the IR sensor
int handle_; // Handle for GPIO access
bool active_low_; // Flag to indicate if the sensor is active low
std::atomic<bool> present_; // Thread-safe flag to indicate if the sensor is present
std::chrono::steady_clock::time_point last_edge_{
    std::chrono::steady_clock::now() - std::chrono::seconds(10)
}; // Timestamp of the lastd debouncing.

EventCallback cb_; // Callback function.
int debounce_ms_ = 50;  // 


// Static callback function to handle GPIO alerts. 
// It updates the [present_] flag based on the sensor's state.
static void callback(int, lgGpioAlert_p evt, void* userdata) {
    IRSensor* self = static_cast<IRSensor*>(userdata);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - self->last_edge_).count();

    // If edge arrived too soon after previous-it is noise, ignore it
    if (elapsed < self->debounce_ms_) return;

    // Edge is real-update timestamp and state
    self->last_edge_ = now;
// Determine if the sensor is currently detecting the container based on the alert level 
// and active_low configuration.
    bool detected = self->active_low_
        ? (evt->report.level == 0)
        : (evt->report.level == 1);
// Update the present_ flag in a thread-safe manner and call the callback function if it is set.
    self->present_.store(detected, std::memory_order_release);
    if (self->cb_) self->cb_(detected);
}

};  
