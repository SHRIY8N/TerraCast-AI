# 🌱 TerraCast AI

### Weather-Aware Smart Irrigation System using Arduino UNO Q

TerraCast AI is a single-plant smart irrigation prototype built using the Arduino UNO Q. It combines soil-moisture sensing with weather information to make a simple irrigation decision: when the soil is dry and significant rain is not expected, the system activates a water pump. The pump continues watering until the soil reaches the defined wet threshold.

The project is designed as a physical AI demonstration where sensing, decision-making, and physical actuation happen as one system.

---

## 🎯 Problem

Traditional automatic plant-watering systems often rely only on soil moisture. This can cause unnecessary watering when rain is expected.

TerraCast AI addresses this by considering both:

- Soil moisture
- Weather / rain probability

The goal is to reduce unnecessary water usage while keeping the plant adequately watered.

---

## 🧠 How TerraCast AI Works

The system continuously reads the soil moisture sensor.

### Soil calibration

| Raw sensor value | Soil state |
|---:|---|
| `< 1000` | Wet |
| `1000–1200` | Moderate |
| `> 1200` | Dry |

### Irrigation decision

```text
Soil is dry
      ↓
Check weather
      ↓
Is rain probability ≥ 60%?
   ↙              ↘
 YES               NO
 ↓                  ↓
SKIP WATERING     PUMP ON
                    ↓
             Keep watering
                    ↓
             RAW <= 1000?
                 ↓
              PUMP OFF
