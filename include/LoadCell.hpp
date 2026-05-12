#pragma once
#include <thread>    // for std::thread
#include <atomic>    // for std::atomic<bool>-thread-safe flag
#include <lgpio.h>   // for GPIO access
#include <cstdint>   // for int32_t



// LoadCell class definition for interfacing with 
// a load cell sensor.
class LoadCell {
public:
// Constructor to initialize the load cell with the specified GPIO pin and handle.    
LoadCell(int dout_pin, int sck_pin,int gpio_handle);
// Destructor to clean up resources.
~LoadCell();


bool init(); //initialize the load cell.
void tare(); //tare the load cell, setting the current weight as the zero point.

float getWeight() const; // read the current weight from the load cell.

void startThread(); //start a thread that continuously reads weight data from the load cell.
void stopThread(); //stop the reading thread and clean up resources.


private:
    int gpio_handle_; // Handle for GPIO access.
    int dout_pin_; // GPIO pin number for the data line of the load cell.
    int sck_pin_; // GPIO pin number for the clock line of the load cell.

    int32_t offset_; //store the tare output value.
    float scale_; //store the scale factor for converting raw readings to weight.

    float readBlocking(); // read raw data from the load cell in a blocking manner.
    
  std::thread thread_; // Thread for continuously reading weight data from the load cell.
  std::atomic<float> latest_weight_ {0.0f}; // Variable to store the latest weight reading from the load cell.
  std::atomic<bool> thread_running_ {false}; // Atomic flag to control the reading thread.


};