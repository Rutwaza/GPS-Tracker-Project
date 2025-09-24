# Multi-Device GPS Tracker with Firebase + ESP32 + Leaflet.js

This document provides a **step-by-step roadmap** to replicate the multi-device GPS tracking project. It covers the hardware, circuit design, firmware (ESP32), Firebase setup, and the web dashboard with real-time map, device persistence, polylines, and flags.

---

## 1. Hardware & Components

* **ESP32** (NodeMCU-32S or WROOM module)
* **Neo-6M GPS Module** (or similar)
* **Power Source** (LiPo battery or USB)
* **Jumper Wires**
* **Breadboard**

### Circuit Connections

| ESP32 Pin               | GPS Module |
| ----------------------- | ---------- |
| 3.3V                    | VCC        |
| GND                     | GND        |
| RX (GPIO16 recommended) | TX         |
| TX (GPIO17 recommended) | RX         |

> **Tip:** GPS modules require a clear view of the sky for accurate signals.

---

## 2. Firebase Setup

We use **Firebase Realtime Database** to store device locations and history.

### Steps:

1. Go to [Firebase Console](https://console.firebase.google.com/) and create a new project.
2. Add a **Realtime Database**.
3. Set database rules to allow read/write while testing (restrict later for security):

   ```json
   {
     "rules": {
       ".read": true,
       ".write": true
     }
   }
   ```
4. Copy the Firebase config from **Project Settings > Web App**.
5. Your database will look like:

   ```
   devices/{deviceID}
     lat: ...
     lng: ...
     speed: ...
     time: ...
   history/{deviceID}/timestamp
     lat: ...
     lng: ...
   flags/{flagID}
     lat: ...
     lng: ...
     label: ...
   ```

---

## 3. ESP32 Firmware

The ESP32 reads GPS data and uploads it to Firebase.

### Key Features:

* Connects to WiFi.
* Reads NMEA data from the GPS module.
* Sends latitude, longitude, altitude, speed, satellites, and HDOP to:

  * `devices/{trackerName}` for real-time position.
  * `history/{trackerName}/{timestamp}` for path persistence.

### Pseudocode Logic:

```
loop:
    read GPS data
    if valid:
        write to /devices/device1
        push to /history/device1/timestamp
```

### Libraries Needed:

* TinyGPS++
* Firebase ESP Client
* WiFi

Upload firmware to each ESP32 with unique `trackerName` (e.g., tracker1, tracker2).

---

## 4. Web Dashboard

The frontend uses **Leaflet.js**, **Chart.js**, and Firebase JS SDK.

### Key Features:

* **Real-time updates:** All connected devices appear automatically.
* **Polylines:** Tracks path history from Firebase `history` node.
* **Flags:** Right-click to set flags anywhere on the map (persisted in Firebase).
* **Flag Removal:** Right-click flag to delete.
* **Device Sidebar:** Searchable list with speed/altitude, auto-updates for online/offline.
* **Charts:** Live altitude & speed charts for the selected device.

### Workflow:

1. Load Firebase config.
2. Listen to `/devices` for live data.
3. Listen to `/history` for polyline reconstruction.
4. Listen to `/flags` to persist markers across sessions.
5. Allow right-click actions on the map for adding/removing flags.

### File Structure:

```
/web
  |-- index.html   (Dashboard)
  |-- /assets
      |-- leaflet.css/js
      |-- chart.js
```

---

## 5. Deployment

* **Local Test:** Serve `index.html` using `Live Server` in VS Code or any HTTP server.
* **Hosting:**

  * Use **Firebase Hosting** or **GitHub Pages** for free hosting.
  * Ensure the Firebase config in HTML matches your project.

---

## 6. Optional Enhancements

* **Authentication:** Restrict access with Firebase Auth.
* **Device Commands:** Add remote control features (e.g., reset device).
* **Advanced Analytics:** Add distance traveled, geofencing, or alerts.

---

## Project Flow Summary

1. **Hardware**: ESP32 + GPS wired to send NMEA data.
2. **Firmware**: ESP32 uploads live data + history to Firebase.
3. **Database**: Realtime DB stores devices, history, and flags.
4. **Web App**: Leaflet dashboard displays devices, tracks paths, and manages flags.
5. **Persistence**: Polylines & flags persist even after refresh or reconnect.

---

## Repository Structure for GitHub

```
GPS-Tracker-Project/
├─ firmware/
│  └─ esp32_tracker.ino
├─ web/
│  └─ index.html
└─ README.md (this roadmap)
```

Add this document as `README.md` to your GitHub repo for clear instructions.
