/*
  EasyEXG over Wi-Fi (UDP)
  ------------------------
  Samples one analog pin at 500 Hz, removes the resting level, applies a mains
  notch, computes an activity envelope, and streams packets of samples over UDP
  to the PC running easyexg_wifi_plot.py.

  Boards: Arduino UNO R4 WiFi, ESP32 (any), ESP8266.

  Two network modes (set USE_ACCESS_POINT below):
    1 = the board makes its own Wi-Fi network "EasyEXG" (no router needed).
        Connect the PC to that network, then run:
            python easyexg_wifi_plot.py
        (default board address 192.168.4.1)
    0 = the board joins your Wi-Fi router. Open the Serial Monitor once at
        115200 baud to read the board's IP, then run:
            python easyexg_wifi_plot.py --host <board-ip>

  The PC sends "HELLO" about once a second; the board streams to whoever said
  hello last, and stops 5 s after the hellos stop.

  Packet (little-endian), 20 packets per second:
    char[4] "EXG1" | uint32 first_sample_index | uint16 count | uint16 fs |
    uint8 adc_bits | 3 bytes pad | count x { float signal, float envelope }
*/

#include <Arduino.h>
#if defined(ARDUINO_UNOR4_WIFI)
  #include <WiFiS3.h>
#elif defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <WiFiUdp.h>
#else
  #error "This sketch needs a Wi-Fi board: Arduino UNO R4 WiFi, ESP32 or ESP8266."
#endif

// ---------------- settings ----------------
// !!! IMPORTANT: choose the mode !!!
//   1 = board makes its own Wi-Fi called "EasyEXG". Connect your PC to it.
//   0 = board joins YOUR home Wi-Fi (fill in STA_SSID / STA_PASS below).
// Typing your Wi-Fi name and password is NOT enough: you must also set this to 0.
#define USE_ACCESS_POINT 0

const char* AP_SSID  = "EasyEXG";
const char* AP_PASS  = "easyexg123";     // at least 8 characters
const char* STA_SSID = "BT-Your Wi Fi";   // used when USE_ACCESS_POINT = 0
const char* STA_PASS = "Your Password";

const uint16_t UDP_PORT = 5005;

const int      PIN        = A0;          // ESP32: A0 = GPIO36 on most boards (input max 3.3 V!)
const uint8_t  ADC_BITS   = 10;          // 0..1023, same as a classic UNO
const uint16_t FS         = 500;         // samples per second
const uint32_t PERIOD_US  = 1000000UL / FS;
const bool     USE_NOTCH  = true;
const float    NOTCH_HZ   = 50.0;        // 50 Hz (Europe/Asia) or 60 Hz (Americas)

const uint8_t  SAMPLES_PER_PACKET = 25;  // 25 samples -> 20 packets/s
const uint32_t CLIENT_TIMEOUT_MS  = 5000;

// ---------------- notch filter (biquad) ----------------
float nb0, nb1, nb2, na1, na2;
float xPrev1 = 0, xPrev2 = 0, yPrev1 = 0, yPrev2 = 0;

void setupNotch(float f0, float fs, float q) {
  float w0 = 2.0f * PI * f0 / fs;
  float alpha = sinf(w0) / (2.0f * q);
  float c = cosf(w0);
  float a0 = 1.0f + alpha;
  nb0 = 1.0f / a0;  nb1 = -2.0f * c / a0;  nb2 = 1.0f / a0;
  na1 = -2.0f * c / a0;  na2 = (1.0f - alpha) / a0;
}

float notch(float x) {
  float y = nb0 * x + nb1 * xPrev1 + nb2 * xPrev2 - na1 * yPrev1 - na2 * yPrev2;
  xPrev2 = xPrev1; xPrev1 = x;
  yPrev2 = yPrev1; yPrev1 = y;
  return y;
}

// ---------------- network state ----------------
WiFiUDP udp;
IPAddress clientIP;
uint16_t clientPort = 0;
bool haveClient = false;
uint32_t lastHelloMs = 0;
IPAddress boardIP;
uint32_t lastInfoMs = 0;

const size_t HEADER_BYTES = 16;
uint8_t packet[HEADER_BYTES + SAMPLES_PER_PACKET * 8];
uint8_t inCount = 0;
uint32_t sampleIndex = 0;      // index of the next sample taken
uint32_t packetFirstIndex = 0;

// ---------------- signal state ----------------
float baseline = (1 << ADC_BITS) / 2.0f;
float envelope = 0;
uint32_t nextSampleUs;

