# EasyEXG – Python viewer

These programs run on your PC. They receive the data that the Arduino sketch
(`Arduino` folder) sends over Wi-Fi and show it as live graphs.

| File | What it is |
|------|------------|
| `easyexg_studio.py` | **Main program.** A window where you choose EEG, EOG, EMG or ECG. Shows the raw signal and the band-pass filtered signal. |
| `easyexg_wifi_plot.py` | Simple viewer started from the command line. Shows the signal and its activity envelope. |

Both work on Windows, macOS and Linux with Python 3.9 or newer.


## 1. Install

Install Python from [python.org](https://www.python.org/downloads/).
On Windows, tick **"Add Python to PATH"** during installation.

Then install the libraries once (Command Prompt / Terminal):

```
pip install matplotlib numpy scipy
```


## 2. Before you start

1. Upload the sketch from the `Arduino` folder and check the Serial Monitor shows
   `Board IP: ...` (see the Arduino readme).
2. Your PC must be on the **same Wi-Fi** as the board:
   - `USE_ACCESS_POINT 0` in the sketch: PC on your home Wi-Fi
   - `USE_ACCESS_POINT 1` in the sketch: PC connected to the **EasyEXG** network (password `easyexg123`)
3. **Windows firewall:** the first time you run a program, Windows asks whether to allow Python.
   Tick **Private and Public** networks and click **Allow**.


## 3. EasyEXG Studio (recommended)

```
python easyexg_studio.py
```

Or open the file in IDLE and press **F5**.

1. Click **EEG**, **EOG**, **EMG** or **ECG**.
   The window shows how to set the JP1 jumper and where to put the electrodes.
2. Leave Source on **Wi-Fi** and click **Connect**. The board is found automatically.
3. Top graph: raw signal. Bottom graph: signal after the band-pass filter.

### Settings for each measurement

| Mode | Band-pass | JP1 / gain | Window | Extra reading |
|------|-----------|------------|--------|---------------|
| EEG  | 1–40 Hz   | bridged, 1048× | 5 s  | Alpha (8–12 Hz) share of power |
| EOG  | 0.1–10 Hz | open, 221×     | 10 s | – |
| EMG  | 20–240 Hz | open, 221×     | 3 s  | Activity line (moving RMS, orange) |
| ECG  | 0.5–40 Hz | bridged, 1048× | 5 s  | Heart rate (bpm) |

You can change these in the window:

- **Band-pass**: type new edges and press **Apply**
- **Notch**: removes mains hum at 50 or 60 Hz and its harmonics inside the band
- **Gain**: must match the JP1 jumper on the board, so the µV values are correct
- **Window**: seconds shown on screen
- **● Record CSV**: saves every sample to a file
- **Source → Demo**: fake signal for each mode, to try the program without the board

The status bar at the bottom shows the samples per second received (should be about 500)
and how many samples were lost over Wi-Fi.

### Settings at the top of the file

| Setting    | Default  | Meaning |
|------------|----------|---------|
| `BOARD_IP` | `"auto"` | Finds the board by itself. Put the IP from the Serial Monitor here if that fails. |
| `UDP_PORT` | `5005`   | Must match the Arduino sketch. |
| `VREF`     | `5.0`    | ADC reference voltage: 5.0 for UNO R4, 3.3 for ESP32. |
| `MAINS_HZ` | `50`     | 50 in UK / Europe / Asia, 60 in the Americas. |


## 4. Simple viewer

```
python easyexg_wifi_plot.py
```

Options:

| Option | Meaning |
|--------|---------|
| `--host IP` | Board IP (default `auto`, finds it by itself) |
| `--seconds 5` | Width of the scrolling window |
| `--units uV --gain 1048` | Show µV at the electrodes. Gain 221 with JP1 open, 1048 bridged. |
| `--vref 3.3` | For ESP32 boards |
| `--csv file.csv` | Also save the data |
| `--demo` | Fake signal, no board needed |

Example for ECG:

```
python easyexg_wifi_plot.py --units uV --gain 1048 --csv ecg.csv
```


## 5. Recorded CSV files

Files from EasyEXG Studio have these columns:

| Column | Meaning |
|--------|---------|
| `time_s` | Time from the start of the recording, in seconds |
| `raw_uV` | Raw signal, µV at the electrodes |
| `filtered_uV` | Signal after the band-pass and notch filters |
| `mode` | EEG, EOG, EMG or ECG |
| `band_Hz` | Band-pass used, e.g. `0.5-40` |
| `gain` | Gain setting (221 or 1048) |

Example of loading a file:

```python
import pandas as pd
import matplotlib.pyplot as plt

d = pd.read_csv("easyexg_ECG_20261125_180551.csv")
plt.plot(d.time_s, d.filtered_uV)
plt.xlabel("Time (s)"); plt.ylabel("µV")
plt.show()
```

Record at least 10–20 seconds so there are enough heartbeats, blinks or muscle bursts to look at.


## Troubleshooting

| Problem | Fix |
|---------|-----|
| "No data yet" / "Looking for the board" | PC and board on the same Wi-Fi? Check `USE_ACCESS_POINT` in the sketch. |
| Still no data | Windows firewall: allow Python on Private **and** Public networks. |
| Auto doesn't find the board | Type the IP from the Arduino Serial Monitor into "Board IP" (or `--host`). |
| `ModuleNotFoundError` | Run `pip install matplotlib numpy scipy` again. |
| Much fewer than 500 samples/s, or "lost" keeps rising | Weak Wi-Fi: move the board closer to the router or use mode 1. |
| Signal jumps in steps every 50 ms | Wi-Fi interference on the board; see the note in the Arduino readme. |
| Values look far too big or too small | The Gain setting doesn't match the JP1 jumper. |


## Safety

EasyEXG has no electrical isolation. While electrodes are on a person, the board,
the Arduino and the laptop must all run on **battery** (laptop charger unplugged).
EasyEXG is a hobby and research board, **not a medical device**.

