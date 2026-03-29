# HydroPHAI φ 💧⚡

> **HydroPHAI** — *Hydro* (Greek: liquid/water) · *PHAI* (Physical Hardware AI) · *φ* (phi: fluid dynamics flow rate symbol)
>
> **Genuinely real-time adaptive filling system — **

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Raspberry%20Pi%205-red.svg)
![Language](https://img.shields.io/badge/language-C%2B%2B17-blue.svg)
![OS](https://img.shields.io/badge/OS-Linux%20Raspberry%20Pi%20OS-green.svg)
![Name](https://img.shields.io/badge/name-HydroPHAI%20%CF%86-0F6E56.svg)
![Status](https://img.shields.io/badge/status-active-brightgreen.svg)

---

## 📌 Overview

**HydroPHAI** *(φ — phi, fluid dynamics flow rate symbol)* is a fully autonomous adaptive filling system for Raspberry Pi 5. Place any container — bottle, mug, jug, shot glass — and the system detects it, profiles its geometry, and fills it precisely with zero operator input.


---

## 🚀 Getting Started

### Prerequisites

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y cmake g++ liblgpio-dev libi2c-dev i2c-tools
```

### Build

```bash
git clone https://github.com/RTEP5220/Hydro-PHI.git
cd Hydro-PHI
mkdir build && cd build
cmake ..
make -j4
```

### Run

```bash
# Must run as root for SCHED_FIFO, mlockall, and GPIO access
sudo ./Hydro-PHI
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
cd build && ./Hydro-PHI_tests
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
cd build && ./Hydro-PHI_tests
```

---

## 📄 License

[MIT License](LICENSE) — Copyright © 2026 HydroPHAI

---

<p align="center">
**HydroPHAI φ**<br/>⚡ Genuinely Real-Time · 🔒 Lock-Free ·<br/>
HydroPHAI φ · Raspberry Pi 5 · C++17 ·
</p>

---
