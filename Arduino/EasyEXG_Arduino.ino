// EasyEXG -> Arduino: read EMG / ECG / EOG on A0 and show it in the Serial Plotter
//
// Wiring (J3 on EasyEXG -> Arduino):  GND -> GND,  VCC -> 5V (or 3.3V),  OUT -> A0,  VREF not connected
// Gain jumper JP1: open for EMG / EOG (221x), bridged for ECG / EEG (1048x)
// Open Tools > Serial Plotter, 115200 baud.
// Safety: power the Arduino from a laptop running on battery (unplugged) while electrodes are on a person.

const int PIN = A0;
const long BAUD = 115200;
const int FS = 500;                                // samples per second
const unsigned long PERIOD_US = 1000000UL / FS;

float baseline = 512;   // tracks the resting level (VCC/2, about 512 counts)
float envelope = 0;     // smoothed EMG amplitude

void setup() {
  Serial.begin(BAUD);
}

void loop() {
  static unsigned long next = micros();
  if ((long)(micros() - next) < 0) return;         // wait for the next sample time
  next += PERIOD_US;

  int raw = analogRead(PIN);
  baseline += (raw - baseline) * 0.002f;           // slow drift removal (~1 s)
  float signal = raw - baseline;                   // biosignal centred at 0
  envelope += (fabs(signal) - envelope) * 0.02f;   // muscle activity level (EMG)

  Serial.print(signal);                            // trace 1: raw signal
  Serial.print(' ');
  Serial.println(envelope);                        // trace 2: envelope
}
