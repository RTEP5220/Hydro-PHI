# HydroPHI φ 💧

> **HydroPHI** — *Hydro* (Greek: liquid/water) · *φ* (phi: fluid dynamics flow rate symbol)
>
> **Genuinely real-time adaptive filling system — **

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Raspberry%20Pi%205-red.svg)
![Language](https://img.shields.io/badge/language-C%2B%2B17-blue.svg)
![OS](https://img.shields.io/badge/OS-Linux%20Raspberry%20Pi%20OS-green.svg)
![Name](https://img.shields.io/badge/name-HydroPHI%20%CF%86-0F6E56.svg)
![Status](https://img.shields.io/badge/status-active-brightgreen.svg)


## Overview

**HydroPHI** *(φ — phi, fluid dynamics flow rate symbol)* is a fully autonomous adaptive filling system for Raspberry Pi 5. Place any container — bottle, mug, jug, shot glass — and the system detects it, profiles its geometry, and fills it precisely with zero operator input.


## 📋 Agenda

- [Bill of Materials](#-bill-of-materials)
- [Getting Started](#-getting-started)
- [Tests](#-tests)
- [License](#-license)
- [Connect with Us](#-connect-with-us)

## 🧾 Bill of Materials

| Component | Qty |
|---|---|
| Raspberry Pi 5 | 1 |
| VL53L1X ToF Laser Sensor module | 1 |
| VL53L1X Breakout Board (VL53L1X-SATEL) | 1 |
| HX711 Load Cell ADC breakout module | 1 |
| 5kg Load Cell (strain gauge, 4-wire) | 1 |
| IR Proximity Sensor (digital output) | 1 |
| Peristaltic Pump 12V DC | 1 |
| 5V Relay Module (single channel) | 1 |
| Silicone Tubing 6mm ID × 1m | 1 |
| Dispensing Nozzle 6mm | 1 |

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
# Recommended GPIO Config Coming Later.
sudo ./Hydro-PHI
```
---

## 🧪 Tests

```bash
cd build && ./Hydro-PHI_tests
```
---

## 📄 License

[MIT License](LICENSE) — Copyright © 2026 HydroPHI


## 📱 Connect with Us

<p align="left">
<a href="https://www.instagram.com/hydro_phi" target="blank"><img align="center" src="https://raw.githubusercontent.com/rahuldkjain/github-profile-readme-generator/master/src/images/icons/Social/instagram.svg" alt="instagram" height="30" width="40" />Follow our Instgram Page ! </a></br>

<a href="https://www.youtube.com/@hydrophi" target="blank"><img align="center" src="https://raw.githubusercontent.com/rahuldkjain/github-profile-readme-generator/master/src/images/icons/Social/youtube.svg" alt="youtube" height="30" width="40" />Subscribe to our Youtube Channel.</a>
</p>


<p align="left">
<img src="images/HydroPHI.png" width="200" align="right" alt="HYDRO-PHI Project"> </br>
</p>


<p align="center">
**HydroPHI φ**<br/>⚡ Genuinely Real-Time · 🔒 Lock-Free · Raspberry Pi 5 · C++17<br/>
HydroPHI φ · Team_3 Real-Time Embedded Programming . University of Glasgow
</p>
