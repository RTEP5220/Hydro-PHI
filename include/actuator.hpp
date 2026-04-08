#pragma once
#include "config.hpp"
#include <atomic>

// ─────────────────────────────────────────────────────────────────────────────
// Actuator — peristaltic pump control via lgpio relay
//
// A peristaltic pump works by squeezing a flexible tube with rollers.
// When the motor stops, the tube springs back and pinches shut — no liquid
// can flow forward or back. This gives a clean, drip-free cut-off without
// any valve sequencing. startFilling() and stopFilling() are now a single
// GPIO write each, with no inter-device delays.
//
// isPumping() is lock-free atomic — safe to read from any thread.
// ─────────────────────────────────────────────────────────────────────────────
class Actuator {
public:
    explicit Actuator(const ActuatorConfig& cfg, int gpio_handle);
    ~Actuator();

    bool init();
    void startFilling();   // Single GPIO HIGH — pump motor on
    void stopFilling();    // Single GPIO LOW  — pump motor off, tube self-seals

    bool isPumping() const { return pumping_.load(std::memory_order_acquire); }

private:
    ActuatorConfig    cfg_;
    int               gpio_handle_;
    std::atomic<bool> pumping_{ false };
};
