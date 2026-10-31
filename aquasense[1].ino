/*
  AquaSense: reference implementation
  ------------------------------------
  Team Aquawarriors, Youth Ideathon 2024

  Our pitch proposed two parts on one microcontroller:
    1. A capacitive soil moisture sensor that tells farmers when the field
       actually needs water, and switches irrigation on and off.
    2. A pressure sensor on the water pipe that detects a sudden pressure
       drop, which usually means a leak, and raises an alert.

  This sketch was written after the competition to show how the design
  would work. The threshold values are placeholders and need calibrating
  with real sensors.

  Wiring (Arduino Uno)
    A0  Capacitive soil moisture sensor (analog out)
    A1  Pressure transducer, 0.5-4.5 V output
    D7  Relay module that drives the irrigation pump / valve
    D8  Buzzer (leak alarm)
    D13 Built-in LED (on while irrigating)
*/

const int MOISTURE_PIN = A0;
const int PRESSURE_PIN = A1;
const int RELAY_PIN    = 7;
const int BUZZER_PIN   = 8;
const int LED_PIN      = 13;

// --- Soil moisture calibration ---------------------------------------
// Capacitive sensors read HIGHER when the soil is DRIER.
// Measure these two numbers with your own sensor:
const int DRY_READING = 600;   // sensor held in dry air
const int WET_READING = 280;   // sensor dipped in water

// Irrigation turns ON below START and OFF above STOP.
// The gap between them (hysteresis) stops the pump flickering on and off.
const int MOISTURE_START_PERCENT = 35;
const int MOISTURE_STOP_PERCENT  = 60;

// --- Leak detection ---------------------------------------------------
const float LEAK_DROP_PERCENT  = 25.0;  // a drop this big below normal = suspected leak
const int   LEAK_CONFIRM_COUNT = 5;     // must last this many readings in a row
const float BASELINE_SMOOTHING = 0.05;  // how fast "normal pressure" adapts

const unsigned long READ_INTERVAL_MS = 2000;

bool  irrigating     = false;
bool  leakAlarm      = false;
float baselinePsi    = -1;  // -1 means "not measured yet"
int   lowPressureRun = 0;

// Converts the raw sensor value into 0% (bone dry) .. 100% (in water).
int moisturePercent(int raw) {
  int percent = map(raw, DRY_READING, WET_READING, 0, 100);
  return constrain(percent, 0, 100);
}

// Converts the transducer voltage (0.5 V = 0 psi, 4.5 V = 100 psi) to psi.
float readPressurePsi() {
  float volts = analogRead(PRESSURE_PIN) * (5.0 / 1023.0);
  float psi = (volts - 0.5) * (100.0 / 4.0);
  return max(psi, 0.0f);
}

void setIrrigation(bool on) {
  irrigating = on;
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

void checkSoil() {
  int moisture = moisturePercent(analogRead(MOISTURE_PIN));

  if (!irrigating && moisture < MOISTURE_START_PERCENT) {
    setIrrigation(true);
  } else if (irrigating && moisture > MOISTURE_STOP_PERCENT) {
    setIrrigation(false);
  }

  Serial.print("Moisture: ");
  Serial.print(moisture);
  Serial.print("%  Irrigation: ");
  Serial.println(irrigating ? "ON" : "OFF");
}

void checkPipe() {
  float psi = readPressurePsi();

  // First reading becomes the starting "normal" pressure.
  if (baselinePsi < 0) baselinePsi = psi;

  float dropPercent = 0;
  if (baselinePsi > 0) {
    dropPercent = (baselinePsi - psi) / baselinePsi * 100.0;
  }

  if (dropPercent > LEAK_DROP_PERCENT) {
    lowPressureRun++;
  } else {
    lowPressureRun = 0;
    // Only learn "normal" from normal readings, so a leak
    // does not slowly become the new baseline.
    baselinePsi += BASELINE_SMOOTHING * (psi - baselinePsi);
  }

  leakAlarm = (lowPressureRun >= LEAK_CONFIRM_COUNT);
  digitalWrite(BUZZER_PIN, leakAlarm ? HIGH : LOW);

  Serial.print("Pressure: ");
  Serial.print(psi, 1);
  Serial.print(" psi (normal ");
  Serial.print(baselinePsi, 1);
  Serial.print(")  ");
  Serial.println(leakAlarm ? "LEAK SUSPECTED - alert sent" : "Pipe OK");
}

void setup() {
  Serial.begin(9600);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  setIrrigation(false);
  Serial.println("AquaSense started");
}

void loop() {
  checkSoil();
  checkPipe();
  Serial.println("---");
  delay(READ_INTERVAL_MS);
}
