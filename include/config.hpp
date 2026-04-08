#pragma once
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Configuration structs — loaded from settings.cfg at startup

// ─────────────────────────────────────────────────────────────────────────────

struct TofConfig {
    int    i2c_bus            = 1;
    int    i2c_address        = 0x29;
    float  baseline_cm        = 30.0f;
    float  rim_detect_delta   = 3.0f;
    float  safety_margin_cm   = 1.5f;
    int    poll_interval_ms   = 100;
};

struct LoadCellConfig {
    int    data_pin            = 5;
    int    clock_pin           = 6;
    float  calibration_factor  = 420.0f;
    int    poll_interval_ms    = 80;
    float  weight_delta_g      = 1.0f;
    int    stable_cycles       = 6;
    float  present_threshold_g = 10.0f;
};

struct IRConfig {
    int    gpio_pin   = 16;
    bool   active_low = true;
};

struct ActuatorConfig {
    // Peristaltic pump.
    int    pump_pin = 22;
 
};

struct ControllerConfig {
    int    fill_timeout_s = 60;
    int    idle_scan_ms   = 50;
};

struct ThreadConfig {
    int    sensor_priority     = 80;
    int    safety_priority     = 90;
    int    controller_priority = 70;
    int    logger_priority     = 0;
};

struct LogConfig {
    bool        enable     = true;
    std::string log_file   = "/var/log/HydroPHI/fill_log.csv"; // Log file Container Volume / TimeStamp
    std::string log_level  = "INFO";
    int         queue_size = 256;
};

struct AppConfig {
    TofConfig        tof;
    LoadCellConfig   load_cell;
    IRConfig         ir;
    ActuatorConfig   actuator;
    ControllerConfig controller;
    ThreadConfig     threads;
    LogConfig        logging;
};

class Config {
public:
    static AppConfig load(const std::string& path);
};
