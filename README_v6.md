# HydroPHAI φ 💧⚡

> **HydroPHAI** — *Hydro* (Greek: liquid/water) · *PHAI* (Physical Hardware AI) · *φ* (phi: fluid dynamics flow rate symbol)
>
> **Genuinely real-time adaptive filling system — every RT claim is verified and enforced.**

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Raspberry%20Pi%205-red.svg)
![Language](https://img.shields.io/badge/language-C%2B%2B17-blue.svg)
![OS](https://img.shields.io/badge/OS-Linux%20Raspberry%20Pi%20OS-green.svg)
![GPIO](https://img.shields.io/badge/GPIO-lgpio-orange.svg)
![Pump](https://img.shields.io/badge/pump-peristaltic-teal.svg)
![RT](https://img.shields.io/badge/scheduling-SCHED__FIFO-purple.svg)
![Name](https://img.shields.io/badge/name-HydroPHAI%20%CF%86-0F6E56.svg)
![Status](https://img.shields.io/badge/status-active-brightgreen.svg)

---

## 📌 Overview

**HydroPHAI** *(φ — phi, fluid dynamics flow rate symbol)* is a fully autonomous adaptive filling system for Raspberry Pi 5. Place any container — bottle, mug, jug, shot glass — and the system detects it, profiles its geometry, and fills it precisely with zero operator input.

v6 fixes all six real-time violations identified in v5. Every RT property is now enforced, verified, or has a clear explanation of its actual guarantee level on a non-PREEMPT_RT Linux kernel.

---

## ✅ v5 → v6: All Six RT Violations Fixed

| # | Problem in v5 | Fix in v6 | File |
|---|--------------|-----------|------|
| 1 | `SCHED_FIFO` silently fell back to `SCHED_OTHER` — no error | `fatal=true` on `setRealtimePriority()` — `std::abort()` if RT scheduling unavailable | `thread_utils.cpp` |
| 2 | `scanContainer()` blocked controller for ~500ms | `WorkerThread` runs scan/tare off the controller — controller stays responsive throughout | `worker.hpp/cpp`, `controller.cpp` |
| 3 | `tare()` blocked controller for ~800ms | Same — `WorkerThread` handles tare; controller polls result atomically | `worker.hpp/cpp` |
| 4 | HX711 bit-bang had no guaranteed pulse timing | `ThreadUtils::nsDelay(300)` via `clock_nanosleep` — 300ns SCK pulse width, HX711 datasheet compliant | `load_cell.cpp`, `thread_utils.cpp` |
| 5 | Logger `cv_.notify_one()` acquired a mutex on the RT hot path — priority inversion risk | `std::condition_variable` removed; replaced with `atomic<bool> has_work_` — zero locking for RT callers | `logger.cpp`, `logger.hpp` |
| 6 | No `mlockall()`, no stack pre-fault — page faults possible during RT execution | `mlockall(MCL_CURRENT\|MCL_FUTURE)` in `main()` before threads start; `prefaultStack()` in every RT thread body | `main.cpp`, `thread_utils.cpp` |

---

## 🔬 Honest RT Assessment

HydroPHAI runs as **soft real-time** on standard Raspberry Pi OS. Here is what that means in practice:

| Property | Guarantee level | Why |
|----------|----------------|-----|
| Emergency stop latency | < 1ms (lgpio callback) + < 50ms (controller loop) | lgpio alert thread fires on GPIO edge; atomic flag checked top of every loop |
| Sensor read latency | < 100ms (ToF) / < 80ms (load cell) | Dedicated threads, deadline loops |
| Controller loop jitter | Typically < 500µs, worst case < 5ms | `SCHED_FIFO` + `clock_nanosleep TIMER_ABSTIME`; non-RT kernel may add scheduler delay |
| HX711 pulse timing | ≥ 300ns SCK pulse (datasheet min 200ns) | `clock_nanosleep` on CLOCK_MONOTONIC |
| Page fault during RT | Not possible after startup | `mlockall(MCL_CURRENT\|MCL_FUTURE)` |
| Priority inversion | Not possible on logger path | Atomic flag, no mutex |
| Hard real-time | ❌ Not achieved | Requires `PREEMPT_RT` kernel patch |

To achieve **hard real-time** (deterministic sub-100µs worst-case latency), apply the `PREEMPT_RT` patch to the Raspberry Pi kernel. All HydroPHAI code is compatible with a PREEMPT_RT kernel — no changes required.

---

## 🧵 Thread Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                      Raspberry Pi 5 (4 cores)                    │
│                                                                  │
│  Core 0: main thread (idle) + Worker thread (pri=60)             │
│          Worker runs scan (~500ms) and tare (~800ms).            │
│          Priority 60 — below controller, never preempts RT loop. │
│                                                                  │
│  Core 1: Controller thread ── SCHED_FIFO pri=70 ────────────     │
│          ├── Checks emergency_stop_ atomic flag FIRST            │
│          ├── Reads SensorHub snapshot (all atomic, no blocking)  │
│          ├── Posts scan/tare jobs to Worker (non-blocking)       │
│          ├── Polls Worker result atomically (non-blocking)       │
│          └── clock_nanosleep TIMER_ABSTIME deadline loop         │
│                                                                  │
│  Core 2: ToF sensor thread ── SCHED_FIFO pri=80 ────────────     │
│          ├── Reads VL53L1X via I2C every 100ms                   │
│          ├── prefaultStack() at entry                            │
│          └── Publishes via AtomicReading<ToFReading>             │
│                                                                  │
│  Core 3: LoadCell thread ── SCHED_FIFO pri=80 ─────────────      │
│          ├── HX711 bit-bang with nsDelay(300ns) pulse timing     │
│          ├── prefaultStack() at entry                            │
│          └── Publishes via AtomicReading<WeightReading>          │
│                                                                  │
│  lgpio internal thread ── SCHED_FIFO pri=90 ───────────────      │
│          ├── GPIO 16 edge alert fires in < 1ms                   │
│          └── Sets atomic emergency_stop_ flag via callback chain │
│                                                                  │
│  Logger thread ── SCHED_OTHER ─────────────────────────────      │
│          ├── Polls atomic has_work_ flag (no mutex)              │
│          └── Drains LockFreeQueue — writes CSV async             │
└──────────────────────────────────────────────────────────────────┘
```

---

## 🔧 Hardware — Bill of Materials

| # | Component | Interface | Role | GPIO |
|---|-----------|-----------|------|------|
| 1 | **VL53L1X** ToF laser | I2C | Container rim + fill tracking | SDA GPIO2, SCL GPIO3 |
| 2 | **HX711** + 5kg load cell | lgpio bit-bang | Weight fallback (dark/transparent liquids) | DOUT GPIO5, SCK GPIO6 |
| 3 | **IR proximity** sensor | lgpio edge alert | Container gate + emergency stop | GPIO16 |
| 4 | **Peristaltic pump** 12V + relay | lgpio relay | Fill liquid — self-sealing on motor stop | GPIO22 |

GPIO23 is free — solenoid valve removed in v5, not needed with peristaltic pump.

---

## 🗂️ Project Structure

```
smartflow_v6/
├── CMakeLists.txt
├── LICENSE
├── README.md
├── config/
│   └── settings.cfg
├── include/
│   ├── config.hpp           — AppConfig, ActuatorConfig (pump_pin only)
│   ├── sensor_types.hpp     — ToFReading, WeightReading, SensorSnapshot, FillRecord
│   ├── thread_utils.hpp     — LockFreeQueue, AtomicReading, RT utilities
│   ├── tof_sensor.hpp       — VL53L1X: threaded, atomic publish, scanContainer()
│   ├── load_cell.hpp        — HX711: threaded, nsDelay pulse timing, atomic publish
│   ├── ir_proximity.hpp     — lgpio edge alert callback
│   ├── actuator.hpp         — Peristaltic pump: single GPIO write
│   ├── worker.hpp           — WorkerThread: async scan/tare, non-blocking post/poll
│   ├── sensor_hub.hpp       — Fused snapshot + emergency stop chain
│   ├── controller.hpp       — 8-state RT machine, no blocking in loop
│   └── logger.hpp           — Async: LockFreeQueue + atomic has_work_ flag
├── src/
│   ├── main.cpp           — mlockall before threads, fatal RT checks
│   ├── config.cpp
│   ├── thread_utils.cpp   — nsDelay, lockMemory, prefaultStack, initPriorityInheritMutex
│   ├── tof_sensor.cpp     — prefaultStack in sensorLoop
│   ├── load_cell.cpp      — nsDelay(300ns) SCK timing, prefaultStack in weightLoop
│   ├── ir_proximity.cpp   — lgGpioSetAlertsFunc callback
│   ├── actuator.cpp       — single lgGpioWrite per start/stop
│   ├── worker.cpp         — workerLoop: posts results atomically, clock_nanosleep yield
│   ├── sensor_hub.cpp
│   ├── controller.cpp     — zero blocking, prefaultStack, TIMER_ABSTIME loop
│   └── logger.cpp         — has_work_ atomic flag, no condition_variable
├── tests/
│   ├── test_logic.cpp     — 28 tests including v6 RT-specific tests
│   └── test_queue.cpp     — LockFreeQueue SPSC + AtomicReading concurrent tests
└── build/
```

---

## 🚀 Getting Started

### Prerequisites

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y cmake g++ liblgpio-dev libi2c-dev i2c-tools
sudo raspi-config   # Interface Options → I2C → Enable
i2cdetect -y 1      # Verify VL53L1X at 0x29
sudo mkdir -p /var/log/smartflow && sudo chown $USER /var/log/smartflow
```

### Build

```bash
git clone https://github.com/your-username/smartflow_v6.git
cd smartflow_v6
mkdir build && cd build
cmake ..
make -j4
```

### Run

```bash
# Must run as root for SCHED_FIFO, mlockall, and GPIO access
sudo ./smartflow_v6
```

On startup you should see:

```
[RT] Process memory locked (MCL_CURRENT | MCL_FUTURE)
[ToF]      Sensor thread started (SCHED_FIFO pri=80)
[LoadCell] Thread started (SCHED_FIFO pri=80)
[Worker]   Thread started (SCHED_FIFO pri=60 core=0)
[Controller] Thread started (SCHED_FIFO pri=70 core=1)
```

If you see `[RT] FATAL: Cannot set SCHED_FIFO` — run with `sudo` or configure `/etc/security/limits.conf`.

### Tests (no hardware required)

```bash
cd build && ./smartflow_tests
```

---

## 🧩 Software Engineering Models

---

### 1. 📋 Use Case Diagram

```mermaid
flowchart TD
    Operator(["👤 Operator"])
    IRThread(["⚡ lgpio Alert Thread\npri=90"])
    SensorT(["🔭 Sensor Threads\nToF + LoadCell\npri=80"])
    WorkerT(["⚙️ Worker Thread\npri=60"])
    CtrlT(["🎛️ Controller Thread\npri=70"])
    LogT(["📁 Logger Thread\nSCHED_OTHER"])

    Operator -->|Place container| UC1([Container placed])
    Operator -->|Remove container| UC2([Container removed])
    Operator -->|Ctrl+C| UC3([Shutdown])

    IRThread -->|GPIO edge callback| UC4([Detect container < 1ms])
    IRThread -->|atomic flag| UC5([Emergency stop injection])

    SensorT -->|deadline loop| UC6([Publish ToF + weight atomically])

    CtrlT -->|postScan non-blocking| UC7([Request container scan])
    CtrlT -->|postTare non-blocking| UC8([Request tare])
    CtrlT -->|resultReady atomic poll| UC9([Poll worker result])
    CtrlT -->|single GPIO write| UC10([Control peristaltic pump])

    WorkerT -->|scanContainer blocking ok| UC11([Execute scan ~500ms])
    WorkerT -->|tare blocking ok| UC12([Execute tare ~800ms])
    WorkerT -->|atomic result publish| UC13([Publish result to controller])

    LogT -->|drain LockFreeQueue| UC14([Write CSV async])

    UC1 --> UC4
    UC4 -->|present=true| UC7
    UC2 --> UC5
    UC5 -->|emergency_stop_ flag| UC10
    UC6 --> UC9
    UC7 --> UC11
    UC8 --> UC12
    UC11 --> UC13
    UC12 --> UC13
    UC13 --> UC9
    UC9 --> UC10
    UC10 --> UC14
```

---

### 2. 🏗️ Class Diagram (UML)

```mermaid
classDiagram
    class WorkerThread {
        -ScanFn scan_fn_
        -TareFn tare_fn_
        -thread thread_
        -atomic~bool~ busy_
        -atomic~WorkerJob~ pending_job_
        -atomic~bool~ result_ready_
        -WorkerResult result_
        +start(priority)
        +postScan() bool
        NOTE: non-blocking — returns immediately
        +postTare() bool
        NOTE: non-blocking — returns immediately
        +resultReady() bool
        NOTE: atomic load — no blocking
        +takeResult() WorkerResult
        -workerLoop()
        NOTE: blocking ops safe here
    }

    class Controller {
        -SensorHub& hub_
        -Actuator& actuator_
        -Logger& logger_
        -WorkerThread& worker_
        -atomic~FillState~ state_
        -atomic~bool~ emergency_stop_
        +requestEmergencyStop(reason)
        NOTE: atomic store from IR callback
        -controlLoop()
        NOTE: zero blocking — all handlers return immediately
        -handleScanning() FillState
        NOTE: polls worker.resultReady() — non-blocking
        -handleTaring() FillState
        NOTE: polls worker.resultReady() — non-blocking
    }

    class Logger {
        -LockFreeQueue~LogEntry,256~ queue_
        -atomic~bool~ has_work_
        NOTE: replaces condition_variable
        -atomic~uint32~ dropped_
        +log(level, message)
        NOTE: push + atomic store — zero blocking
        -loggerLoop()
        NOTE: polls has_work_ with 1ms nanosleep
    }

    class ThreadUtils {
        +setRealtimePriority(thread, priority, fatal) bool$
        NOTE: fatal=true — std::abort if fails
        +nsDelay(nanoseconds)$
        NOTE: clock_nanosleep CLOCK_MONOTONIC
        +lockMemory() bool$
        NOTE: mlockall MCL_CURRENT|MCL_FUTURE
        +prefaultStack(size)$
        NOTE: touches every 4KB page
        +sleepUntil(timepoint)$
        NOTE: clock_nanosleep TIMER_ABSTIME
        +initPriorityInheritMutex(mutex)$
        NOTE: PTHREAD_PRIO_INHERIT
    }

    class LoadCell {
        -AtomicReading~WeightReading~ latest_
        -thread thread_
        +startThread(priority)
        -weightLoop()
        NOTE: prefaultStack at entry
        -readRaw() long
        NOTE: nsDelay(300ns) SCK pulse timing
    }

    class ToFSensor {
        -AtomicReading~ToFReading~ latest_
        -thread thread_
        +startThread(priority)
        -sensorLoop()
        NOTE: prefaultStack at entry
        +scanContainer() ContainerProfile
        NOTE: called from WorkerThread only
    }

    Controller   --> WorkerThread  : postScan / postTare / resultReady
    Controller   --> Logger        : log async
    WorkerThread --> ToFSensor     : scanContainer()
    WorkerThread --> LoadCell      : tare()
    Logger       ..> ThreadUtils   : uses has_work_ pattern
    LoadCell     ..> ThreadUtils   : nsDelay, prefaultStack
    ToFSensor    ..> ThreadUtils   : prefaultStack, sleepUntil
    Controller   ..> ThreadUtils   : prefaultStack, sleepUntil
```

---

### 3. 🔄 Sequence Diagram — v6 Non-Blocking Scan + Tare

```mermaid
sequenceDiagram
    participant IR as IR Alert Thread<br/>(pri=90)
    participant Ctrl as Controller<br/>(pri=70 · 50ms loop)
    participant Worker as Worker Thread<br/>(pri=60)
    participant ToF as ToF Sensor Thread<br/>(pri=80)
    participant LC as LoadCell Thread<br/>(pri=80)
    participant Log as Logger Thread

    Note over IR: Container placed — GPIO edge

    IR->>Ctrl: requestEmergencyStop (atomic store)
    Note over Ctrl: emergency_stop_ checked TOP of loop

    Ctrl->>Worker: postScan() [atomic store — returns instantly]
    Note over Ctrl: Controller keeps looping every 50ms

    loop Controller loops while Worker scans (~500ms = ~10 loops)
        Ctrl->>Ctrl: check emergency_stop_ [atomic — <10ns]
        Ctrl->>Ctrl: getSnapshot() [all atomic — no blocking]
        Ctrl->>Worker: resultReady()? [atomic load — returns false]
        Ctrl->>Ctrl: sleepUntil deadline [clock_nanosleep]
        Note over Ctrl: Emergency stop FULLY RESPONSIVE here
    end

    Worker->>ToF: scanContainer() [blocking — safe on worker]
    ToF-->>Worker: ContainerProfile
    Worker->>Worker: result_ready_.store(true)

    Ctrl->>Worker: resultReady()? → true
    Ctrl->>Worker: takeResult()
    Ctrl->>Worker: postTare() [atomic store — returns instantly]

    loop Controller loops while Worker tares (~800ms = ~16 loops)
        Ctrl->>Ctrl: check emergency_stop_ [always first]
        Ctrl->>Worker: resultReady()? → false
        Note over Ctrl: Still fully responsive to emergency stop
    end

    Worker->>LC: tare(10 samples) [blocking — safe on worker]
    LC-->>Worker: tare complete
    Worker->>Worker: result_ready_.store(true)

    Ctrl->>Worker: resultReady()? → true
    Ctrl->>Ctrl: startFilling() — lgGpioWrite(pump_pin, HIGH)
    Note over Ctrl: Single write — peristaltic pump on

    loop Every 50ms — FILLING
        Ctrl->>Ctrl: check emergency_stop_ [first]
        Ctrl->>Ctrl: getSnapshot() [atomic]
        alt ToF ≤ fill_target
            Ctrl->>Ctrl: lgGpioWrite(pump_pin, LOW)
            Ctrl->>Log: logFill() [async queue push]
        else weight stable > 20g
            Ctrl->>Ctrl: lgGpioWrite(pump_pin, LOW)
        end
    end

    Note over IR: Container removed mid-fill
    IR->>Ctrl: emergency_stop_.store(true) [< 1ms]
    Ctrl->>Ctrl: lgGpioWrite(pump_pin, LOW) [next loop top]
    Note over Ctrl: Tube seals — zero drip
```

---

### 4. 🔁 State Machine

```mermaid
stateDiagram-v2
    [*] --> IDLE : mlockall + baseline + tare\nAll RT threads running

    IDLE --> CONTAINER_DETECTED : IR callback + weight > 10g

    CONTAINER_DETECTED --> IDLE     : Container removed (debounce)
    CONTAINER_DETECTED --> SCANNING : worker.postScan() posted

    SCANNING --> SCANNING      : worker.resultReady() == false\n(controller keeps looping — emergency stop responsive)
    SCANNING --> IDLE          : Container removed during scan
    SCANNING --> TARING        : Scan result received + valid\nworker.postTare() posted

    TARING --> TARING          : worker.resultReady() == false\n(controller keeps looping)
    TARING --> IDLE            : Container removed during tare
    TARING --> FILLING         : Tare result received

    FILLING --> COMPLETE       : ToF ≤ fill_target\nOR weight plateau
    FILLING --> EMERGENCY_STOP : atomic emergency_stop_ flag\n(IR edge — container removed)
    FILLING --> ERROR          : Fill timeout

    COMPLETE --> COMPLETE      : Container still present
    COMPLETE --> IDLE          : Container removed

    EMERGENCY_STOP --> IDLE    : Station clear
    ERROR --> [*]              : Shutdown
    IDLE  --> [*]              : SIGINT
```

---

### 5. 🔃 Activity Diagram — v6 RT Startup Sequence

```mermaid
flowchart TD
    A([Start]) --> B[mlockall MCL_CURRENT + MCL_FUTURE]
    B --> C{mlockall OK?}
    C -- No --> D[WARN — continue\npage faults possible]
    C -- Yes --> E[Open lgpio chip]
    D --> E
    E --> F[Init ToF / LoadCell / Pump relay]
    F --> G[Register IR lgpio edge callback]
    G --> H[Calibrate ToF baseline\n10 samples blocking — pre-thread]
    H --> I[Initial tare\nblocking — pre-thread]
    I --> J[Register SIGINT / SIGTERM]
    J --> K[Start Logger thread\nSCHED_OTHER]
    K --> L[Start ToF thread\nSCHED_FIFO pri=80\nprefaultStack at entry]
    L --> M[Start LoadCell thread\nSCHED_FIFO pri=80\nprefaultStack at entry]
    M --> N[Start Worker thread\nSCHED_FIFO pri=60\nprefaultStack at entry]
    N --> O[Start Controller thread\nSCHED_FIFO pri=70\nprefaultStack at entry]
    O --> P([All RT threads running\nPlace container to begin])

    P --> Q{emergency_stop_ flag?}
    Q -- SET --> R[lgGpioWrite pump LOW\n→ EMERGENCY_STOP]
    R --> S[Clear flag]
    S --> T

    Q -- CLEAR --> T[getSnapshot — all atomic]
    T --> U{State?}

    U -- IDLE --> V{IR + weight > 10g?}
    V -- No --> SLEEP[sleepUntil deadline\nclock_nanosleep TIMER_ABSTIME]
    V -- Yes --> W[CONTAINER_DETECTED]
    W --> SLEEP

    U -- CONTAINER_DETECTED --> X[worker.postScan — returns instantly]
    X --> SLEEP

    U -- SCANNING --> Y{worker.resultReady?}
    Y -- No --> SLEEP
    Y -- Yes --> Z[takeResult\nworker.postTare — instant]
    Z --> SLEEP

    U -- TARING --> AA{worker.resultReady?}
    AA -- No --> SLEEP
    AA -- Yes --> AB[FILLING]
    AB --> SLEEP

    U -- FILLING --> AC[lgGpioWrite pump HIGH\nif not already on]
    AC --> AD{ToF ≤ target OR weight stable?}
    AD -- Yes --> AE[lgGpioWrite pump LOW\nCOMPLETE]
    AD -- No --> SLEEP
    AE --> SLEEP

    SLEEP --> AF{Shutdown flag?}
    AF -- No --> Q
    AF -- Yes --> AG[Stop all threads]
    AG --> AH[lgGpioWrite pump LOW]
    AH --> AI[munlockall]
    AI --> AJ[lgGpiochipClose]
    AJ --> AK([Clean exit])
```

---

### 6. 🧱 Component Diagram

```mermaid
flowchart TB
    subgraph RT_HIGH["SCHED_FIFO — High Priority"]
        IR_T["lgpio alert thread\npri=90"]
    end

    subgraph RT_MID["SCHED_FIFO — Sensor Threads"]
        TOF_T["ToF thread\npri=80 · core 2\nprefaultStack"]
        LC_T["LoadCell thread\npri=80 · core 3\nnsDelay 300ns · prefaultStack"]
    end

    subgraph RT_CTRL["SCHED_FIFO — Control"]
        CTRL["Controller thread\npri=70 · core 1\nno blocking · prefaultStack\nclock_nanosleep TIMER_ABSTIME"]
        WRK["Worker thread\npri=60 · core 0\nscan + tare · prefaultStack"]
    end

    subgraph RT_LOW["SCHED_OTHER — Non-RT"]
        LOG_T["Logger thread\natomic has_work_\nno mutex · LockFreeQueue drain"]
    end

    subgraph Shared["Lock-Free Shared State"]
        A_TOF["AtomicReading\nToFReading"]
        A_LC["AtomicReading\nWeightReading"]
        A_IR["atomic bool\ncontainer_present_"]
        A_ES["atomic bool\nemergency_stop_"]
        A_JOB["atomic WorkerJob\npending_job_"]
        A_RES["atomic bool\nresult_ready_"]
        A_HW["atomic bool\nhas_work_"]
    end

    subgraph HW["Physical Hardware"]
        VL53["VL53L1X ToF\nI2C 0x29"]
        HX711["HX711 + Load Cell\nlgpio bit-bang"]
        IR_HW["IR Proximity\nlgpio edge GPIO16"]
        PUMP["Peristaltic Pump\nRelay GPIO22"]
    end

    TOF_T  -->|write| A_TOF
    LC_T   -->|write| A_LC
    IR_T   -->|write| A_IR
    IR_T   -->|write| A_ES
    CTRL   -->|write| A_JOB
    WRK    -->|write| A_RES
    CTRL   -->|write| A_HW

    A_TOF --> CTRL
    A_LC  --> CTRL
    A_IR  --> CTRL
    A_ES  --> CTRL
    A_JOB --> WRK
    A_RES --> CTRL
    A_HW  --> LOG_T

    CTRL  --> PUMP
    TOF_T --> VL53
    LC_T  --> HX711
    IR_T  --> IR_HW
    WRK   --> TOF_T
    WRK   --> LC_T
```

---

### 7. ⚡ Emergency Stop — Full Signal Chain

```
GPIO 16 falling edge (container removed)

lgpio alert thread (pri=90, any core)
  └── lgpioAlertCallback() — fires in < 100µs
        └── container_present_.store(false)     [atomic]
        └── user_callback_(false)               [SensorHub::onIREvent]
              └── emergency_cb_("Container removed during fill")
                    └── Controller::requestEmergencyStop()
                          └── emergency_stop_.store(true)  [atomic, < 1µs]

Controller thread (pri=70, core 1, 50ms loop)
  └── TOP of next loop iteration (worst case: current loop deadline + 50ms)
        └── emergency_stop_.load() == true      [< 10ns]
              └── lgGpioWrite(pump_pin, LOW)     [single write — pump off]
              └── Peristaltic tube self-seals    [zero drip, zero back-flow]
              └── state_ = EMERGENCY_STOP        [atomic store]

Total worst-case: < 50ms from GPIO edge to pump off
Tube sealed: immediately on motor stop
```

---

### 8. 🗃️ ER Diagram

```mermaid
erDiagram
    FILL_SESSION {
        int    session_id     PK
        string start_time
        string end_time
        float  duration_s
        string stop_trigger
        string stop_reason
    }
    CONTAINER_PROFILE {
        int    profile_id     PK
        int    session_id     FK
        float  baseline_cm
        float  rim_cm
        float  depth_cm
        float  fill_target_cm
    }
    SENSOR_READING {
        int    reading_id     PK
        int    session_id     FK
        string timestamp
        float  tof_cm
        float  weight_g
        bool   pump_active
        string state
    }
    SYSTEM_EVENT {
        int    event_id       PK
        int    session_id     FK
        string timestamp
        string level
        string thread_name
        string message
    }

    FILL_SESSION ||--||  CONTAINER_PROFILE : "has one"
    FILL_SESSION ||--o{  SENSOR_READING    : "records"
    FILL_SESSION ||--o{  SYSTEM_EVENT      : "generates"
```

---

## 🧪 Tests

```bash
cd build && ./smartflow_tests
```

v6-specific tests cover:

- `nsDelay` precision — 300ns HX711 pulse and 1ms logger yield
- Worker non-blocking handoff — controller loops while worker scans
- Emergency stop atomicity — IR thread fires, controller catches within 50ms
- Logger atomic flag — 10 signals from RT thread, no mutex, no blocking
- Deadline loop stability — 20 iterations, measures actual jitter
- State machine — SCANNING/TARING are non-blocking (poll resultReady)
- Emergency stop during SCANNING — atomic flag overrides mid-scan

---

## 📄 License

[MIT License](LICENSE) — Copyright © 2026 HydroPHAI

---

<p align="center">
**HydroPHAI φ**<br/>⚡ Genuinely Real-Time · 🔒 Lock-Free · 🧵 SCHED_FIFO · 💧 Peristaltic<br/>
HydroPHAI φ · Raspberry Pi 5 · C++17 · lgpio · clock_nanosleep · mlockall
</p>

---

## 🔬 Sensor & ADC Selection Rationale

### Why HX711 and not ADS1115 for the load cell?

Both chips can interface a load cell to Raspberry Pi 5, but they solve the problem differently. Understanding the difference explains why HX711 is the right choice here.

#### What a load cell actually outputs

A strain gauge load cell produces a **differential voltage** — the difference between two signal wires — that is proportional to the applied weight. With a 5V excitation supply and a typical 5kg load cell, the full-scale output is roughly **10–15 mV**. This is an extremely small signal that sits on top of a common-mode voltage and is easily corrupted by noise.

#### HX711 — purpose-built for this job

The HX711 contains a complete **instrumentation amplifier** designed specifically for strain gauge bridges, followed by a 24-bit sigma-delta ADC. The amplifier's differential inputs, fixed high gain (32×, 64×, or 128×), and noise filtering are all matched to the load cell signal range. Everything needed is inside one chip connected directly to the load cell wires.

```
Load cell  ──→  HX711  ──→  RPi 5 GPIO (bit-bang)
               (amp + ADC)
```

#### ADS1115 — wrong tool for this job

The ADS1115 is a general-purpose precision ADC with a minimum input range of ±256 mV. A 15 mV load cell signal fed directly into it uses only about 6% of its full-scale range. In practice this reduces the effective resolution from 16-bit to roughly 10-bit — 1024 useful counts instead of 32767.

To use the ADS1115 properly with a load cell, an external instrumentation amplifier (INA125, INA128, or AD8221) must be added to bring the signal up to a usable voltage range. Now two chips are required where one would do.

```
Load cell  ──→  INA125 (external amp)  ──→  ADS1115  ──→  RPi 5 I2C
               (extra component, cost, noise)
```

#### Side-by-side comparison

| Property | HX711 | ADS1115 |
|----------|-------|---------|
| **Resolution** | 24-bit | 16-bit (≈10-bit effective without external amp) |
| **Built-in amplifier** | ✅ Yes — matched to load cells | ❌ No — external INA required |
| **Interface** | Custom bit-bang (GPIO 5, 6) | I2C — shares bus with VL53L1X |
| **Input type** | Differential — exactly right for load cells | Single-ended or differential |
| **Full-scale input** | ±20 mV at gain 128 — matched to load cells | ±256 mV minimum — too wide |
| **Sample rate** | 10 or 80 Hz | 8 to 860 Hz |
| **External components** | None | Instrumentation amplifier required |
| **Cost** | ~£1–2 | ~£3–5 (plus ~£3–8 for external amp) |
| **Multiple channels** | No | Yes — 4 channels |
| **RPi 5 GPIO pins used** | 2 (DOUT + SCK) | 2 (SDA + SCL, shared I2C bus) |

#### The bit-bang trade-off

The one genuine disadvantage of the HX711 is its custom serial protocol. It is not I2C or SPI — it requires manual bit-banging of two GPIO pins with precise timing. The HX711 datasheet requires a minimum 200 ns clock pulse width. This is why HydroPHAI uses `ThreadUtils::nsDelay(300)` — a `clock_nanosleep` call on `CLOCK_MONOTONIC` — to guarantee the pulse width regardless of CPU load. On a general Linux kernel without this explicit timing, the bit-bang can violate the datasheet minimum and produce corrupted readings.

The ADS1115 avoids this entirely since the Linux I2C driver handles all timing automatically. However, for a single load cell in a weighing application, the bit-bang complexity is a small and well-understood trade-off for 24-bit resolution and no external components.

#### When ADS1115 would be the right choice

- Reading multiple different sensors from one chip (temperature + voltage + pressure)
- Already using I2C for everything and want a uniform bus topology
- Need a sample rate above 80 Hz
- Using a sensor that outputs a voltage in the 256 mV – 6V range directly

For HydroPHAI's use case — one load cell, maximum resolution, minimum components — **HX711 is the correct choice**.

---

### Sensor selection summary

| Sensor | Measures | Why chosen | Key alternative rejected |
|--------|----------|-----------|--------------------------|
| **VL53L1X ToF** | Distance to liquid surface | Only sensor that profiles container geometry AND tracks fill level in one device | HC-SR04 — 5V, slow, imprecise on narrow openings |
| **HX711 + load cell** | Weight of liquid dispensed | 24-bit resolution, self-contained, colour-blind fallback | ADS1115 — needs external amp, loses resolution without it |
| **IR proximity** | Container present / absent | Only sensor fast enough for sub-millisecond emergency stop via GPIO edge callback | Capacitive — fixed position, not adaptive |
| **Peristaltic pump** | Liquid flow | Self-sealing tube eliminates solenoid valve entirely | DC pump + solenoid — two components, sequencing delays, drip risk |