void startWiFi() {
#if USE_ACCESS_POINT
  #if defined(ARDUINO_UNOR4_WIFI)
    if (WiFi.beginAP(AP_SSID, AP_PASS) != WL_AP_LISTENING) {
      Serial.println("Could not start the access point.");
      while (true) delay(1000);
    }
    delay(2000);
    IPAddress ip = WiFi.localIP();
  #else
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    IPAddress ip = WiFi.softAPIP();
  #endif
  Serial.print("Wi-Fi network: "); Serial.print(AP_SSID);
  Serial.print("  password: ");    Serial.println(AP_PASS);
#else
  #if !defined(ARDUINO_UNOR4_WIFI)
    WiFi.mode(WIFI_STA);
  #endif
  Serial.print("Connecting to "); Serial.print(STA_SSID);
  WiFi.begin(STA_SSID, STA_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  #if defined(ESP32)
    WiFi.setSleep(false);              // lower latency, steadier packets
  #endif
  IPAddress ip = WiFi.localIP();
  while (ip == IPAddress(0, 0, 0, 0)) { delay(200); ip = WiFi.localIP(); }
  Serial.println();
#endif
  boardIP = ip;
  Serial.print("Board IP: "); Serial.print(ip);
  Serial.print("  UDP port: "); Serial.println(UDP_PORT);
  udp.begin(UDP_PORT);
}

// Check for HELLO from the PC (called once per packet, not every sample).
void serviceNetwork() {
  int len = udp.parsePacket();
  while (len > 0) {
    char buf[16] = {0};
    int n = udp.read((uint8_t*)buf, sizeof(buf) - 1);
    if (n >= 5 && strncmp(buf, "HELLO", 5) == 0) {
      IPAddress ip = udp.remoteIP();
      uint16_t port = udp.remotePort();
      if (!haveClient || ip != clientIP || port != clientPort) {
        Serial.print("Streaming to "); Serial.print(ip); Serial.print(":"); Serial.println(port);
      }
      clientIP = ip; clientPort = port; haveClient = true; lastHelloMs = millis();
    }
    len = udp.parsePacket();
  }
  // while nobody is connected, repeat the address so it's easy to find
  if (!haveClient && millis() - lastInfoMs > 3000) {
    lastInfoMs = millis();
#if USE_ACCESS_POINT
    Serial.print("Waiting for PC. Join Wi-Fi '"); Serial.print(AP_SSID);
    Serial.print("' (password "); Serial.print(AP_PASS); Serial.print("), board IP ");
#else
    Serial.print("Waiting for PC on Wi-Fi '"); Serial.print(STA_SSID); Serial.print("', board IP ");
#endif
    Serial.println(boardIP);
  }
  if (haveClient && millis() - lastHelloMs > CLIENT_TIMEOUT_MS) {
    haveClient = false;
    Serial.println("PC stopped saying hello; pausing stream.");
  }
}

void sendPacket() {
  memcpy(packet, "EXG1", 4);
  memcpy(packet + 4, &packetFirstIndex, 4);
  uint16_t count = inCount;
  memcpy(packet + 8, &count, 2);
  uint16_t fs = FS;
  memcpy(packet + 10, &fs, 2);
  packet[12] = ADC_BITS; packet[13] = packet[14] = packet[15] = 0;
  udp.beginPacket(clientIP, clientPort);
  udp.write(packet, HEADER_BYTES + count * 8);
  udp.endPacket();
}

void setup() {
  Serial.begin(115200);
  delay(1500);
#if !defined(ESP8266)
  analogReadResolution(ADC_BITS);
#endif
  setupNotch(NOTCH_HZ, FS, 30.0);
  startWiFi();
  nextSampleUs = micros();
}

void loop() {
  if ((long)(micros() - nextSampleUs) < 0) return;   // wait for the next sample time
  nextSampleUs += PERIOD_US;
  // after a long Wi-Fi stall, resync instead of bursting hundreds of late samples
  if ((long)(micros() - nextSampleUs) > 100000L) nextSampleUs = micros() + PERIOD_US;

  int raw = analogRead(PIN);
  baseline += (raw - baseline) * 0.002f;             // remove slow drift (about 1 s)
  float signal = raw - baseline;                     // biosignal centred at 0
  if (USE_NOTCH) signal = notch(signal);             // remove mains hum
  envelope += (fabsf(signal) - envelope) * 0.02f;    // activity level

  if (inCount == 0) packetFirstIndex = sampleIndex;
  memcpy(packet + HEADER_BYTES + inCount * 8, &signal, 4);
  memcpy(packet + HEADER_BYTES + inCount * 8 + 4, &envelope, 4);
  inCount++;
  sampleIndex++;

  if (inCount >= SAMPLES_PER_PACKET) {
    serviceNetwork();
    if (haveClient) sendPacket();
    inCount = 0;
  }
}
