
# 🔄 STM32 Rotary Encoder Interface
### Autonics E50S8-500-3-T-1 × STM32F103 (Blue Pill)

Real-time RPM measurement and distance tracking using STM32 hardware quadrature encoder timer (TIM2), with EMA filtering and dual UART output.

---

## 📋 Table of Contents
- [Hardware](#-hardware)
- [Wiring](#-wiring)
- [How It Works](#-how-it-works)
- [Configuration](#-configuration)
- [Output Format](#-output-format)
- [Tuning Guide](#-tuning-guide)
- [Known Issues & Fixes](#-known-issues--fixes)
- [Fault Diagnosis](#-fault-diagnosis)
- [Project Structure](#-project-structure)

---

## 🔧 Hardware

| Component | Details |
|-----------|---------|
| **MCU** | STM32F103C8T6 (Blue Pill) |
| **Encoder** | Autonics E50S8-500-3-T-1 |
| **PPR** | 500 Pulses Per Revolution |
| **Effective Resolution** | 2000 counts/rev (×4 quadrature) |
| **Timer** | TIM2 — Encoder Mode TI12 |
| **UART** | USART1 (external) + USART2 (serial monitor) @ 115200 baud |
| **Framework** | STM32 HAL (CubeIDE / CubeMX) |

---

## 🔌 Wiring

```
Autonics E50S8              STM32F103 Blue Pill
─────────────────           ───────────────────
Brown   (+VCC)   ──────────  5V / 12V
Blue    (GND)    ──────────  GND
Black   (A)      ──────────  PA0  (TIM2_CH1)
White   (B)      ──────────  PA1  (TIM2_CH2)
Orange  (Z)         ✗        Not connected (index pulse unused)
```

> ⚠️ **Critical:** If the encoder supply is 12V, level-shift A and B signals to 3.3V before connecting to STM32 GPIO pins.

---

## ⚙️ How It Works

```
┌─────────────────────────────────────────────────────────┐
│                    Every 100 ms                         │
│                                                         │
│  TIM2 Counter ──► Delta Calculation ──► Noise Filter   │
│                                              │          │
│                                         Dead-band       │
│                                         (< 1 count)     │
│                                              │          │
│                        ┌─────────────────────┤          │
│                        ▼                     ▼          │
│                  RPM Calculation      Position Update   │
│                  (counts/rev/sec)     (g_total_counts)  │
│                        │                     │          │
│                   EMA Filter            Distance (cm)   │
│                  (alpha = 0.5)               │          │
│                        │                     │          │
│                        └──────────┬──────────┘          │
│                                   ▼                     │
│                           UART Transmit                 │
│                      USART1 + USART2 (both)             │
└─────────────────────────────────────────────────────────┘
```

**Key formulas:**

```c
// RPM
rpm_raw = (|delta| / COUNTS_PER_REV) / sample_sec * 60.0f;

// EMA filter (smoothing)
rpm_filtered = alpha * rpm_raw + (1 - alpha) * rpm_filtered;

// Distance
distance_cm = (g_total_counts / COUNTS_PER_REV) * CIRCUMFERENCE_CM;
```

---

## 🛠️ Configuration

All tunable parameters are at the top of `main.c`:

```c
/* USER CODE BEGIN PD */
#define COUNTS_PER_REV    2000      // 500 PPR × 4 quadrature
#define CIRCUMFERENCE_CM  10.0f     // cm per full revolution — set for your application
#define SAMPLE_MS         100       // sampling window in ms (lower = faster updates)
#define MIN_DELTA         1         // noise deadband in counts
#define EMA_ALPHA         0.5f      // 0.0 = max smooth/slow, 1.0 = raw/instant
/* USER CODE END PD */
```

### TIM2 Encoder Settings (CubeMX)

```c
sConfig.EncoderMode  = TIM_ENCODERMODE_TI12;
sConfig.IC1Polarity  = TIM_ICPOLARITY_RISING;   // ← MUST be RISING
sConfig.IC2Polarity  = TIM_ICPOLARITY_RISING;   // ← MUST be RISING (not FALLING!)
sConfig.IC1Filter    = 15;                       // max noise filtering
sConfig.IC2Filter    = 15;
htim2.Init.Period    = 65535;                    // full 16-bit range
```

> ⚠️ **Setting IC2Polarity to FALLING breaks direction detection silently — both CW and CCW will appear identical.**

---

## 📟 Output Format

Transmitted over both UART ports at 115200 baud every 100 ms:

```
=== Autonics E50S8-500-3-T-1 ===
[CW ] RPM:  125.76  |  Distance:    20.430 cm
[CW ] RPM:  150.90  |  Distance:    31.385 cm
[---] RPM:    0.00  |  Distance:    43.645 cm   ← shaft stopped
[CCW] RPM:   88.70  |  Distance:    38.200 cm   ← reversed direction
```

| Field | Description |
|-------|-------------|
| `[CW ]` | Clockwise rotation |
| `[CCW]` | Counter-clockwise rotation |
| `[---]` | Shaft stationary |
| `RPM` | EMA-filtered revolutions per minute |
| `Distance` | Signed cumulative travel in cm (+ = CW, − = CCW) |

---

## 📐 Tuning Guide

### SAMPLE_MS — Update Rate vs. Accuracy

| Value | Update Rate | Best For |
|-------|-------------|----------|
| `50`  | 20 Hz | High-speed motors, fastest response |
| `100` | 10 Hz | ✅ General purpose (recommended) |
| `200` | 5 Hz  | Slow machines, better low-RPM accuracy |
| `500` | 2 Hz  | Very slow / near-static measurements |

### EMA_ALPHA — Smoothing vs. Response

| Value | Smoothing | Response | Best For |
|-------|-----------|----------|----------|
| `0.1` | Very heavy | Slow | Very noisy signals |
| `0.2` | Heavy | Slow | Noisy environments |
| `0.5` | Moderate | Medium | ✅ General purpose (recommended) |
| `0.7` | Light | Fast | Clean signals, quick response needed |
| `1.0` | None (raw) | Instant | Debug only |

---

## 🐛 Known Issues & Fixes

All bugs encountered and resolved during development:

| # | Symptom | Root Cause | Fix |
|---|---------|------------|-----|
| 1 | Distance always increased | IC2 polarity = FALLING broke direction detection | Set `IC2Polarity = TIM_ICPOLARITY_RISING` |
| 2 | Both CW and CCW showed `[CW]` | Same as above | Same fix |
| 3 | RPM unstable, large spikes | 500ms sample window, no smoothing | Added EMA filter + reduced `SAMPLE_MS` to 100 |
| 4 | 1–2 second delay in values | `SAMPLE_MS=500`, `EMA_ALPHA=0.2` | Set `SAMPLE_MS=100`, `EMA_ALPHA=0.5` |
| 5 | RPM decayed slowly after stop | EMA formula `g * (1-alpha)` never reached zero | Force `g_rpm_filtered = 0.0f` when `delta == 0` |
| 6 | EMA_ALPHA tuning had no effect | `#define` inside function — can't be changed from top | Moved to top-level `#define` section |
| 7 | Negative distance readings | `delta = -delta` applied when wiring already correct | Matched negate line to actual wiring direction |

---

## 🔍 Fault Diagnosis

| Symptom | Most Likely Cause | Action |
|---------|-------------------|--------|
| Distance only increases | Direction detection broken | Check `IC2Polarity` — must be `RISING` |
| Both rotations show `[CW]` | `IC2Polarity = FALLING` | Change to `TIM_ICPOLARITY_RISING` in `MX_TIM2_Init()` |
| RPM doesn't drop to 0 on stop | EMA decay not hard-zeroed | Ensure `delta==0` sets `g_rpm_filtered = 0.0f` |
| RPM very spiky / unstable | `SAMPLE_MS` too large | Reduce to `100`, increase `EMA_ALPHA` to `0.5` |
| No counts at all | Wiring or power issue | Check VCC/GND, probe A and B with oscilloscope |
| Counts but wrong direction | A and B wires swapped | Swap A↔B wires on connector |
| Values delayed 1–2 seconds | `SAMPLE_MS=500`, `EMA_ALPHA=0.2` | Change both as described above |

---

## 📁 Project Structure

```
project/
├── Core/
│   ├── Src/
│   │   └── main.c          ← All encoder logic lives here
│   └── Inc/
│       └── main.h
├── Drivers/
│   └── STM32F1xx_HAL_Driver/
├── .ioc                    ← CubeMX project file
└── README.md
```

### Key sections in `main.c`

| Section | Location | Contents |
|---------|----------|----------|
| Defines | `USER CODE BEGIN PD` | `COUNTS_PER_REV`, `SAMPLE_MS`, `EMA_ALPHA`, etc. |
| Global variables | `USER CODE BEGIN PV` | `g_total_counts`, `g_prev_count`, `g_rpm_filtered` |
| Init & startup | `USER CODE BEGIN 2` | Encoder start, settle delay, zero counter |
| Main loop | `USER CODE BEGIN 3` | 100ms timer check → `Encoder_Process()` |
| Encoder logic | `USER CODE BEGIN 4` | `Encoder_Process()` + `Send_Both()` |

---

## 📝 Notes for Future Maintainers

- **`delta = -delta`** may or may not be needed depending on how A/B are physically wired. If CW and CCW are swapped, remove this line or swap the wires — do not do both.
- **`g_total_counts`** accumulates indefinitely from power-on. Reset it deliberately with `g_total_counts = 0` if you need a new zero reference point.
- **`CIRCUMFERENCE_CM`** must match your actual mechanical setup (wheel/pulley circumference attached to the shaft).
- Changing **`SAMPLE_MS`** automatically adjusts the RPM formula — no other changes needed.

---

## 📄 License

Internal project — for reference and knowledge transfer.

---

*Encoder: Autonics E50S8-500-3-T-1 | MCU: STM32F103C8T6 | Framework: STM32 HAL*
