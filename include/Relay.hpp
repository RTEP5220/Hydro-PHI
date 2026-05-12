#pragma once
#include <lgpio.h>   // for GPIO access

class Relay
{
public:
// Constructor to initialize the relay with the specified GPIO pin
    Relay(int handle, int pin): handle_(handle), pin_(pin), state_(false) {
        // Initialize the GPIO pin for output
        // Set the initial state of the relay to off (0)
    };

// Destructor to clean up resources
    ~Relay(){
         off(); // Ensure the relay is turned off when the object is destroyed
        lgGpioFree(handle_, pin_); // Free the GPIO handle and pin.
       
    };

    bool init(){
        // Initialize the GPIO pin for output and set initial state
       if(lgGpioClaimOutput(handle_, 0, pin_, 0) < 0) return false;
        
        return true;
    }; 
    void on(){
        lgGpioWrite(handle_, pin_, 1); // Set the GPIO pin high to turn on the relay
        state_ = true; // Update the state to on
    };  
    void off(){
        lgGpioWrite(handle_, pin_, 0); // Set the GPIO pin low to turn off the relay
        state_ = false; // Update the state to off
    };

    bool isOn() const{
        return state_; // Return the current state of the relay
    }; 




private:
    int handle_; // Handle for GPIO access
    int pin_;
    bool state_; // Current state of the relay (On || Off).

};  