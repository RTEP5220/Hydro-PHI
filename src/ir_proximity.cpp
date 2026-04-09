#include "ir_proximity.hpp"
#include <iostream>
#include <cstring>

IRProximity::IRProximity(const IRConfig& cfg, int gpio_handle)
    : cfg_(cfg), gpio_handle_(gpio_handle) {}

IRProximity::~IRProximity() {
    // Cancel lgpio alerts on this pin
    lgGpioSetAlertsFunc(gpio_handle_, cfg_.gpio_pin, nullptr, nullptr);
    lgGpioFree(gpio_handle_, cfg_.gpio_pin);
}

bool IRProximity::init(IRCallback onEvent) {
    user_callback_ = std::move(onEvent);

    // Claim GPIO as input
    if (lgGpioClaimInput(gpio_handle_, 0, cfg_.gpio_pin) < 0) {
        std::cerr << "[IR] Failed to claim GPIO " << cfg_.gpio_pin << "\n";
        return false;
    }

    // Read initial state before registering callback
    container_present_.store(readInitialState(), std::memory_order_release);

    // ─── Register lgpio edge alert callback ───────────────────────────────
    // lgGpioSetAlertsFunc fires lgpioAlertCallback on BOTH rising and falling
    // edges-immediately when the GPIO changes, from lgpio's internal thread.
    
    int ret = lgGpioSetAlertsFunc(
        gpio_handle_,
        cfg_.gpio_pin,
        &IRProximity::lgpioAlertCallback,
        static_cast<void*>(this)   // userData → reaches our instance
    );

    if (ret < 0) {
        std::cerr << "[IR] Failed to register lgpio alert callback: "
                  << lguErrorText(ret) << "\n";
        return false;
    }

    std::cout << "[IR] Edge alert callback registered on GPIO "
              << cfg_.gpio_pin
              << " (active_low=" << cfg_.active_low << ")\n";
    std::cout << "[IR] Initial state: "
              << (container_present_ ? "CONTAINER PRESENT" : "EMPTY") << "\n";
    return true;
}



// lgGpioAlert_p fields:
//   alerts[i].report.gpio  — which GPIO pin fired
//   alerts[i].report.level — new logic level (0 or 1)
//   alerts[i].report.tick  — timestamp in microseconds
// ─────────────────────────────────────────────────────────────────────────────
void IRProximity::lgpioAlertCallback(int        num_alerts,
                                     lgGpioAlert_p alerts,
                                     void*      userdata) {
    auto* self = static_cast<IRProximity*>(userdata);

    for (int i = 0; i < num_alerts; ++i) {
        int level = alerts[i].report.level;

        // active_low: GPIO=0 means container present
        // active_high: GPIO=1 means container present
        bool present = self->cfg_.active_low ? (level == 0) : (level == 1);

        // Atomic store — no mutex, no blocking
        self->container_present_.store(present, std::memory_order_release);

        // Fire user callback (registered by SensorHub)
        // This is what triggers the emergency stop path
        if (self->user_callback_) {
            self->user_callback_(present);
        }
    }
}

bool IRProximity::isContainerPresent() const {
    return container_present_.load(std::memory_order_acquire);
}

bool IRProximity::readInitialState() const {
    int level = lgGpioRead(gpio_handle_, cfg_.gpio_pin);
    return cfg_.active_low ? (level == 0) : (level == 1);
}
