#include <Wire.h>
#include <ESP32Servo.h>

// ===== IMU ADDRESSES =====
#define WRIST_IMU 0x68
#define KNUCKLE_IMU 0x69

#define SERVO_PIN 18

Servo myServo;

// ===== TIMING =====
unsigned long lastSample = 0;
const int sampleInterval = 10; // 100 Hz

// ===== OFFSETS =====
float wristOffset = 0;
float knuckleOffset = 0;

// ===== FILTER =====
float wristSmooth = 0;
float knuckleSmooth = 0;
float alpha = 0.2;

// ===== CHANGE DETECTION =====
float previousSignal = 0;
float changeThreshold = 1.5;   // Sensitivity for drastic change
bool rapidChangeOngoing = false;
bool motorActive = false;

unsigned long rapidStartTime = 0;
unsigned long lastRapidSeen = 0;

const unsigned long confirmTime = 400; // 0.4 sec confirmation

void setup() {

  Serial.begin(115200);
  delay(2000);

  Wire.begin(8, 9);
  Wire.setClock(100000);
  delay(100);

  initIMU(WRIST_IMU);
  initIMU(KNUCKLE_IMU);

  myServo.attach(SERVO_PIN);
  myServo.write(0);

  calibrate();

  Serial.println("SYSTEM READY");
}

void loop() {

  if (millis() - lastSample < sampleInterval) return;
  lastSample += sampleInterval;

  // ===== READ IMUs =====
  float wristGyro = readGyroX(WRIST_IMU) - wristOffset;
  float knuckleGyro = readGyroX(KNUCKLE_IMU) - knuckleOffset;

  wristGyro /= 131.0;
  knuckleGyro /= 131.0;

  // ===== SMOOTHING =====
  wristSmooth = alpha * wristGyro + (1 - alpha) * wristSmooth;
  knuckleSmooth = alpha * knuckleGyro + (1 - alpha) * knuckleSmooth;

  float tremorSignal = wristSmooth - knuckleSmooth;

  // ===== RATE OF CHANGE =====
  float delta = abs(tremorSignal - previousSignal);
  previousSignal = tremorSignal;

  Serial.print("Signal: ");
  Serial.print(tremorSignal, 2);
  Serial.print(" | Delta: ");
  Serial.println(delta, 2);

  bool rapidChange = (delta > changeThreshold);

  // ===== 0.4 sec CONTINUOUS CHECK =====
  if (rapidChange) {

    lastRapidSeen = millis();

    if (!rapidChangeOngoing) {
      rapidChangeOngoing = true;
      rapidStartTime = millis();
    }
  }
  else {
    rapidChangeOngoing = false;
  }

  // ===== CONFIRM ROTATION =====
  if (rapidChangeOngoing && !motorActive) {

    if (millis() - rapidStartTime >= confirmTime) {
      myServo.write(180);
      motorActive = true;
      Serial.println("RAPID CHANGE CONFIRMED - MOTOR 180");
    }
  }

  // ===== RELEASE WHEN RAPID CHANGE STOPS =====
  if (motorActive) {

    if (millis() - lastRapidSeen > 300) {
      myServo.write(0);
      motorActive = false;
      Serial.println("CHANGE STOPPED - MOTOR 0");
    }
  }
}

// ================= FUNCTIONS =================

void initIMU(uint8_t addr) {
  writeReg(addr, 0x6B, 0x00);
  delay(50);
  writeReg(addr, 0x1B, 0x00);
}

int16_t readGyroX(uint8_t addr) {

  Wire.beginTransmission(addr);
  Wire.write(0x43);
  Wire.endTransmission(false);
  Wire.requestFrom(addr, 2);

  if (Wire.available() < 2) return 0;

  return Wire.read() << 8 | Wire.read();
}

void writeReg(uint8_t addr, uint8_t reg, uint8_t data) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

void calibrate() {

  Serial.println("CALIBRATING - KEEP STILL");

  wristOffset = 0;
  knuckleOffset = 0;

  for (int i = 0; i < 2000; i++) {
    wristOffset += readGyroX(WRIST_IMU);
    knuckleOffset += readGyroX(KNUCKLE_IMU);
    delay(2);
  }

  wristOffset /= 2000.0;
  knuckleOffset /= 2000.0;

  Serial.println("CALIBRATION DONE");
}
