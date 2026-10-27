// Blazer idle controller - Step 1: read tach pulses and compute RPM
// Board: Uno/Nano (interrupt pins are D2 and D3)
// Input: conditioned HEI tach signal (via optocoupler) on D2
//next step will be to code the temperature sensor so that it can take a differing voltage and output a temp.
//For test make the temperature come out as the voltage of the 
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const byte TACH_PIN = 2;
const byte PULSES_PER_REV = 4;                 // V8, 4-stroke: 8 sparks per 2 revs
const unsigned long MIN_PERIOD_US = 2000UL;    // ignore edges closer than 2 ms (ringing/noise; ~7500 RPM ceiling)
const unsigned long TIMEOUT_US = 500000UL;     // no pulse for 0.5 s -> engine stopped

volatile unsigned long lastEdge = 0;
volatile unsigned long period = 0;
volatile bool newPulse = false;

float rpmSmooth = 0;
unsigned long lastPrint = 0;

const int TEMP_PIN = A0;
const float V_REF = 5.0;

void tachISR() {
  unsigned long now = micros();
  unsigned long dt = now - lastEdge;
  if (dt >= MIN_PERIOD_US) {
    period = dt;
    lastEdge = now;
    newPulse = true;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TACH_PIN, INPUT_PULLUP);
  // Optocoupler inverts the signal. If readings are jumpy, try RISING.
  attachInterrupt(digitalPinToInterrupt(TACH_PIN), tachISR, FALLING);
  Wire.begin();
  Wire.setWireTimeout(3000, true);   // give up after 3 ms and reset the I2C hardware
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);

}

void loop() {
  unsigned long p, last;
  bool fresh;

  // Copy the shared variables atomically
  noInterrupts();
  p = period;
  last = lastEdge;
  fresh = newPulse;
  newPulse = false;
  interrupts();

  if (fresh && p < TIMEOUT_US) {
    float rpm = 60000000.0 / ((float)p * PULSES_PER_REV);
    if (rpmSmooth == 0) rpmSmooth = rpm;
    else rpmSmooth += 0.25 * (rpm - rpmSmooth);   // light smoothing; raise 0.25 for faster response
  }

  if (micros() - last > TIMEOUT_US) rpmSmooth = 0;  // engine off

  if (millis() - lastPrint >= 200) {
      int rawADC = analogRead(TEMP_PIN);
      float voltage = (rawADC * V_REF) / 1023.0;
    lastPrint = millis();
    Serial.print("RPM: ");
    Serial.println((int)rpmSmooth);
    Serial.print("Temp V: ");
    Serial.println(voltage,2);
    static int lastShown = -1;
    int shown = (int)rpmSmooth;
    if (shown != lastShown) {
      lastShown = shown;
      lcd.setCursor(0, 1);
      lcd.print("RPM: ");
      lcd.print(shown);
      lcd.print("    ");
    }

  }


}