#include <Arduino.h>
#include <Wire.h>
#include <DHTesp.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// Reference pin map. Change these constants if your existing Wokwi wiring differs.
constexpr uint8_t DHT_PIN = 18;
constexpr uint8_t SKIN_TEMP_PIN = 4;
constexpr uint8_t HEART_RATE_PIN = 34;
constexpr uint8_t LED_PIN = 26;       // Prototype substitute for vibration motor
constexpr uint8_t BUZZER_PIN = 12;    // Prototype substitute for voice/audio module
constexpr uint8_t BUTTON_PIN = 27;
constexpr uint8_t I2C_SDA_PIN = 21;
constexpr uint8_t I2C_SCL_PIN = 22;

constexpr unsigned long SAMPLE_MS = 2000;
constexpr unsigned long RESPONSE_TIMEOUT_MS = 15000; // shortened for simulation

DHTesp dht;
OneWire oneWire(SKIN_TEMP_PIN);
DallasTemperature skinSensor(&oneWire);
Adafruit_MPU6050 mpu;

enum class RiskLevel { LOW, MODERATE, HIGH };

struct Reading {
  float airC;
  float humidity;
  float skinC;
  float heartBpm;
  float movementG;
  float exposureMinutes;
};

struct Assessment {
  int score;
  RiskLevel level;
  String factors;
};

struct SimulationOverride {
  bool active = false;
  Reading value{};
} simulation;

unsigned long lastSample = 0;
unsigned long hotSince = 0;
unsigned long highRiskSince = 0;
bool caregiverAlertSent = false;

const char *riskName(RiskLevel level) {
  switch (level) {
    case RiskLevel::LOW: return "LOW";
    case RiskLevel::MODERATE: return "MODERATE";
    default: return "HIGH";
  }
}

// NOAA Rothfusz heat-index equation. Below 26.7 C, air temperature is returned.
float heatIndexC(float tC, float rh) {
  if (tC < 26.7f) return tC;
  const float t = tC * 9.0f / 5.0f + 32.0f;
  float hi = -42.379f + 2.04901523f * t + 10.14333127f * rh
           - 0.22475541f * t * rh - 0.00683783f * t * t
           - 0.05481717f * rh * rh + 0.00122874f * t * t * rh
           + 0.00085282f * t * rh * rh
           - 0.00000199f * t * t * rh * rh;
  return (hi - 32.0f) * 5.0f / 9.0f;
}

void addFactor(String &factors, const String &text) {
  if (factors.length()) factors += "; ";
  factors += text;
}

Assessment assessRisk(const Reading &r) {
  Assessment a{0, RiskLevel::LOW, "none"};
  a.factors = "";
  const float hi = heatIndexC(r.airC, r.humidity);

  if (hi >= 39.0f) { a.score += 3; addFactor(a.factors, "heat index >=39C (+3)"); }
  else if (hi >= 32.0f) { a.score += 2; addFactor(a.factors, "heat index 32-38.9C (+2)"); }
  else if (hi >= 27.0f) { a.score += 1; addFactor(a.factors, "heat index 27-31.9C (+1)"); }

  if (r.skinC >= 36.0f) { a.score += 3; addFactor(a.factors, "skin temp >=36C (+3)"); }
  else if (r.skinC >= 35.0f) { a.score += 2; addFactor(a.factors, "skin temp 35-35.9C (+2)"); }
  else if (r.skinC >= 34.0f) { a.score += 1; addFactor(a.factors, "skin temp 34-34.9C (+1)"); }

  if (r.heartBpm >= 120.0f) { a.score += 3; addFactor(a.factors, "heart rate >=120 (+3)"); }
  else if (r.heartBpm >= 100.0f) { a.score += 2; addFactor(a.factors, "heart rate 100-119 (+2)"); }
  else if (r.heartBpm >= 90.0f) { a.score += 1; addFactor(a.factors, "heart rate 90-99 (+1)"); }

  if (hi >= 32.0f && r.movementG >= 0.20f) {
    a.score += 1; addFactor(a.factors, "activity in heat (+1)");
  }
  if (hi >= 32.0f && r.movementG < 0.03f) {
    a.score += 1; addFactor(a.factors, "very low movement in heat (+1)");
  }
  if (r.exposureMinutes >= 30.0f) {
    a.score += 2; addFactor(a.factors, "heat exposure >=30 min (+2)");
  } else if (r.exposureMinutes >= 10.0f) {
    a.score += 1; addFactor(a.factors, "heat exposure >=10 min (+1)");
  }

  // Prototype classification: LOW 0-3, MODERATE 4-6, HIGH 7+.
  if (a.score >= 7 || hi >= 54.0f || r.skinC >= 37.5f) a.level = RiskLevel::HIGH;
  else if (a.score >= 4) a.level = RiskLevel::MODERATE;
  if (!a.factors.length()) a.factors = "none";
  return a;
}

