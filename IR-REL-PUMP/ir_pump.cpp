/**
 * IR Proximity Sensor + Relay for Pump Control
 * Raspberry Pi 5 — lgpio library
 * =========================================
 *
 * Wiring:
 *   IR Sensor Module:
 *     VCC -> 3.3V or 5V
 *     GND -> GND
 *     DO  -> GPIO 17 (pin 11)
 *
 *   Relay Module:
 *     VCC -> 5V  (pin 2)
 *     GND -> GND (pin 6)
 *     IN  -> GPIO 27 (pin 13)
 *
 *   Relay Contacts (12V Pump):
 *     COM -> 12V Supply +
 *     NO  -> Pump +
 *     Pump - -> 12V GND
 *
 * Logic:
 *   IR detects object (DO = LOW)  → Relay ON .. Pump runs
 *   No object         (DO = HIGH) → Relay OFF .. Pump stops
 *
 * Build:
 *   sudo apt install liblgpio-dev
 *   g++ -o ir_pump ir_pump.cpp -llgpio
 *
 * Run:
 *   sudo ./ir_pump
 */

#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <lgpio.h>

// ── Pin Configuration ─────────────────────────────────────────────────────────
constexpr int IR_PIN    = 17;   // GPIO 17 — IR sensor digital output (DO)
constexpr int RELAY_PIN = 27;   // GPIO 27 — Relay module IN
constexpr int POLL_MS   = 100;  // Polling interval in milliseconds
// ─────────────────────────────────────────────────────────────────────────────

std::atomic<bool> running{true};
int gpio_handle = -1;

void signal_handler(int) {
    running = false;
}

// Try opening gpiochip0, fall back to gpiochip4 (Pi 5 header)
int open_gpio_chip() {
    int h = lgGpiochipOpen(0);
    if (h >= 0) return h;
    h = lgGpiochipOpen(4);
    if (h >= 0) {
        std::cout << "Using gpiochip4 (Raspberry Pi 5)\n";
        return h;
    }
    return -1;
}

void set_relay(int handle, bool on) {
    lgGpioWrite(handle, RELAY_PIN, on ? 1 : 0);
}

int main() {
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Open GPIO chip
    gpio_handle = open_gpio_chip();
    if (gpio_handle < 0) {
        std::cerr << "ERROR: Cannot open GPIO chip. Try running with sudo.\n";
        return 1;
    }

    // Setup IR pin as input with pull-up
    int rc = lgGpioClaimInput(gpio_handle, LG_SET_PULL_UP, IR_PIN);
    if (rc < 0) {
        std::cerr << "ERROR: Cannot claim GPIO " << IR_PIN
                  << ": " << lguErrorText(rc) << "\n";
        lgGpiochipClose(gpio_handle);
        return 1;
    }

    // Setup Relay pin as output, default OFF
    rc = lgGpioClaimOutput(gpio_handle, 0, RELAY_PIN, 0);
    if (rc < 0) {
        std::cerr << "ERROR: Cannot claim GPIO " << RELAY_PIN
                  << ": " << lguErrorText(rc) << "\n";
        lgGpioFree(gpio_handle, IR_PIN);
        lgGpiochipClose(gpio_handle);
        return 1;
    }

    std::cout << "IR Proximity + Pump Controller\n"
              << "────────────────────────────────\n"
              << "IR Sensor  : GPIO " << IR_PIN    << "\n"
              << "Relay/Pump : GPIO " << RELAY_PIN << "\n"
              << "Press Ctrl+C to stop.\n\n";

    int prev_ir = -1;

    while (running) {
        int ir_level = lgGpioRead(gpio_handle, IR_PIN);

        if (ir_level < 0) {
            std::cerr << "Read error: " << lguErrorText(ir_level) << "\n";
            break;
        }

        // Active-low: LOW (0) = object detected
        bool object_detected = (ir_level == 0);

        // Only act and print on state change
        if (ir_level != prev_ir) {
            if (object_detected) {
                set_relay(gpio_handle, true);
                std::cout << "[DETECTED]  Object found - Pump ON  \n";
            } else {
                set_relay(gpio_handle, false);
                std::cout << "[CLEAR]     No object - Pump OFF \n";
            }
            prev_ir = ir_level;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_MS));
    }

    // Safe shutdown: turn off relay before exit
    std::cout << "\nShutting down — turning pump OFF...\n";
    set_relay(gpio_handle, false);

    lgGpioFree(gpio_handle, IR_PIN);
    lgGpioFree(gpio_handle, RELAY_PIN);
    lgGpiochipClose(gpio_handle);

    std::cout << "GPIO released. Goodbye.\n";
    return 0;
}
