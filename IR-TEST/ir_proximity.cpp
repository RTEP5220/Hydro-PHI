/**
 * IR Proximity Sensor Interface for Raspberry Pi 5
 * ================================================
 * Uses the lgpio library (RPi 5).
 *
 * Wiring (Digital IR Module e.g. GENERIC IR):
 *   VCC  -> 3.3V or 5V (pin 1 or 2)
 *   GND  -> GND         (pin 6)
 *   DO   -> GPIO 17     (pin 11)  [digital output]
 *
 * Build:
 *   sudo apt install liblgpio-dev
 *   g++ -o ir_proximity ir_proximity.cpp -llgpio
 *
 * Run:
 *   sudo ./ir_proximity
 */

#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <lgpio.h>

// ── Configuration ────────────────────────────────────────────────────────────
constexpr int IR_PIN        = 17;   // GPIO pin 
constexpr int POLL_INTERVAL = 100;  // ms between reads
constexpr int CHIP          = 0;    // /dev/gpiochip0 
                                    
// ─────────────────────────────────────────────────────────────────────────────

std::atomic<bool> running{true};

void signal_handler(int) {
    running = false;
}

// Returns a human-readable label for the sensor state.
// Most digital IR modules output LOW (0) when an object is DETECTED,
// and HIGH (1) when no object is present (active-low logic).
const char* interpret(int level) {
    return (level == 0) ? "DETECTED  " : "clear  ";
}

int main() {
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Open GPIO chip
    int handle = lgGpiochipOpen(CHIP);
    if (handle < 0) {
        // Pi 5 GPIO header is on chip 4
        handle = lgGpiochipOpen(4);
        if (handle < 0) {
            std::cerr << "ERROR: Cannot open gpiochip. "
                      << "Try running with sudo.\n"
                      << "lgpio error: " << lguErrorText(handle) << "\n";
            return 1;
        }
        std::cout << "Using gpiochip4 (Raspberry Pi 5 header)\n";
    }

    // Claim the pin as input
    int rc = lgGpioClaimInput(handle, LG_SET_PULL_UP, IR_PIN);
    if (rc < 0) {
        std::cerr << "ERROR: Cannot claim GPIO " << IR_PIN << ": "
                  << lguErrorText(rc) << "\n";
        lgGpiochipClose(handle);
        return 1;
    }

    std::cout << "IR Proximity Sensor Monitor\n"
              << "────────────────────────────\n"
              << "GPIO pin : " << IR_PIN << "\n"
              << "Press Ctrl+C to quit.\n\n";

    int prev_level = -1;

    while (running) {
        int level = lgGpioRead(handle, IR_PIN);

        if (level < 0) {
            std::cerr << "Read error: " << lguErrorText(level) << "\n";
            break;
        }

        // Print on state change (edge detection in software)
        if (level != prev_level) {
            std::cout << "Status: " << interpret(level) << "\n";
            prev_level = level;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL));
    }

    // Cleanup
    lgGpioFree(handle, IR_PIN);
    lgGpiochipClose(handle);
    std::cout << "\nGPIO released. Goodbye.\n";
    return 0;
}