float readMovementG() {
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);
  const float magnitude = sqrtf(accel.acceleration.x * accel.acceleration.x
                              + accel.acceleration.y * accel.acceleration.y
                              + accel.acceleration.z * accel.acceleration.z) / 9.80665f;
  return fabsf(magnitude - 1.0f);
}

Reading readSensors() {
  if (simulation.active) return simulation.value;
  TempAndHumidity env = dht.getTempAndHumidity();
  skinSensor.requestTemperatures();
  const int rawHeart = analogRead(HEART_RATE_PIN);
  const float heart = 40.0f + (rawHeart / 4095.0f) * 140.0f;
  const float movement = readMovementG();
  const float hi = heatIndexC(env.temperature, env.humidity);
  if (hi >= 32.0f) {
    if (!hotSince) hotSince = millis();
  } else {
    hotSince = 0;
  }
  const float exposure = hotSince ? (millis() - hotSince) / 60000.0f : 0.0f;
  return {env.temperature, env.humidity, skinSensor.getTempCByIndex(0), heart, movement, exposure};
}

void tonePattern(RiskLevel level) {
  if (level == RiskLevel::LOW) {
    digitalWrite(LED_PIN, LOW);
    noTone(BUZZER_PIN);
  } else if (level == RiskLevel::MODERATE) {
    digitalWrite(LED_PIN, HIGH);
    tone(BUZZER_PIN, 880, 180);
  } else {
    digitalWrite(LED_PIN, (millis() / 250) % 2);
    tone(BUZZER_PIN, 1200, 220);
  }
}

const char *voiceMessage(RiskLevel level) {
  if (level == RiskLevel::MODERATE)
    return "Please move somewhere cool and drink water. Press the button when safe.";
  if (level == RiskLevel::HIGH)
    return "Dangerous heat detected. Go to a cool place now. Press the button. Help may be contacted.";
  return "No heat warning.";
}

void printReading(const Reading &r, const Assessment &a) {
  Serial.println("--- MIMAMORI STATUS ---");
  Serial.printf("Air: %.1f C | Humidity: %.1f %% | Heat index: %.1f C\n",
                r.airC, r.humidity, heatIndexC(r.airC, r.humidity));
  Serial.printf("Skin: %.1f C | Heart: %.0f BPM | Movement: %.2f g | Exposure: %.1f min\n",
                r.skinC, r.heartBpm, r.movementG, r.exposureMinutes);
  Serial.printf("Risk: %s | Score: %d | Factors: %s\n", riskName(a.level), a.score, a.factors.c_str());
  Serial.printf("Voice: %s\n", voiceMessage(a.level));
}

void handleResponseAndEscalation(RiskLevel level) {
  if (level == RiskLevel::HIGH) {
    if (!highRiskSince) highRiskSince = millis();
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("RESPONSE RECEIVED: wearer pressed the safety button.");
      highRiskSince = 0;
      caregiverAlertSent = false;
    } else if (!caregiverAlertSent && millis() - highRiskSince >= RESPONSE_TIMEOUT_MS) {
      Serial.println("CAREGIVER ALERT: high risk and no wearer response.");
      caregiverAlertSent = true;
    }
  } else {
    highRiskSince = 0;
    caregiverAlertSent = false;
  }
}

// Serial simulation command:
// SIM airC,humidity,heartBpm,skinC,movementG,exposureMinutes
// Example: SIM 38,70,125,36.2,0.25,35
// Send LIVE to return to physical/simulated Wokwi sensors.
void processSerialCommand() {
  if (!Serial.available()) return;
  String command = Serial.readStringUntil('\n');
  command.trim();
  if (command.equalsIgnoreCase("LIVE")) {
    simulation.active = false;
    Serial.println("Simulation override disabled.");
    return;
  }
  if (!command.startsWith("SIM ")) return;
  Reading r{};
  if (sscanf(command.c_str() + 4, "%f,%f,%f,%f,%f,%f", &r.airC, &r.humidity,
             &r.heartBpm, &r.skinC, &r.movementG, &r.exposureMinutes) == 6) {
    simulation.value = r;
    simulation.active = true;
    Serial.println("Simulation override accepted.");
  } else {
    Serial.println("Invalid command. Use: SIM airC,humidity,heartBpm,skinC,movementG,exposureMinutes");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  dht.setup(DHT_PIN, DHTesp::DHT22);
  skinSensor.begin();
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!mpu.begin()) Serial.println("WARNING: MPU6050 not detected; verify SDA/SCL wiring.");
  else mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.println("Mimamori prototype ready. Enter SIM ... or LIVE.");
}

void loop() {
  processSerialCommand();
  if (millis() - lastSample >= SAMPLE_MS) {
    lastSample = millis();
    Reading reading = readSensors();
    Assessment assessment = assessRisk(reading);
    printReading(reading, assessment);
    tonePattern(assessment.level);
    handleResponseAndEscalation(assessment.level);
  }
}
