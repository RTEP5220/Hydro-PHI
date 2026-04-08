#pragma once
#include "config.hpp"
#include "sensor_types.hpp"
#include "thread_utils.hpp"
#include <atomic>
#include <functional>
#include <lgpio.h>

// ─────────────────────────────────────────────────────────────────────────────
// IRProximity — lgpio GPIO edge alert callback.
//
// Uses lgGpioSetAlertsFunc() to register a callback that fires IMMEDIATELY on GPIO edge — rising or falling.
// This means container placement and removal events are detected in microseconds
// The callback atomically sets container_present—readable from any thread without locking.
// ─────────────────────────────────────────────────────────────────────────────

// Forward declare lgpio callback signature
using IRCallback = std::function<void(bool container_present)>;

class IRProximity {
public:
    explicit IRProximity(const IRConfig& cfg, int gpio_handle);
    ~IRProximity();

    // init() registers the lgpio edge alert callback
    // onEvent fires on every GPIO edge — true = container present
    bool init(IRCallback onEvent);

    // Non-blocking atomic read of latest state
    bool isContainerPresent() const;

    // Manually read GPIO (used at startup to get initial state)
    bool readInitialState() const;

private:
    IRConfig cfg_;
    int      gpio_handle_;

    std::atomic<bool> container_present_{ false };
    IRCallback        user_callback_;

    // lgpio callback — called from lgpio internal thread
    // Must be static — uses userData pointer to reach instance
    static void lgpioAlertCallback(int num_alerts,
                                   lgGpioAlert_p alerts,
                                   void* userdata);
};
