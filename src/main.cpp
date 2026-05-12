#include <iostream> 
#include <thread> 
#include <chrono>
#include <csignal>
#include <lgpio.h>
#include<atomic>
#include "IRSensor.hpp"
#include "Relay.hpp"
#include "ToFSensor.hpp"
#include "LoadCell.hpp"



// static bool running = true; - Change to atomic for thread safety.
static std::atomic<bool> running{true};

static std::atomic<bool> container_present{false};
// static bool container_present = false;
void sigHandler(int) { running = false; }

//Defining the State Machine for the Filling Process.
enum class State { IDLE, SCANNING, FILLING, DONE };



int main() {


std::cout << "HydroPHI starting...\n";

/////////////////////// ToF Calibration Block /////////////////////////
///// Open ToF - Ensure it is Ready.
ToFSensor tof(1, 0x29);
if (!tof.open()) {
    std::cerr << "ToF failed\n";
    return 1;
}
std::cout << "ToF ready\n";

// Take baseline - Calibration Process//
float base = tof.baseline(5);
std::cout << "Baseline = " << base << " cm\n";
if (base < 10.0f){
    std::cerr << "Baseline is too small - Ensure the Platform is Free.\n";
    return 1;
}
//////////////////////////END of ToF Calibration Block //////////////////////.

// Open GPIO chip 0 (the first one) and get a handle for GPIO access.
    int gpio_handle = lgGpiochipOpen(0);
    if (gpio_handle < 0) {
        std::cerr << "Failed to open GPIO chip\n";
        return 1;
    }
    // Successfully opened GPIO chip, now we can initialize our IR sensor and relay.
    std::cout << "GPIO chip opened successfully\n";

////////////////////////////INITIALIZING SENSORS - ACTUATORS//////////////////////////////////////////

    // Initialize the relay on GPIO pin 22.
    Relay relay(gpio_handle, 22);
    if (!relay.init()) {
        std::cerr << "Pump initialization failed\n";
        lgGpiochipClose(gpio_handle);
        return 1;
    }
    std::cout << "Pump Ready\n";
//Initializing the Load Cell on GPIO pins 20 (DOUT) and 21 (SCK).
LoadCell lc(5, 6, gpio_handle);  // DOUT=GPIO5, SCK=GPIO6
if (!lc.init()) {
    std::cerr << "[LoadCell] Initialization Failed\n";
    lgGpiochipClose(gpio_handle);
    return 1;
}
std::cout << "[LoadCell] Ready\n";



    // Initialize the IR sensor on GPIO pin 16, active low configuration.
    IRSensor ir(16, gpio_handle, false);
    if (!ir.init([](bool present) {
      container_present.store(present, std::memory_order_release); 
        if (present) {               
            std::cout << "Container detected\n";             
        } else {
            std::cout << "Container removed \n";
        }
    })) {
        std::cerr << "IR init failed\n";
        lgGpiochipClose(gpio_handle);
        return 1;
    }
    std::cout << "IR sensor ready\n";

    // Set up signal handler for shutdown on Ctrl+C.
    signal(SIGINT, sigHandler);

    // Defining Initial State, rim and target fill.
    State state = State::IDLE;
    float rim    = 0.0f;
    float target = 0.0f;

/////////// Process Preparation  /////////////////////////
    
/////////1. Actuator Safety //////////////////////////////// 

    relay.off(); // Ensure the pump is off at startup. - Safety First.

///////// 2. Prepare the LoadCell /////////
lc.tare(); 
lc.startThread();


//////////////////////3. Prepare the Thread for System inspection 
    tof.startThread(); // Starting the ToF Thread.

  
 
/////////////////////4. System Ready ////////////////////////////////
    std::cout << "Place a container...\n";
       
///////////////// Start Filling Process using 4-States ///////////////////////

///// According to State, each Case is handled ////////////////////////////// 

while (running) {
    
        bool present = container_present.load(std::memory_order_acquire);
        // Sate IDLE //  
        switch(state) {
            case State::IDLE:
            relay.off();
            if (present) {
            std::cout << "IDLE: Container detected - Scanning Process ... \n";
            state = State::SCANNING;
            }else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }break;

            // In the SCANNING state, 
            // we take a baseline measurement to find the rim level of the container.
            case State::SCANNING: {
            if (!present) {
            std::cout << "SCANNING: Container removed - Back to IDLE\n";
            state = State::IDLE;
            break;
            }
            std::cout << "SCANNING: Measuring Container Rim...\n";
            tof.stopThread();
            float r = tof.baseline(3, container_present);
            tof.startThread();

            if (r < 0 || !container_present.load()) {
            std::cout << "SCANNING: Aborted - Container Removed\n";
            state = State::IDLE;
            break;
            }
            // 
        float drop = base - r;
        std::cout << "SCANNING: Rim=" << r << "cm  drop=" << drop << "cm\n";

        if (drop < 2.0f) {
        std::cout << "SCANNING: Drop too small — retrying\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        break;
        }
        // Storing Rim and Filling Target Calculations (in Percentage).
        rim    = r;
        target = rim - (drop * 0.90f); // Target = 90% of Container. - Leaving 10% Safety Margin.
        std::cout << "SCANNING: Target = " << target << " cm\n";
        relay.on();
        std::cout << "SCANNING: Pump ON - Filling Process ... \n";
        state = State::FILLING;
        break;
        }
        // In the FILLING state, 
        // we continuously monitor the distance until we reach the target level 
        // Or the container is removed.          
            case State::FILLING: {
            if (!present) {
            relay.off();
            std::cout << "FILLING: Container Removed — Pump OFF\n";
            state = State::IDLE;
            break;
            }

            float d = tof.getLatest(); // Get the Latest distance -Sync- Reading from ToF.
          
                if (d < 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                break;
               }
             std::cout << "Filling " << d << "cm  Target=" << target << "cm\n";
             if (d < rim && d <= target) {
                relay.off();
                std::cout << "FILLING: Fill Complete - Pump OFF\n";
                state = State::DONE;
                break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                break;
                }

              // DONE State
              // Container is Filled and Successfully Removed 
              // System waits for the next container.

                case State::DONE:
                relay.off();
                if (!present) {
                std::cout << "DONE: Container Removed - Ready for Next ... \n\n";
                state = State::IDLE;
                } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                break;
                // default:
                //  state = State::IDLE;
        }
    }
    
    // Clean up: turn off the relay and close the GPIO handle before exiting.
    tof.stopThread(); // Closing the ToF Thread.
    lc.stopThread(); // Stop LoadCell Thread.
    relay.off(); // Turn the Pump off 
    lgGpiochipClose(gpio_handle); // Releasing GPIO
    std::cout << "Done\n";
    return 0;
}


// ______________________________
