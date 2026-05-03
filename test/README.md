# Smart Agriculture Digital Twin — Wokwi + Firebase Setup

## Files
- `sketch.ino`   → Main ESP32 code
- `diagram.json` → Wokwi circuit (paste into diagram editor)
- `libraries.txt`→ Required libraries

---

## Step 1 — Firebase Setup
1. Go to https://console.firebase.google.com
2. Create new project → "smart-agri"
3. Build → Realtime Database → Create database (test mode)
4. Copy your database URL → looks like:
   your-project-default-rtdb.firebaseio.com
5. Project Settings → Service Accounts → Database Secrets → copy secret key

---

## Step 2 — Update sketch.ino
Replace these two lines:
```cpp
#define FIREBASE_HOST "https://mini-project-a9d3b-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "IGkj6BOba3Bhno8uo1eu8q7SsGYDO937WIMduYNZ"
```

---

## Step 3 — Wokwi Setup
1. Go to https://wokwi.com → New Project → ESP32
2. Paste diagram.json into the diagram editor tab
3. Paste sketch.ino into the code editor
4. Add libraries: FirebaseESP32, DHT sensor library
5. Press Play

---

## Firebase Data Structure
```
farm/
  ├── temperature    → 28.5 °C
  ├── humidity       → 54.2 %
  ├── soil_moisture  → 62.0 %
  ├── light_lux      → 72000 lux
  ├── time_of_day    → "Noon"
  ├── sim_time       → "12:30"
  ├── crop_status    → "Healthy"
  ├── irrigate       → false
  └── history/
       └── {timestamp}/
            ├── temp
            ├── humidity
            ├── moisture
            ├── light_lux
            └── time
```

---

## Day/Night Light Pattern
| Time       | Light (lux) | Description           |
|------------|-------------|----------------------|
| 00:00–5:30 | 0–5         | Night (moonlight)    |
| 5:30–7:00  | 0–15,000    | Dawn (rising)        |
| 7:00–12:00 | 15k–80,000  | Morning (rising)     |
| 12:00      | ~80,000     | Solar noon (peak)    |
| 12:00–18:00| 80k–15,000  | Afternoon (falling)  |
| 18:00–19:30| 15k–0       | Dusk (setting)       |
| 19:30–24:00| 0–5         | Night (moonlight)    |

> Wokwi simulates 1 full day every 24 minutes (1 sec = 1 min)

---

## Wiring Summary
| Sensor         | Pin  |
|----------------|------|
| DHT22 Data     | D4   |
| Soil Moisture  | D34  |
| LDR            | D35  |
| Status LED     | D2   |
| All VCC        | 3V3  |
| All GND        | GND  |
