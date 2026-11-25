
# EasyEXG – Arduino Wi-Fi sketch

`1_Arduino.ino` reads the EasyEXG board on pin A0 and sends the data over Wi-Fi to your PC.
On the PC, the Python viewer in the `Python` folder shows it as a live graph.

What the sketch does:
- samples the signal 500 times per second
- removes the slow drift (the resting level of the board output)
- removes 50 Hz mains hum (notch filter)
- calculates an activity envelope (useful for EMG)
- sends 25 samples per packet, 20 packets per second, over UDP

Supported boards: **Arduino UNO R4 WiFi**, ESP32, ESP8266.
A classic Arduino UNO has no Wi-Fi and will not work with this sketch.


## 1. Wiring

| EasyEXG (J3) | Arduino |
|--------------|---------|
| GND          | GND     |
| VCC          | 5V      |
| OUT          | A0      |
| VREF         | not connected |

ESP32 / ESP8266: connect VCC to **3.3V**, because their analog input takes 3.3 V max.

Gain jumper JP1:
- **open**: 221× gain, for EMG and EOG
- **bridged**: 1048× gain, for ECG and EEG


## 2. Choose the Wi-Fi mode

Open `1_Arduino.ino` and find the settings at the top.

```cpp
#define USE_ACCESS_POINT 0
```

**Mode 0: the board joins your home Wi-Fi (recommended)**

Set `USE_ACCESS_POINT` to `0` and type your Wi-Fi name and password:

```cpp
#define USE_ACCESS_POINT 0

const char* STA_SSID = "YOUR_WIFI_NAME";      // your Wi-Fi network name
const char* STA_PASS = "YOUR_WIFI_PASSWORD";  // your Wi-Fi password
```

Your PC must be on the same Wi-Fi network.
Use a 2.4 GHz network: the boards cannot join 5 GHz-only networks.

**Mode 1: the board makes its own Wi-Fi network**

Set `USE_ACCESS_POINT` to `1`. The board creates a network:

```cpp
const char* AP_SSID = "EasyEXG";
const char* AP_PASS = "easyexg123";   // at least 8 characters
```

Connect your PC to the **EasyEXG** network. No router is needed, but the PC has
no internet while it is connected.

> **Important:** typing your Wi-Fi name and password is not enough.
> You must also set `USE_ACCESS_POINT` to `0`, otherwise the board ignores them
> and makes its own "EasyEXG" network.

> **Do not share your password.** Before you upload the sketch to GitHub or send it to
> anyone, replace your Wi-Fi name and password with `YOUR_WIFI_NAME` / `YOUR_WIFI_PASSWORD`.


## 3. Other settings

| Setting      | Default | Meaning |
|--------------|---------|---------|
| `FS`         | 500     | Samples per second. 1000 is better for EMG. |
| `ADC_BITS`   | 10      | ADC resolution. Use 14 on UNO R4 for EEG (finer steps). |
| `USE_NOTCH`  | true    | Remove mains hum. |
| `NOTCH_HZ`   | 50.0    | 50 Hz in UK / Europe / Asia, 60 Hz in the Americas. |
| `UDP_PORT`   | 5005    | Must match the Python viewer. |

The Python viewer reads the sample rate and ADC resolution from the packets,
so you do not need to change anything on the PC.


## 4. Upload and check

1. Arduino IDE → **Tools → Board** → choose your board
   (UNO R4 WiFi needs the "Arduino UNO R4 Boards" package in the Boards Manager).
2. Click **Upload**.
3. Open **Tools → Serial Monitor** at **115200 baud**.

You should see:

```
Connecting to YOUR_WIFI_NAME....
Board IP: 192.168.1.57  UDP port: 5005
Waiting for PC on Wi-Fi 'YOUR_WIFI_NAME', board IP 192.168.1.57
```

When the PC viewer connects:

```
Streaming to 192.168.1.20:52113
```

If it keeps printing dots after "Connecting to", the Wi-Fi name or password is wrong,
or the network is 5 GHz only.


## 5. Run the viewer on the PC

See the `Python` folder. In short:

```
pip install matplotlib numpy scipy
python easyexg_studio.py
```

Choose EEG, EOG, EMG or ECG and press **Connect**. The viewer finds the board by itself.
If it does not, type the board IP from the Serial Monitor into the "Board IP" box.


## Troubleshooting

| Problem | Fix |
|---------|-----|
| Viewer says "No data yet" | PC and board must be on the same Wi-Fi. Check `USE_ACCESS_POINT`. |
| Still no data | Windows firewall: allow Python on **Private and Public** networks. |
| Board never connects | Wrong name/password, or a 5 GHz-only network. |
| Signal jumps in steps every 50 ms | Wi-Fi interference (see below). |
| Flat line | Check the electrodes, the cable in the jack, and the wiring to A0. |


## Note on Wi-Fi and signal quality

When the board sends a Wi-Fi packet, the radio draws short bursts of current.
This can show up in the signal as small steps every 50 ms, especially with
high gain (JP1 bridged) or loose electrodes. To reduce it:

- use fresh gel electrodes on clean skin and push the plug fully into the jack
- power the Arduino from a power bank instead of the laptop
- for the cleanest signal, use the USB sketch and the Arduino Serial Plotter


## Data packet format

For your own software. Little-endian, 16-byte header followed by the samples:

| Bytes | Type      | Content |
|-------|-----------|---------|
| 0–3   | char[4]   | `"EXG1"` |
| 4–7   | uint32    | index of the first sample in this packet |
| 8–9   | uint16    | number of samples (normally 25) |
| 10–11 | uint16    | sample rate (Hz) |
| 12    | uint8     | ADC bits |
| 13–15 |           | padding |
| 16…   | float, float | per sample: signal, envelope (ADC counts, centred at 0) |

The PC must send the text `HELLO` to the board's UDP port 5005 about once a second.
The board streams to the last PC that said hello, and stops 5 s after the hellos stop.


## Safety

EasyEXG has no electrical isolation. While electrodes are on a person, the board,
the Arduino and the laptop must all run on **battery** (laptop charger unplugged).
EasyEXG is a hobby and research board, **not a medical device**.
