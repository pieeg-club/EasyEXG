# EasyEXG

EasyEXG is a small analog front end for recording **EMG** (muscle), **ECG** (heart) and **EOG** (eye movement) signals
with any microcontroller ADC (Arduino, ESP32, STM32, Raspberry Pi Pico with an ADC, …).
It runs from **3.3 V or 5 V** and outputs one analog signal centred at VCC / 2.

<p align="center">
  <img src="Images/board_3d.gif" alt="EasyEXG board rotating in 3D" width="720">
</p>

## Board at a glance

<p align="center">
  <img src="Images/board_3d.png" alt="EasyEXG 3D model with dimensions" width="900">
</p>

*3D model of the board (revision EXG-Front v1) with the main dimensions. Orange lines are dimensions in mm.*

What you see in the picture:

| Label | Part | What it does |
|---|---|---|
| **J1** | 3.5 mm audio jack (CUI SJ-3523-SMT) | Plug for a standard 3-lead snap-electrode cable: Tip = IN+, Ring = IN−, Sleeve = REF/DRL |
| **J2** | 3-pin header, 2.54 mm (optional) | Same three electrode signals as J1, for loose wires: + / − / REF. Use J1 **or** J2, not both |
| **U1** | INA333 | Instrumentation amplifier, first gain stage (9.9×) |
| **U2** | OPA4388 | Quad op-amp: mid-supply reference, driven right leg, high-pass + gain, 500 Hz low-pass |
| **JP1** | Solder jumper "HI GAIN" | Open = 221× (EMG, EOG). Bridged with solder = 1 048× (ECG, EEG) |
| **J3** | 4-pin header, 2.54 mm | 1 GND · 2 VCC · 3 OUT (to ADC) · 4 VREF |
| **Ø2.2** | 2 × M2 mounting holes | 40.2 mm apart in X |

| Dimension | Value |
|---|---|
| Board | 46.0 × 26.0 × 1.6 mm, corner radius 1.5 mm, 2-layer FR-4 |
| With jack nose (sticks out 2.0 mm on the left) | 48.0 × 26.0 mm |
| Mounting holes | 2 × Ø2.2 mm (M2) at (3.0, 16.2) and (43.2, 21.8) mm from the top-left corner |
| Height: ICs / jack / pin headers | ≈ 1.2 mm / ≈ 5.0 mm / 8.5 mm above the board |
| Bottom side | no components |

Jack and header heights are typical catalogue values; check them against the parts you buy before designing an enclosure.

## How to start (Arduino)

<p align="center">
  <img src="Images/easyexg_arduino.gif" alt="How to connect EasyEXG to Arduino and read EMG" width="800">
</p>

1. **Wire J3 to the Arduino** with 3 jumper wires: **GND → GND**, **VCC → 5V**, **OUT → A0**. Leave VREF unconnected.
2. **Place the electrodes** (EMG on the forearm): IN+ and IN− on the muscle, 2–3 cm apart, and REF on a bony spot such as the elbow.
   Plug the electrode cable into the 3.5 mm jack J1. Leave JP1 open for EMG; bridge it for ECG.
3. **Upload the sketch** [`EasyEXG_Arduino.ino`](Arduino/EasyEXG_Arduino/EasyEXG_Arduino.ino) with the Arduino IDE.
4. **Open Tools → Serial Plotter** at **115200 baud** and clench your fist. You will see bursts on the blue trace (signal)
   and a rising yellow trace (envelope); when you relax, both go flat.

> ⚠️ Power the Arduino from a laptop running on battery (charger unplugged) while electrodes are on a person.

## Amplifier design and verification

The amplifier chain was simulated in ngspice with a 10 MHz op-amp model (OPA4388).

<p align="center">
  <img src="Images/amp_response.png" alt="Simulated gain versus frequency" width="900">
</p>

*Simulated total gain versus frequency. Blue: JP1 open (221×, for EMG and EOG). Green: JP1 bridged (1 048×, for ECG and EEG).
Shaded areas mark the ECG band (0.5–40 Hz) and the EMG band (20–450 Hz). The gain is flat from 0.16 Hz to 500 Hz
(dotted lines mark the −3 dB points) and rolls off steeply above 500 Hz.*

### Signal chain

| Stage | Part | Function | Gain |
|---|---|---|---|
| 1 | INA333 (U1) | Instrumentation amplifier, G = 1 + 100 kΩ / 11.2 kΩ | 9.9× |
| 2 | OPA4388 (U2C) | 0.16 Hz high-pass + non-inverting gain (JP1 selects gain) | 22× / 106× |
| 3 | OPA4388 (U2D) | 500 Hz 2nd-order Sallen-Key low-pass (Q = 0.73) | 1× |
| — | OPA4388 (U2A) | Mid-supply reference VREF = VCC / 2 | — |
| — | OPA4388 (U2B) | Driven right leg (DRL), 390 kΩ current limit | — |

### Gain and bandwidth

| JP1 | Use | Total gain | Band (−3 dB) | Max input before clipping (5 V / 3.3 V) |
|---|---|---|---|---|
| Open | EMG, EOG | **221×** | 0.16–501 Hz | 10.9 mV / 7.2 mV |
| Bridged | ECG, EEG | **1 048×** | 0.16–501 Hz | 2.3 mV / 1.5 mV |

- ✅ Instrumentation amplifier gain is 9.9×, as designed.
- ✅ The 0.16 Hz high-pass keeps the true ECG shape.
- ✅ The 500 Hz 2nd-order low-pass covers the whole EMG band (20–450 Hz) and removes noise above it.
- ✅ The INA333 rejects mains hum with CMRR ≥ 100 dB.
- ✅ The DRL loop is stable, with at most 2 dB of peaking (around 1.3–1.5 kHz).

### Design note: R8 = 10 MΩ

R8 (DRL feedback resistor) is **10 MΩ** (Yageo RC0603FR-0710ML, the same part as R3 and R4).
With 1 MΩ the right-leg drive is too weak: simulated 50 Hz common mode at the input is −46.8 dB. With 10 MΩ it
improves to **−55.3 dB**, with peaking still below +2 dB.
R9 stays at 390 kΩ, which limits the current into the body to about 13 µA at 5 V.

## Operating notes

- Supply: 3.3 V or 5 V (absolute max 5.5 V). OUT rests at VCC / 2 with no signal.
- Sample EMG at ≥ 1 kHz, ECG/EOG at ≥ 250 Hz.
- Electrodes (3.5 mm jack): Tip = IN+, Ring = IN−, Sleeve = REF/DRL.
- Power from a battery or an isolated USB supply while electrodes are on a person.

> Simulations use idealised electrode (51 kΩ ∥ 47 nF) and body (200 pF mains coupling) models, so real hum levels
> will differ. This is a hobby / research board, not a medical device.
