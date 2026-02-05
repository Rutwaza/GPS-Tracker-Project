# 📡 ESP32 + A7670C 4G GPS Tracker with Firebase

## 🎯 Project Overview

A real-time GPS tracking device that uses an ESP32 microcontroller, A7670C 4G LTE module, and Neo-6M GPS to send location data to Firebase Realtime Database. Perfect for vehicle tracking, asset monitoring, or personal location sharing.

---

## 🛠️ Hardware Components

| Component              | Specification                    | Purpose            | Price (Approx.) |
| ---------------------- | -------------------------------- | ------------------ | --------------- |
| ESP32 D1 Mini          | ESP32-WROOM-32, 240MHz dual-core | Main controller    | $5–8            |
| A7670C 4G Module       | LTE Cat.1, SIMCOM                | Cellular data      | $15–25          |
| Neo-6M GPS             | UART, 3.3V–5V                    | GPS location       | $8–12           |
| 18650 Li-ion Battery   | 2600mAh, 3.7V                    | Power supply       | $5              |
| TP4056 Charging Module | Micro USB, protection            | Battery charging   | $2              |
| MT3608 Boost Converter | 2A output                        | Voltage regulation | $2              |
| Antennas               | GPS + 4G                         | Signal reception   | Included        |
| SIM Card               | Airtel Rwanda                    | Cellular network   | Varies          |

**Total Cost:** ~$40–55

---

## 🔌 Wiring Diagram

### Power Connections

```
18650 Battery (3.7V)
 ├── TP4056 Charging Module ← USB charging
 ├── MT3608 Boost (4.0V) → A7670C VIN
 └── AMS1117 3.3V → ESP32 3.3V + GPS VCC
```

### Data Connections

```
ESP32 → A7670C
GPIO21 (RX1) ← TX
GPIO22 (TX1) → RX
GPIO4         → PWR_KEY (optional)
GND           → GND

ESP32 → Neo-6M GPS
GPIO16 (RX2) ← GPS TX
GPIO17 (TX2) → GPS RX (optional)
3.3V         → VCC
GND          → GND
```

### Physical Layout

```
[Front View]
┌─────────────────────────────────────┐
│        ESP32 D1 Mini (Center)       │
│  21 ← A7670C TX      GPS TX → 16    │
│  22 → A7670C RX      GPS RX → 17    │
│   4 → PWR_KEY                       │
│  GND ───────┬───────┬─────── GND   │
│          A7670C   GPS Module        │
└─────────────────────────────────────┘
```

---

## ⚙️ Software Architecture

### 1. GPS Module (Neo-6M)

* Protocol: NMEA 0183 (UART)
* Baud Rate: 9600
* Library: TinyGPSPlus
* Update Rate: 1Hz
* Accuracy: ~2.5m CEP

### 2. Cellular Module (A7670C)

* Technology: LTE Cat.1
* Speed: 10 Mbps ↓ / 5 Mbps ↑
* APN: `internet` (Airtel Rwanda)
* Power: ~3mA idle, 500mA+ TX

### 3. Firebase Integration

```
tracker-6c988-default-rtdb.firebaseio.com/
└── devices/
    └── syndicate1/
        ├── -NgXpL...k1
        ├── -NgXpL...k2
        └── ...
```

Each entry:

```json
{
  "lat": -2.234770,
  "lng": 30.181665,
  "alt": 1476.0,
  "sat": 5,
  "spd": 3.2,
  "t": 1706187993000
}
```

---

## 📋 Setup Instructions

### Step 1: Hardware Assembly

* Solder headers
* Power chain first (battery → regulators → modules)
* Add 1000µF capacitor to A7670C VIN
* Connect antennas before power
* Insert SIM with data

### Step 2: Software Installation

Required libraries:

* TinyGPSPlus (Mikal Hart)
* ArduinoJSON (Benoit Blanchon)

Arduino settings:

* Board: ESP32 Dev Module
* Flash: 4MB
* Upload speed: 921600

### Step 3: Firebase Setup

* Create project: `tracker-6c988`
* Enable Realtime Database (test mode)
* Copy Database URL and Secret

### Step 4: SIM Configuration (Airtel Rwanda)

```
APN: internet
Username: blank
Password: blank
Activation: SMS "DATA" → 555
Balance: *550*1#
```

### Step 5: Upload Code

* Disconnect GPS & GSM during upload
* Upload firmware
* Reconnect modules
* Serial Monitor @115200

---

## 🚀 Code Logic Overview

```
loop():
1. Read GPS
2. Every 3s: minimal JSON upload
3. Every 6s: full JSON upload
4. Firebase auto push-ID
```

Key functions:

* setupGSM()
* firebasePush()
* buildMinimalJson()
* getGPSFixFast()

---

## 📊 Performance Metrics

### Update Modes

| Mode       | Interval | Data  | Use Case  |
| ---------- | -------- | ----- | --------- |
| Live       | 2–3s     | ~50B  | Real-time |
| Balanced   | 6s       | ~100B | Normal    |
| Power Save | 30s      | ~150B | Long life |

### Battery Life (18650 2600mAh)

* Live: 8–12 hrs
* Balanced: 18–24 hrs
* Power Save: 3–5 days

### Data Usage

* Daily: 1.7–17MB
* Monthly: 50–500MB

---

## 🌐 Web Dashboard

* Leaflet.js + Firebase fetch
* Live marker + path tracking
* Firebase Hosting supported

---

## 🔧 Troubleshooting

| Issue      | Cause          | Fix               |
| ---------- | -------------- | ----------------- |
| No GPS     | Weak signal    | Outdoor + antenna |
| No GSM     | No balance/APN | Check SIM         |
| HTTP Error | Power dip      | Add capacitor     |
| Drain fast | Too frequent   | Increase interval |

AT Commands:

```
AT
AT+CPIN?
AT+CSQ
AT+CREG?
AT+CGACT=1,1
```

---

## 🔒 Security

* Firebase rules validation
* Waterproof enclosure
* Hide wiring
* Token rotation

---

## 🚀 Advanced Features (Not yet implemented)

* Deep sleep
* Geofencing
* OTA updates
* Battery alerts
* SMS fallback
* SD card logging

---

## 👥 Credits

**Primary Developer:** Nelson RTZ

Special thanks:

* ESP32 community
* SIMCOM
* Firebase
* OpenStreetMap

---

## 📄 License

MIT License © 2025 Nelson RTZ

---

## 🌟 Success Story

Used across Rwanda for vehicle tracking with ~3s updates, under $50 hardware cost, and ~100 RWF/month data usage.

**Status:** Production Ready 🚀

*Last Updated: December 2025 | Version 2.0*
