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
### Tests (no hardware required)

```bash
cd build && ./Hydro-PHI_tests
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
