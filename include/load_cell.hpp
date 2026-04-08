#pragma once
#include "config.hpp"
#include "sensor_types.hpp"
#include "thread_utils.hpp"
#include <atomic>
#include <thread>

// ─────────────────────────────────────────────────────────────────────────────
// LoadCell — HX711 24-bit ADC via lgpio bit-bang, dedicated thread
// ─────────────────────────────────────────────────────────────────────────────
class LoadCell {
public:
    explicit LoadCell(const LoadCellConfig& cfg, int gpio_handle);
    ~LoadCell();

    bool init();
    bool tare(int samples = 10);         // Blocking — call before startThread()
    void startThread(int rt_priority);   // Launch dedicated load cell thread
    void stopThread();

    // Non-blocking — returns atomically published latest reading
    WeightReading getLatest() const;

    bool isContainerPresent() const;     // Reads latest atomically

private:
    LoadCellConfig  cfg_;
    int             gpio_handle_;
    long            tare_offset_    = 0;
    float           cal_factor_;

    AtomicReading<WeightReading> latest_;

    std::thread       thread_;
    std::atomic<bool> running_{ false };

    // Stability tracking (inside thread — no shared state needed)
    float last_weight_  = 0.0f;
    int   stable_count_ = 0;

    void   weightLoop();             // Thread body — fixed cadence polling
    long   readRaw();                // Blocking HX711 bit-bang — thread only
    long   averageSamples(int n);
    bool   isHX711Ready() const;
    bool   isStable(float current);
};
