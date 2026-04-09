#include "actuator.hpp"
#include <lgpio.h>
#include <iostream>

//Actuator — peristaltic pump control via lgpio relay

Actuator::Actuator(const ActuatorConfig& cfg, int gpio_handle)
    : cfg_(cfg), gpio_handle_(gpio_handle) {}

Actuator::~Actuator() {
    stopFilling();
    lgGpioFree(gpio_handle_, cfg_.pump_pin);
}

bool Actuator::init() {
    // Claim single GPIO output for peristaltic pump relay
    if (lgGpioClaimOutput(gpio_handle_, 0, cfg_.pump_pin, 0) < 0) {
        std::cerr << "[Actuator] Failed to claim pump GPIO " << cfg_.pump_pin << "\n";
        return false;
    }
    lgGpioWrite(gpio_handle_, cfg_.pump_pin, 0);  // Ensure pump is off at start
    std::cout << "[Actuator] Peristaltic pump on GPIO " << cfg_.pump_pin << " — ready\n";
    return true;
}

void Actuator::startFilling() {
    if (pumping_.load(std::memory_order_acquire)) return;
    // Single GPIO write —  peristaltic pump
    lgGpioWrite(gpio_handle_, cfg_.pump_pin, 1);
    pumping_.store(true, std::memory_order_release);
    std::cout << "[Actuator] Peristaltic pump ON\n";
}

void Actuator::stopFilling() {
    if (!pumping_.load(std::memory_order_acquire)) return;
    // Single GPIO write — tube self-seals immediately, no drip
    lgGpioWrite(gpio_handle_, cfg_.pump_pin, 0);
    pumping_.store(false, std::memory_order_release);
    std::cout << "[Actuator] Peristaltic pump OFF — tube sealed\n";
}
