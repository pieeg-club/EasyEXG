# EasyEXG SDK – quick start

Software for EasyEXG: an Arduino sketch that reads the board and sends the data
over Wi-Fi, and a Python program that shows it live on your PC.

```
EasyEXG board  ──wires──►  Arduino UNO R4 WiFi  ──Wi-Fi──►  PC (Python viewer)
                               ▲
                               └── USB cable: only to upload the sketch
```

| Folder | What's inside |
|--------|---------------|
| [`Arduino`](Arduino) | Sketch for the Arduino (UNO R4 WiFi, ESP32, ESP8266) |
| [`Python`](Python) | Viewer for the PC: choose EEG / EOG / EMG / ECG, live graphs, CSV recording |

**The USB cable is only used once, to upload the sketch.** After that the data goes over Wi-Fi.


## Step 1 – Wire the board

| EasyEXG (J3) | Arduino |
|--------------|---------|
| GND  | GND |
| VCC  | 5V  |
| OUT  | A0  |
| VREF | not connected |

JP1 jumper: **open** for EMG / EOG (221×), **bridged** for ECG / EEG (1048×).


## Step 2 – Upload the sketch over USB (COM port)

1. Install the [Arduino IDE](https://www.arduino.cc/en/software).
2. **Tools → Board → Boards Manager**: install **Arduino UNO R4 Boards**.
3. Open `Arduino/1_Arduino.ino`.
4. At the top of the sketch, set your Wi-Fi:

   ```cpp
   #define USE_ACCESS_POINT 0                    // 0 = join your home Wi-Fi

   const char* STA_SSID = "YOUR_WIFI_NAME";      // your Wi-Fi name (2.4 GHz)
   const char* STA_PASS = "YOUR_WIFI_PASSWORD";  // your Wi-Fi password
   ```

   No router? Set `USE_ACCESS_POINT 1` and the board makes its own network
   called **EasyEXG** (password `easyexg123`).

5. Connect the Arduino to the PC with a USB cable.
6. **Tools → Board**: choose **Arduino UNO R4 WiFi**.
   **Tools → Port**: choose the COM port of the Arduino (for example `COM5`).
   On macOS it looks like `/dev/cu.usbmodem…`, on Linux `/dev/ttyACM0`.
7. Click **Upload** (→).


## Step 3 – Check the Wi-Fi connection

Open **Tools → Serial Monitor** and set it to **115200 baud**. You should see:

```
Connecting to YOUR_WIFI_NAME....
Board IP: 192.168.1.57  UDP port: 5005
Waiting for PC on Wi-Fi 'YOUR_WIFI_NAME', board IP 192.168.1.57
```

Write down the **Board IP**. You only need it if the viewer can't find the board by itself.

Now the board can run without the PC cable: power it from a **power bank** through USB.


## Step 4 – Connect via Wi-Fi with the Python viewer

1. Install [Python](https://www.python.org/downloads/) (on Windows tick **"Add Python to PATH"**).
2. Install the libraries once:

   ```
   pip install matplotlib numpy scipy
   ```

3. Make sure the PC is on the **same Wi-Fi** as the board
   (or on the **EasyEXG** network if you used `USE_ACCESS_POINT 1`).
4. Start the viewer:

   ```
   cd Python
   python easyexg_studio.py
   ```

5. Click **EEG**, **EOG**, **EMG** or **ECG**, then **Connect**.
   When Windows asks, allow Python on **Private and Public** networks.

Top graph: raw signal. Bottom graph: band-pass filtered signal.
See the [`Python` readme](Python) for all settings and the CSV format.


## Something not working?

| Problem | Fix |
|---------|-----|
| No COM port in Tools → Port | Try another USB cable (some only charge) or another USB socket. |
| Upload fails: "port busy" | Close the Serial Monitor / Plotter and any Python viewer, then upload again. |
| Serial Monitor shows only dots | Wrong Wi-Fi name or password, or a 5 GHz-only network. |
| Board makes "EasyEXG" network instead of joining yours | Set `USE_ACCESS_POINT 0` and upload again. |
| Viewer: "No data yet" | PC and board must be on the same Wi-Fi; allow Python in the Windows firewall. |
| Viewer can't find the board | Type the Board IP from step 3 into the "Board IP" box. |

More details: [`Arduino` readme](Arduino) · [`Python` readme](Python).


## Safety

EasyEXG has no electrical isolation. While electrodes are on a person, everything
must run on **battery**: the Arduino on a power bank and the laptop with its charger unplugged.
EasyEXG is a hobby and research board, **not a medical device**.
