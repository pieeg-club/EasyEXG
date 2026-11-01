# EasyEXG
## Amplifier design and verification

This board is an analog front end for **EMG** (muscle), **ECG** (heart) and **EOG** (eye) signals.
Its amplifier chain was simulated in ngspice with a 10 MHz op-amp model (OPA4388) and compared with the
[BioAmp EXG Pill](https://github.com/upsidedownlabs/BioAmp-EXG-Pill) by Upside Down Labs, rebuilt with an OPA4388.

![Simulated gain vs frequency](docs/amp_response.png)

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
| Closed | ECG, EEG | **1 048×** | 0.16–501 Hz | 2.3 mV / 1.5 mV |

- ✅ Instrumentation amplifier gain is 9.9×, as designed.
- ✅ The 0.16 Hz high-pass keeps the true ECG shape.
- ✅ The 500 Hz 2nd-order low-pass covers the whole EMG band (20–450 Hz) and removes noise above it.
- ✅ The INA333 rejects mains hum with CMRR ≥ 100 dB.
- ✅ The DRL loop is stable, with at most 2 dB of peaking (around 1.3–1.5 kHz).

### Comparison with BioAmp EXG Pill (OPA4388 version)

| | BioAmp EXG Pill (OPA4388) | This board |
|---|---|---|
| Input amplifier | Two-op-amp INA, 11× | INA333, 9.9× |
| Mains-hum rejection (CMRR) | ≈ 50 dB (set by 1 % resistor matching) | ≥ 100 dB |
| Total gain, EMG setting | 1 976× (JP1 closed) | 221× (JP1 open) |
| Total gain, high setting | 10 760× (JP1 open) | 1 048× (JP1 closed) |
| Band, EMG setting | 3.3–891 Hz | 0.16–501 Hz |
| Band, high setting | 3.2–171 Hz | 0.16–501 Hz |
| Max EMG input before clipping (5 V) | 1.2 mV | 10.9 mV |
| Reference electrode | Buffered VCC/2, 0 Ω | Driven right leg, 390 kΩ |
| 50 Hz common mode at input (simulated) | −51.9 dB | −55.3 dB (with R8 = 10 MΩ) |

Notes on the BioAmp OPA4388 version:

- Bridge **JP1** for EMG and ECG. With JP1 open the gain is 10 760×, which suits EEG; EMG and ECG signals will saturate.
- Keep **VCC ≤ 5.5 V**. The original TL074H accepts up to 40 V; the OPA4388 does not.
- **R11** is 0 Ω, connecting the op-amp output directly to the body. 1–10 kΩ is safer and keeps the op-amp stable when it drives the electrode cable.
- The 3.2 Hz high-pass is fine for heart rate and EMG, but it distorts the slow parts of the ECG waveform.

### Design note: R8 = 10 MΩ

R8 (DRL feedback resistor) should be **10 MΩ** (Yageo RC0603FR-0710ML, the same part as R3 and R4).
With 1 MΩ the right-leg drive is too weak: simulated 50 Hz common mode at the input is −46.8 dB, slightly worse
than a plain direct reference (−51.9 dB). With 10 MΩ it improves to **−55.3 dB**, with peaking still below +2 dB.
R9 stays at 390 kΩ, which limits the current into the body to about 13 µA at 5 V.
This is a value change only; the PCB does not change.

### Operating notes

- Supply: 3.3 V or 5 V (absolute max 5.5 V). OUT rests at VCC / 2 with no signal.
- Sample EMG at ≥ 1 kHz, ECG/EOG at ≥ 250 Hz.
- Electrodes (3.5 mm jack): Tip = IN+, Ring = IN−, Sleeve = REF/DRL.
- Power from a battery or an isolated USB supply while electrodes are on a person.

> Simulations use idealised electrode (51 kΩ ∥ 47 nF) and body (200 pF mains coupling) models, so real hum levels
> will differ. The comparison between the circuits is still valid. This is a hobby / research board, not a medical device.
