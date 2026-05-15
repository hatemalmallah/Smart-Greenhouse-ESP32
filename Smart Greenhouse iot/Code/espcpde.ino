
#define BLYNK_TEMPLATE_ID   "YOUR_BLYNK_ID"
#define BLYNK_TEMPLATE_NAME "Smart Greenhouse IoT"
#define BLYNK_AUTH_TOKEN    "YOUR_BLYNK_TOKEN"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <Adafruit_BMP280.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>
#include <vector>

char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASSWORD";

#define BOT_TOKEN   "YOUR_TELEGRAM_BOT_TOKEN"
#define DEFAULT_CHAT_ID "YOUR_CHAT_ID"

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

std::vector<String> knownChatIDs = {DEFAULT_CHAT_ID};
bool telegramNeedsReconnect = false;

#define THINGSPEAK_API_KEY   "YOUR_THINGSPEAK_API KEY"
#define THINGSPEAK_URL       "http://api.thingspeak.com/update"
#define THINGSPEAK_INTERVAL  20000UL
#define NETWORK_WARMUP_MS   15000UL
unsigned long lastThingSpeakSend = 0;
unsigned long wifiConnectedAt    = 0;
bool          networkReady       = false;

#define SDA_PIN       21
#define SCL_PIN       22

#define SOIL_PIN      34
#define LDR_PIN       32
#define MQ2_PIN       35

#define COOLING_PIN    25
#define HEATER_PIN     26
#define FAN_PIN        27
#define HUMIDIFIER_PIN 14
#define PUMP_PIN       12
#define GROWLIGHT_PIN  33

#define EMERGENCY_BTN_PIN  13

#define BUZZER_PIN         4

#define VPIN_TEMP         V0
#define VPIN_HUM          V1
#define VPIN_SOIL         V2
#define VPIN_COOLING_SW   V3
#define VPIN_HEATER_SW    V4
#define VPIN_FAN_SW       V5
#define VPIN_HUMIDIFIER_SW V6
#define VPIN_PUMP_SW      V7
#define VPIN_MODE         V8
#define VPIN_EMERGENCY    V9
#define VPIN_RESET        V10
#define VPIN_LIGHT        V11
#define VPIN_GAS          V12
#define VPIN_GROWLIGHT_SW V13

#define TEMP_COOL_ON    30.0f
#define TEMP_COOL_OFF   28.0f
#define TEMP_HEAT_ON    20.0f
#define TEMP_HEAT_OFF   22.0f

#define HUM_FAN_ON      80.0f
#define HUM_FAN_OFF     75.0f
#define HUM_HUMID_ON    50.0f
#define HUM_HUMID_OFF   55.0f

#define SOIL_PUMP_ON    40.0f
#define SOIL_PUMP_OFF   60.0f

#define LIGHT_GROW_ON   65.0f
#define LIGHT_GROW_OFF  75.0f

#define GAS_FAN_ON     2000
#define GAS_FAN_OFF    1500

#define TEMP_MIN_VALID   -40.0f
#define TEMP_MAX_VALID    85.0f
#define HUM_MIN_VALID      0.0f
#define HUM_MAX_VALID    100.0f
#define SOIL_MIN_VALID     0.0f
#define SOIL_MAX_VALID   100.0f

#define SENSOR_FAIL_THRESHOLD  5

#define SENSOR_INTERVAL   2000UL
#define BLYNK_INTERVAL    2000UL
#define TELEGRAM_BOT_POLL 2000UL
#define TELEGRAM_INTERVAL 1000UL
#define WARMUP_DURATION   10000UL

Adafruit_BME680 bme680;
Adafruit_BMP280 bmp280;
BlynkTimer timer;

bool useBME680  = false;
bool useBMP280  = false;
bool useDummy   = false;

#define DUMMY_TEMP       25.0f
#define DUMMY_HUMIDITY   60.0f
#define BMP280_FAKE_HUM  60.0f

LiquidCrystal_I2C lcd(0x27, 16, 2);

int lcdScreen = 0;
unsigned long lastLCDSwitch = 0;

float temperature  = 0.0f;
float humidity     = 0.0f;
float soilMoisture = 0.0f;
float lightLevel   = 0.0f;
int   gasLevel     = 0;

int  bmeFailCount  = 0;
bool bmeCritical   = false;
bool sensorFault   = false;
bool lcdAvailable  = false;

bool coolingState    = false;
bool heaterState     = false;
bool fanState        = false;
bool humidifierState = false;
bool pumpState       = false;
bool growLightState  = false;

bool manCooling    = false;
bool manHeater     = false;
bool manFan        = false;
bool manHumidifier = false;
bool manPump       = false;
bool manGrowLight  = false;

int operatingMode = 0;

volatile bool emergencyActive = false;

unsigned long bootTime       = 0;
bool          warmupComplete = false;
bool          warmupMsgSent  = false;

unsigned long buttonPressStart = 0;
bool buttonWasPressed = false;
#define LONG_PRESS_MS  3000UL

bool alertTempHigh  = false;
bool alertTempLow   = false;
bool alertHumHigh   = false;
bool alertHumLow    = false;
bool alertSoilLow   = false;
bool alertGasHigh   = false;
bool alertLightLow  = false;
bool alertAllNormal = true;

void readSensors();
void sendToBlynk();
void autoControl();
void manualControl();
void applyActuators();
void enforceSafety();
void checkAlerts();
void sendTelegram(const String &msg, const String &parseMode = "");
void setActuator(uint8_t pin, bool state);
void updateLCD();
void emergencyShutdown();
void sendToThingSpeak();
bool validateBME680(float t, float h);
void checkPhysicalEmergency();
void checkTelegramMessages();

BLYNK_WRITE(VPIN_MODE) {
  operatingMode = param.asInt();
  Serial.print("[Blynk] Mode changed → ");
  Serial.println(operatingMode == 0 ? "AUTO" : "MANUAL");
}

BLYNK_WRITE(VPIN_COOLING_SW) {
  manCooling = param.asInt();
  Serial.print("[Blynk] Manual Cooling → ");
  Serial.println(manCooling ? "ON" : "OFF");
}

BLYNK_WRITE(VPIN_HEATER_SW) {
  manHeater = param.asInt();
  Serial.print("[Blynk] Manual Heater → ");
  Serial.println(manHeater ? "ON" : "OFF");
}

BLYNK_WRITE(VPIN_FAN_SW) {
  manFan = param.asInt();
  Serial.print("[Blynk] Manual Fan → ");
  Serial.println(manFan ? "ON" : "OFF");
}

BLYNK_WRITE(VPIN_HUMIDIFIER_SW) {
  manHumidifier = param.asInt();
  Serial.print("[Blynk] Manual Humidifier → ");
  Serial.println(manHumidifier ? "ON" : "OFF");
}

BLYNK_WRITE(VPIN_PUMP_SW) {
  manPump = param.asInt();
  Serial.print("[Blynk] Manual Pump → ");
  Serial.println(manPump ? "ON" : "OFF");
}

BLYNK_WRITE(VPIN_GROWLIGHT_SW) {
  manGrowLight = param.asInt();
  Serial.print("[Blynk] Manual Grow Light → ");
  Serial.println(manGrowLight ? "ON" : "OFF");
}

BLYNK_WRITE(VPIN_EMERGENCY) {
  int val = param.asInt();
  if (val == 1) {
    Serial.println("[EMERGENCY] ⚠️ Blynk V9 emergency button PRESSED!");
    emergencyShutdown();
  }
}

BLYNK_WRITE(VPIN_RESET) {
  int val = param.asInt();
  if (val == 1) {
    Serial.println("[RESET] 🔄 Blynk V10 reset button PRESSED!");
    Serial.println("[RESET] Restarting ESP32 in 1 second...");
    sendTelegram("🔄 SYSTEM RESET requested via Blynk V10.\n"
                 "♻️ ESP32 is restarting now...");
    delay(1000);
    ESP.restart();
  }
}

BLYNK_CONNECTED() {
  Serial.println("[Blynk] Connected — syncing virtual pins…");
  Blynk.syncVirtual(VPIN_MODE);
  Blynk.syncVirtual(VPIN_COOLING_SW);
  Blynk.syncVirtual(VPIN_HEATER_SW);
  Blynk.syncVirtual(VPIN_FAN_SW);
  Blynk.syncVirtual(VPIN_HUMIDIFIER_SW);
  Blynk.syncVirtual(VPIN_PUMP_SW);
  Blynk.syncVirtual(VPIN_GROWLIGHT_SW);
  Blynk.syncVirtual(VPIN_EMERGENCY);

}

void setup() {

  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("========================================");
  Serial.println("   Smart Greenhouse IoT — ESP32");
  Serial.println("   + Safety & Monitoring Layer");
  Serial.println("========================================");

  pinMode(COOLING_PIN,    OUTPUT);
  pinMode(HEATER_PIN,     OUTPUT);
  pinMode(FAN_PIN,        OUTPUT);
  pinMode(HUMIDIFIER_PIN, OUTPUT);
  pinMode(PUMP_PIN,       OUTPUT);
  pinMode(GROWLIGHT_PIN,  OUTPUT);

  digitalWrite(COOLING_PIN,    LOW);
  digitalWrite(HEATER_PIN,     LOW);
  digitalWrite(FAN_PIN,        LOW);
  digitalWrite(HUMIDIFIER_PIN, LOW);
  digitalWrite(PUMP_PIN,       LOW);
  digitalWrite(GROWLIGHT_PIN,  LOW);

  pinMode(EMERGENCY_BTN_PIN, INPUT_PULLUP);
  Serial.println("[OK] Emergency button configured (GPIO 13, INPUT_PULLUP).");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("[OK] Buzzer configured (GPIO 4).");

  Serial.println("[I2C] Starting bus recovery...");
  Serial.flush();

  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, OUTPUT);
  for (int i = 0; i < 16; i++) {
    digitalWrite(SCL_PIN, LOW);
    delayMicroseconds(10);
    digitalWrite(SCL_PIN, HIGH);
    delayMicroseconds(10);
  }

  pinMode(SDA_PIN, OUTPUT);
  digitalWrite(SDA_PIN, LOW);
  delayMicroseconds(10);
  digitalWrite(SCL_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(SDA_PIN, HIGH);
  delayMicroseconds(10);

  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, INPUT_PULLUP);
  delay(5);
  bool sdaOK = digitalRead(SDA_PIN) == HIGH;
  bool sclOK = digitalRead(SCL_PIN) == HIGH;
  Serial.printf("[I2C] Bus check: SDA=%s  SCL=%s\n",
                sdaOK ? "OK(HIGH)" : "STUCK(LOW)",
                sclOK ? "OK(HIGH)" : "STUCK(LOW)");
  Serial.flush();

  if (!sdaOK || !sclOK) {

    Serial.println("[WARN] I2C bus stuck! Sensor hardware fault.");
    Serial.println("[WARN] Skipping sensor detection — using dummy values.");
    Serial.println("Using fallback dummy sensor values");
    Serial.flush();

    useDummy    = true;
    sensorFault = true;
    temperature = DUMMY_TEMP;
    humidity    = DUMMY_HUMIDITY;

    pinMode(SDA_PIN, INPUT_PULLUP);
    pinMode(SCL_PIN, OUTPUT);
    for (int i = 0; i < 32; i++) {
      digitalWrite(SCL_PIN, LOW);
      delayMicroseconds(10);
      digitalWrite(SCL_PIN, HIGH);
      delayMicroseconds(10);
    }
    pinMode(SDA_PIN, OUTPUT);
    digitalWrite(SDA_PIN, LOW);
    delayMicroseconds(10);
    digitalWrite(SCL_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(SDA_PIN, HIGH);
    delay(50);

  } else {

    Serial.println("[I2C] Bus is free — scanning for sensors...");
    Serial.flush();

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setTimeOut(1000);

    bool found0x77 = false;
    bool found0x76 = false;

    Wire.beginTransmission(0x77);
    if (Wire.endTransmission() == 0) found0x77 = true;

    Wire.beginTransmission(0x76);
    if (Wire.endTransmission() == 0) found0x76 = true;

    Serial.printf("[I2C Scan] 0x76=%s  0x77=%s\n",
                  found0x76 ? "FOUND" : "---",
                  found0x77 ? "FOUND" : "---");

    if (found0x77 && bme680.begin(0x77)) {
      useBME680 = true;
      Serial.println("[OK] BME680 detected at 0x77.");
    } else if (found0x76 && bme680.begin(0x76)) {
      useBME680 = true;
      Serial.println("[OK] BME680 detected at 0x76.");
    }

    if (useBME680) {
      bme680.setTemperatureOversampling(BME680_OS_8X);
      bme680.setHumidityOversampling(BME680_OS_2X);
      bme680.setPressureOversampling(BME680_OS_4X);
      bme680.setIIRFilterSize(BME680_FILTER_SIZE_3);
      bme680.setGasHeater(0, 0);
    } else {
      Serial.println("BME680 not detected");

      if ((found0x76 && bmp280.begin(0x76)) || (found0x77 && bmp280.begin(0x77))) {
        useBMP280 = true;
        Serial.println("[OK] BMP280 detected and initialised.");
        Serial.println("[INFO] No humidity sensor — using fixed fake humidity.");

        bmp280.setSampling(Adafruit_BMP280::MODE_NORMAL,
                           Adafruit_BMP280::SAMPLING_X8,
                           Adafruit_BMP280::SAMPLING_X4,
                           Adafruit_BMP280::FILTER_X4,
                           Adafruit_BMP280::STANDBY_MS_500);
      } else {
        Serial.println("BMP280 not detected");
        useDummy = true;
        Serial.println("Using fallback dummy sensor values");
        temperature = DUMMY_TEMP;
        humidity    = DUMMY_HUMIDITY;
      }
    }

    Wire.end();
    delay(50);
  }

  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, INPUT_PULLUP);
  delay(5);
  bool busFreeNow = (digitalRead(SDA_PIN) == HIGH && digitalRead(SCL_PIN) == HIGH);

  if (busFreeNow) {
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setTimeOut(1000);
    Serial.println("[OK] I2C bus ready.");
    Serial.flush();

    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Greenhouse");
    lcd.setCursor(0, 1);
    lcd.print("Starting...");
    lcdAvailable = true;
    Serial.println("[OK] LCD initialised.");
  } else {
    Serial.println("[WARN] I2C bus still stuck — LCD DISABLED.");
    Serial.println("[WARN] System will run without LCD display.");
    lcdAvailable = false;
  }
  Serial.flush();

  secured_client.setInsecure();
  secured_client.setTimeout(10000);

  Serial.print("[WiFi] Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  Blynk.config(BLYNK_AUTH_TOKEN);

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000UL) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] ✅ Connected!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
    Blynk.connect();

    wifiConnectedAt = millis();
    networkReady = false;

    Serial.println("[Telegram] Startup message deferred until warm-up completes.");
  } else {
    Serial.println("[WiFi] ⚠️ Not connected — starting in OFFLINE mode.");
    Serial.println("[WiFi] Sensors, LCD, and emergency button work normally.");
    Serial.println("[WiFi] Cloud services will activate when WiFi connects.");
  }

  timer.setInterval(SENSOR_INTERVAL,   readSensors);
  timer.setInterval(BLYNK_INTERVAL,    sendToBlynk);
  timer.setInterval(TELEGRAM_INTERVAL, checkAlerts);
  timer.setInterval(1000L,             updateLCD);
  timer.setInterval(100L,              checkPhysicalEmergency);

  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);
  Serial.println("[OK] Watchdog Timer armed (30 s timeout).");

  bootTime = millis();
  warmupComplete = false;
  warmupMsgSent  = false;
  Serial.println("[OK] Sensor warm-up started (10 seconds)...");
  Serial.println("[OK] Setup complete — entering main loop.\n");
}

void loop() {

  esp_task_wdt_reset();

  if (!warmupComplete) {
    if (millis() - bootTime >= WARMUP_DURATION) {
      warmupComplete = true;
      Serial.println("[OK] ★ Sensor warm-up complete — automation ENABLED.");
    }
  }

  if (warmupComplete && !warmupMsgSent && WiFi.status() == WL_CONNECTED && networkReady) {
    warmupMsgSent = true;
    sendTelegram("\xE2\x9C\x85 System Online & Sensors Stabilized!");

    String msg = "📊 SENSOR STATUS\n\n";
    msg += "🌡️ Temperature: " + String(temperature, 1) + " °C\n";
    msg += "💧 Humidity: " + String(humidity, 1) + " %\n";
    msg += "🌱 Soil Moisture: " + String(soilMoisture, 0) + " %\n";
    msg += "☀️ Light Level: " + String(lightLevel, 0) + " %\n";
    msg += "💨 Gas/Smoke: " + String(gasLevel) + " (Raw)\n\n";
    msg += "⚙️ Mode: " + String(operatingMode == 0 ? "AUTO" : "MANUAL");
    sendTelegram(msg, "");
  }

  if (emergencyActive) {

    digitalWrite(COOLING_PIN,    LOW);
    digitalWrite(HEATER_PIN,     LOW);
    digitalWrite(FAN_PIN,        LOW);
    digitalWrite(HUMIDIFIER_PIN, LOW);
    digitalWrite(PUMP_PIN,       LOW);
    digitalWrite(GROWLIGHT_PIN,  LOW);

    static unsigned long lastBeep = 0;
    static bool beepState = false;
    if (millis() - lastBeep >= 500) {
      lastBeep = millis();
      beepState = !beepState;
      digitalWrite(BUZZER_PIN, beepState ? HIGH : LOW);
    }

    timer.run();
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {

    if (!networkReady) {
      if (wifiConnectedAt == 0) wifiConnectedAt = millis();
      if (millis() - wifiConnectedAt >= NETWORK_WARMUP_MS) {
        networkReady = true;
        Serial.println("[WiFi] ★ Network warmup complete — enabling cloud services.");
      }
    }

    if (!Blynk.connected()) {
      esp_task_wdt_reset();
      Blynk.connect();
    }
    Blynk.run();
  } else {

    networkReady = false;
    wifiConnectedAt = 0;
    static unsigned long lastWiFiRetry = 0;
    if (millis() - lastWiFiRetry > 30000UL) {
      lastWiFiRetry = millis();
      Serial.println("[WiFi] ⚠️ Disconnected — retrying...");
      WiFi.begin(ssid, pass);
    }
  }

  timer.run();

  if (WiFi.status() == WL_CONNECTED && networkReady) {

    unsigned long now = millis();
    if (now - lastThingSpeakSend >= THINGSPEAK_INTERVAL) {
      lastThingSpeakSend = now;
      esp_task_wdt_reset();
      sendToThingSpeak();
    }

    static unsigned long lastBotPoll = 0;
    if (millis() - lastBotPoll >= TELEGRAM_BOT_POLL) {
      lastBotPoll = millis();
      esp_task_wdt_reset();
      checkTelegramMessages();
    }
  }
}

void checkPhysicalEmergency() {
  bool pressed = (digitalRead(EMERGENCY_BTN_PIN) == LOW);

  if (pressed && !buttonWasPressed) {

    buttonWasPressed = true;
    buttonPressStart = millis();
  }
  else if (pressed && buttonWasPressed) {

    unsigned long holdTime = millis() - buttonPressStart;

    if (holdTime >= LONG_PRESS_MS) {

      Serial.println("[RESET] 🔄 Long press detected (GPIO 13) — restarting ESP32!");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("  RESTARTING  ");
      lcd.setCursor(0, 1);
      lcd.print("  Please wait ");

      sendTelegram("🔄 SYSTEM RESET via physical button (GPIO 13).\n"
                   "♻️ ESP32 is restarting now...");

      delay(500);
      ESP.restart();
    }
  }
  else if (!pressed && buttonWasPressed) {

    buttonWasPressed = false;
    unsigned long holdTime = millis() - buttonPressStart;

    if (holdTime < LONG_PRESS_MS) {

      if (!emergencyActive) {
        Serial.println("[EMERGENCY] ⚠️ Short press detected (GPIO 13) — EMERGENCY!");
        emergencyShutdown();
      } else {
        Serial.println("[EMERGENCY] Already in emergency state. Hold 3s to restart.");
      }
    }
  }
}

void emergencyShutdown() {
  emergencyActive = true;

  coolingState    = false;
  heaterState     = false;
  fanState        = false;
  humidifierState = false;
  pumpState       = false;
  growLightState  = false;

  digitalWrite(COOLING_PIN,    LOW);
  digitalWrite(HEATER_PIN,     LOW);
  digitalWrite(FAN_PIN,        LOW);
  digitalWrite(HUMIDIFIER_PIN, LOW);
  digitalWrite(PUMP_PIN,       LOW);
  digitalWrite(GROWLIGHT_PIN,  LOW);

  digitalWrite(BUZZER_PIN, HIGH);

  Serial.println("╔══════════════════════════════════════╗");
  Serial.println("║   ⚠️  EMERGENCY SHUTDOWN ACTIVE  ⚠️   ║");
  Serial.println("║   All actuators OFF — SAFE STATE     ║");
  Serial.println("║   RESET ESP32 to resume operation    ║");
  Serial.println("╚══════════════════════════════════════╝");

  if (lcdAvailable) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("!! EMERGENCY !!");
    lcd.setCursor(0, 1);
    lcd.print("ALL SYSTEMS OFF");
  }

  sendTelegram("🚨🚨 EMERGENCY SHUTDOWN ACTIVATED! 🚨🚨\n"
               "⛔ ALL actuators have been turned OFF.\n"
               "🔒 System is in SAFE STATE.\n"
               "⚠️ Manual ESP32 RESET required to resume.\n\n"
               "Trigger: " + String(digitalRead(EMERGENCY_BTN_PIN) == LOW ?
               "Physical Button (GPIO 13)" : "Software (Blynk V9 / Sensor Fault)"));
}

bool validateBME680(float t, float h) {

  if (isnan(t) || isnan(h)) {
    return false;
  }

  if (t < TEMP_MIN_VALID || t > TEMP_MAX_VALID) {
    Serial.printf("[SENSOR] ⚠️ Temperature %.1f°C out of valid range [%.0f, %.0f]\n",
                  t, TEMP_MIN_VALID, TEMP_MAX_VALID);
    return false;
  }

  if (h < HUM_MIN_VALID || h > HUM_MAX_VALID) {
    Serial.printf("[SENSOR] ⚠️ Humidity %.1f%% out of valid range [%.0f, %.0f]\n",
                  h, HUM_MIN_VALID, HUM_MAX_VALID);
    return false;
  }

  return true;
}

void readSensors() {

  if (emergencyActive) {

    coolingState = heaterState = fanState = humidifierState = pumpState = growLightState = false;
    applyActuators();
    return;
  }

  if (!warmupComplete) {

    if (useBME680) {
      if (bme680.performReading()) {
        temperature = bme680.temperature;
        humidity    = bme680.humidity;
      }
    } else if (useBMP280) {
      temperature = bmp280.readTemperature();
      humidity    = BMP280_FAKE_HUM;
    }

    int rawSoil = 0;
    for (int i = 0; i < 10; i++) { rawSoil += analogRead(SOIL_PIN); delay(2); }
    rawSoil /= 10;
    soilMoisture = map(rawSoil, 4095, 0, 0, 100);
    soilMoisture = constrain(soilMoisture, 0.0f, 100.0f);
    int rawLDR = analogRead(LDR_PIN);
    lightLevel = map(rawLDR, 0, 4095, 100, 0);
    lightLevel = constrain(lightLevel, 0.0f, 100.0f);
    gasLevel = analogRead(MQ2_PIN);
    Serial.printf("[Warmup] Sensors stabilizing... T=%.1f H=%.1f S=%.0f L=%.0f G=%d\n",
                  temperature, humidity, soilMoisture, lightLevel, gasLevel);
    return;
  }

  if (useBME680) {
    if (bme680.performReading()) {
      float t = bme680.temperature;
      float h = bme680.humidity;

      if (validateBME680(t, h)) {
        temperature  = t;
        humidity     = h;
        bmeFailCount = 0;
        bmeCritical  = false;
      } else {
        bmeFailCount++;
        Serial.printf("[SENSOR] ⚠️ BME680 invalid data (fail #%d/%d)\n",
                      bmeFailCount, SENSOR_FAIL_THRESHOLD);
      }
    } else {
      bmeFailCount++;
      Serial.printf("[WARN] BME680 read failed (fail #%d/%d) — using last good values.\n",
                    bmeFailCount, SENSOR_FAIL_THRESHOLD);
    }

    if (bmeFailCount >= SENSOR_FAIL_THRESHOLD && !bmeCritical) {
      bmeCritical = true;
      sensorFault = true;
      Serial.println("[WARN] ⚠️ BME680 critical failure — switching to safe dummy values.");

      useBME680 = false;
      useDummy  = true;
      temperature = DUMMY_TEMP;
      humidity    = DUMMY_HUMIDITY;

      sendTelegram("⚠️ BME680 sensor failure detected!\n"
                   "Failed " + String(SENSOR_FAIL_THRESHOLD) + " consecutive reads.\n"
                   "📡 Switched to safe dummy values.\n"
                   "🔧 Check sensor wiring when possible.");
    }

  } else if (useBMP280) {
    temperature = bmp280.readTemperature();
    humidity    = BMP280_FAKE_HUM;

  } else {

    temperature = DUMMY_TEMP;
    humidity    = DUMMY_HUMIDITY;
  }

  int rawSoil = 0;
  for (int i = 0; i < 10; i++) {
    rawSoil += analogRead(SOIL_PIN);
    delay(2);
  }
  rawSoil /= 10;
  soilMoisture = map(rawSoil, 4095, 0, 0, 100);
  soilMoisture = constrain(soilMoisture, 0.0f, 100.0f);

  int rawLDR = analogRead(LDR_PIN);
  lightLevel = map(rawLDR, 0, 4095, 100, 0);
  lightLevel = constrain(lightLevel, 0.0f, 100.0f);
  Serial.printf("[LDR] Raw ADC=%d  Mapped=%.0f%%\n", rawLDR, lightLevel);

  gasLevel = analogRead(MQ2_PIN);

  const char* sensorSrc = useBME680 ? "BME680" : (useBMP280 ? "BMP280" : "DUMMY");
  Serial.printf("[Sensor:%s] T=%.1f°C  H=%.1f%%  Soil=%.0f%%  Light=%.0f%%  Gas=%d\n",
                sensorSrc, temperature, humidity, soilMoisture, lightLevel, gasLevel);

  if (operatingMode == 0) {
    autoControl();
  } else {
    manualControl();
  }

  enforceSafety();

  applyActuators();
}

void sendToBlynk() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  Blynk.virtualWrite(VPIN_TEMP, temperature);
  Blynk.virtualWrite(VPIN_HUM,  humidity);
  Blynk.virtualWrite(VPIN_SOIL, soilMoisture);
  Blynk.virtualWrite(VPIN_LIGHT, lightLevel);
  Blynk.virtualWrite(VPIN_GAS,   gasLevel);

}

void sendToThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ThingSpeak] Skipped — WiFi disconnected.");
    return;
  }

  if (sensorFault || emergencyActive) {
    Serial.println("[ThingSpeak] Skipped — sensor fault or emergency active.");
    return;
  }

  esp_task_wdt_reset();

  HTTPClient http;

  http.begin(THINGSPEAK_URL);
  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String postData = "api_key=" + String(THINGSPEAK_API_KEY) +
                    "&field1=" + String(temperature, 1) +
                    "&field2=" + String(humidity, 1) +
                    "&field3=" + String(soilMoisture, 0) +
                    "&field4=" + String(lightLevel, 0) +
                    "&field5=" + String(gasLevel);

  int httpCode = http.POST(postData);
  esp_task_wdt_reset();
  String response = http.getString();

  if (httpCode == 200) {
    Serial.printf("[ThingSpeak] ✅ Data sent (entry #%s) — T:%.1f H:%.1f S:%.0f\n",
                  response.c_str(), temperature, humidity, soilMoisture);
  } else {
    Serial.printf("[ThingSpeak] ❌ Send failed (HTTP %d, response: %s)\n",
                  httpCode, response.c_str());

  }

  http.end();
}

void autoControl() {

  if (emergencyActive) return;

  if (temperature >= TEMP_COOL_ON) {
    coolingState = true;
  } else if (temperature <= TEMP_COOL_OFF) {
    coolingState = false;
  }

  if (temperature <= TEMP_HEAT_ON) {
    heaterState = true;
  } else if (temperature >= TEMP_HEAT_OFF) {
    heaterState = false;
  }

  if (humidity >= HUM_FAN_ON) {
    fanState = true;
  } else if (humidity <= HUM_FAN_OFF) {
    fanState = false;
  }

  if (humidity <= HUM_HUMID_ON) {
    humidifierState = true;
  } else if (humidity >= HUM_HUMID_OFF) {
    humidifierState = false;
  }

  if (soilMoisture <= SOIL_PUMP_ON) {
    pumpState = true;
  } else if (soilMoisture >= SOIL_PUMP_OFF) {
    pumpState = false;
  }

  if (gasLevel >= GAS_FAN_ON) {
    fanState = true;
  }

  if (lightLevel <= LIGHT_GROW_ON) {
    growLightState = true;
  } else if (lightLevel >= LIGHT_GROW_OFF) {
    growLightState = false;
  }

  bool tempNormal = (temperature > TEMP_HEAT_OFF && temperature < TEMP_COOL_OFF);
  bool humNormal  = (humidity > HUM_HUMID_OFF && humidity < HUM_FAN_OFF);
  bool soilNormal = (soilMoisture > SOIL_PUMP_ON && soilMoisture < SOIL_PUMP_OFF);
  bool gasNormal  = (gasLevel < GAS_FAN_OFF);

  if (tempNormal && humNormal && soilNormal && gasNormal) {
    coolingState    = false;
    heaterState     = false;
    fanState        = false;
    humidifierState = false;
    pumpState       = false;

  }
}

void manualControl() {

  if (emergencyActive) return;

  coolingState    = manCooling;
  heaterState     = manHeater;
  fanState        = manFan;
  humidifierState = manHumidifier;
  pumpState       = manPump;
  growLightState  = manGrowLight;
}

void enforceSafety() {

  if (coolingState && heaterState) {
    heaterState = false;
    Serial.println("[SAFETY] Conflict! Heater disabled — Cooling has priority.");
  }

  if (fanState && humidifierState) {
    humidifierState = false;
    Serial.println("[SAFETY] Conflict! Humidifier disabled — Fan has priority.");
  }
}

void applyActuators() {

  if (emergencyActive) {
    digitalWrite(COOLING_PIN,    LOW);
    digitalWrite(HEATER_PIN,     LOW);
    digitalWrite(FAN_PIN,        LOW);
    digitalWrite(HUMIDIFIER_PIN, LOW);
    digitalWrite(PUMP_PIN,       LOW);
    digitalWrite(GROWLIGHT_PIN,  LOW);
    Serial.println("[Actuator] ⛔ EMERGENCY — all forced OFF");
    return;
  }

  setActuator(COOLING_PIN,    coolingState);
  setActuator(HEATER_PIN,     heaterState);
  setActuator(FAN_PIN,        fanState);
  setActuator(HUMIDIFIER_PIN, humidifierState);
  setActuator(PUMP_PIN,       pumpState);
  setActuator(GROWLIGHT_PIN,  growLightState);

  Serial.printf("[Actuator] Cool=%d  Heat=%d  Fan=%d  Humid=%d  Pump=%d  GrowL=%d\n",
                coolingState, heaterState, fanState, humidifierState, pumpState, growLightState);
}

void setActuator(uint8_t pin, bool state) {
  digitalWrite(pin, state ? HIGH : LOW);
}

void checkAlerts() {

  if (emergencyActive || !warmupComplete || WiFi.status() != WL_CONNECTED) {
    return;
  }

  bool anyAlert = false;

  if (temperature >= TEMP_COOL_ON && !alertTempHigh) {
    alertTempHigh = true;
    sendTelegram("🔴 HIGH TEMPERATURE ALERT!\n"
                 "🌡 Temperature: " + String(temperature, 1) + " °C\n"
                 "⚡ Cooling system activated.");
  } else if (temperature < TEMP_COOL_ON) {
    if (alertTempHigh) {
      alertTempHigh = false;
      sendTelegram("✅ Temperature returned to normal.\n"
                   "🌡 Current: " + String(temperature, 1) + " °C");
    }
  }

  if (temperature <= TEMP_HEAT_ON && !alertTempLow) {
    alertTempLow = true;
    sendTelegram("🔵 LOW TEMPERATURE ALERT!\n"
                 "🌡 Temperature: " + String(temperature, 1) + " °C\n"
                 "🔥 Heater activated.");
  } else if (temperature > TEMP_HEAT_ON) {
    if (alertTempLow) {
      alertTempLow = false;
      sendTelegram("✅ Temperature recovered from cold.\n"
                   "🌡 Current: " + String(temperature, 1) + " °C");
    }
  }

  if (humidity >= HUM_FAN_ON && !alertHumHigh) {
    alertHumHigh = true;
    sendTelegram("🔴 HIGH HUMIDITY ALERT!\n"
                 "💧 Humidity: " + String(humidity, 1) + " %\n"
                 "💨 Fan activated.");
  } else if (humidity < HUM_FAN_ON) {
    if (alertHumHigh) {
      alertHumHigh = false;
      sendTelegram("✅ Humidity returned to normal.\n"
                   "💧 Current: " + String(humidity, 1) + " %");
    }
  }

  if (humidity <= HUM_HUMID_ON && !alertHumLow) {
    alertHumLow = true;
    sendTelegram("🔵 LOW HUMIDITY ALERT!\n"
                 "💧 Humidity: " + String(humidity, 1) + " %\n"
                 "🌫 Humidifier activated.");
  } else if (humidity > HUM_HUMID_ON) {
    if (alertHumLow) {
      alertHumLow = false;
      sendTelegram("✅ Humidity recovered.\n"
                   "💧 Current: " + String(humidity, 1) + " %");
    }
  }

  if (soilMoisture <= SOIL_PUMP_ON && !alertSoilLow) {
    alertSoilLow = true;
    sendTelegram("🔴 SOIL DRY ALERT!\n"
                 "🌱 Soil Moisture: " + String(soilMoisture, 0) + " %\n"
                 "💦 Pump activated.");
  } else if (soilMoisture > SOIL_PUMP_ON) {
    if (alertSoilLow) {
      alertSoilLow = false;
      sendTelegram("✅ Soil moisture recovered.\n"
                   "🌱 Current: " + String(soilMoisture, 0) + " %");
    }
  }

  if (gasLevel >= GAS_FAN_ON && !alertGasHigh) {
    alertGasHigh = true;
    sendTelegram("⚠️ ALERT: High Gas/Smoke detected!\n"
                 "🔥 MQ-2 Reading: " + String(gasLevel) + " (threshold: " + String(GAS_FAN_ON) + ")\n"
                 "💨 Fan activated for ventilation.");
  } else if (gasLevel < GAS_FAN_OFF) {
    if (alertGasHigh) {
      alertGasHigh = false;
      sendTelegram("✅ Gas level returned to normal.\n"
                   "🔥 MQ-2 Reading: " + String(gasLevel));
    }
  }

  if (lightLevel <= LIGHT_GROW_ON && !alertLightLow) {
    alertLightLow = true;
    sendTelegram("⚠️ LOW LIGHT ALERT!\n"
                 "☀️ Light Level: " + String(lightLevel, 0) + " %\n"
                 "💡 Grow Light activated automatically.");
  } else if (lightLevel >= LIGHT_GROW_OFF) {
    if (alertLightLow) {
      alertLightLow = false;
      sendTelegram("✅ Light level recovered.\n"
                   "☀️ Current: " + String(lightLevel, 0) + " %\n"
                   "💡 Grow Light deactivated.");
    }
  }

  if (!anyAlert && !alertAllNormal) {
    alertAllNormal = true;
    sendTelegram("🌿✅ ALL SYSTEMS NORMAL\n"
                 "🌡 Temp: " + String(temperature, 1) + " °C\n"
                 "💧 Hum: "  + String(humidity, 1)    + " %\n"
                 "🌱 Soil: " + String(soilMoisture, 0) + " %\n"
                 "All actuators OFF.");
  }

  if (anyAlert) {
    alertAllNormal = false;
  }
}

void sendTelegram(const String &msg, const String &parseMode) {
  Serial.println("[Telegram] Sending: " + msg);
  if (WiFi.status() == WL_CONNECTED) {
    for (const String &id : knownChatIDs) {
      esp_task_wdt_reset();
      bot.sendMessage(id, msg, parseMode);
    }
    telegramNeedsReconnect = true;
    esp_task_wdt_reset();
  } else {
    Serial.println("[Telegram] WiFi disconnected — message not sent.");
  }
}

void updateLCD() {

  if (!lcdAvailable) return;

  if (emergencyActive) {
    lcd.setCursor(0, 0);
    lcd.print("!! EMERGENCY !!");
    lcd.setCursor(0, 1);
    lcd.print("RESET TO RESUME ");
    return;
  }

  int totalScreens = (sensorFault || useDummy || useBMP280) ? 6 : 5;
  if (millis() - lastLCDSwitch > 2000) {
    lcdScreen = (lcdScreen + 1) % totalScreens;
    lastLCDSwitch = millis();
    lcd.clear();
  }

  if (lcdScreen == 0) {
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temperature, 1);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("Hum: ");
    lcd.print(humidity, 0);
    lcd.print("%");
  }

  else if (lcdScreen == 1) {
    lcd.setCursor(0, 0);
    lcd.print("Soil: ");
    lcd.print((int)soilMoisture);
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print("Light: ");
    lcd.print((int)lightLevel);
    lcd.print("%");
  }

  else if (lcdScreen == 2) {
    lcd.setCursor(0, 0);
    lcd.print("Gas: ");
    lcd.print(gasLevel);

    lcd.setCursor(0, 1);
    if (gasLevel >= GAS_FAN_ON) {
      lcd.print("!! DANGER !!");
    } else {
      lcd.print("Status: OK");
    }
  }

  else if (lcdScreen == 3) {
    lcd.setCursor(0, 0);
    if (operatingMode == 0) lcd.print("Mode: AUTO");
    else lcd.print("Mode: MANUAL");

    lcd.setCursor(0, 1);
    if (WiFi.status() == WL_CONNECTED) {
      lcd.print("WiFi: Connected");
    } else {
      lcd.print("WiFi: OFFLINE");
    }
  }

  else if (lcdScreen == 4) {
    lcd.setCursor(0, 0);
    lcd.print("C:");
    lcd.print(coolingState);
    lcd.print(" H:");
    lcd.print(heaterState);
    lcd.print(" F:");
    lcd.print(fanState);

    lcd.setCursor(0, 1);
    lcd.print("P:");
    lcd.print(pumpState);
    lcd.print(" HU:");
    lcd.print(humidifierState);
    lcd.print(" GL:");
    lcd.print(growLightState);
  }

  else if (lcdScreen == 5) {
    lcd.setCursor(0, 0);
    if (useDummy && sensorFault) {
      lcd.print("!! SENSOR FAULT");
      lcd.setCursor(0, 1);
      lcd.print("Using DUMMY vals");
    } else if (useDummy) {
      lcd.print("No sensor found");
      lcd.setCursor(0, 1);
      lcd.print("Using DUMMY vals");
    } else if (useBMP280) {
      lcd.print("Sensor: BMP280");
      lcd.setCursor(0, 1);
      lcd.print("Hum=Fake P=Real");
    } else {
      lcd.print("!! SENSOR FAULT");
      lcd.setCursor(0, 1);
      lcd.print("Fail x");
      lcd.print(bmeFailCount);
    }
  }
}

void checkTelegramMessages() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (telegramNeedsReconnect) {
    secured_client.stop();
    telegramNeedsReconnect = false;
  }

  Serial.printf("[TelegramBot] Polling... (free heap: %u bytes)\n", ESP.getFreeHeap());
  esp_task_wdt_reset();
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  esp_task_wdt_reset();
  Serial.printf("[TelegramBot] getUpdates returned: %d new message(s)\n", numNewMessages);

  int batchCount = 0;

  while (numNewMessages > 0 && batchCount < 3) {
    batchCount++;
    for (int i = 0; i < numNewMessages; i++) {
      String chat_id = bot.messages[i].chat_id;
      String text    = bot.messages[i].text;
      text.trim();
      text.toLowerCase();

      bool known = false;
      for (const String &id : knownChatIDs) {
        if (id == chat_id) { known = true; break; }
      }
      if (!known) {
        knownChatIDs.push_back(chat_id);
        Serial.println("[TelegramBot] Added new user Chat ID: " + chat_id);
      }

      Serial.println("[TelegramBot] Received: '" + text + "' from chat: " + chat_id);

      if (text.startsWith("/start") || text.startsWith("/help")) {
        String msg = "🌿 Smart Greenhouse Control System 🌿\n\n";
        msg += "Welcome! This bot monitors your greenhouse environment, controls actuators automatically, and keeps your plants safe 24/7.\n\n";
        msg += "✅ Available Commands:\n";
        msg += "/status — Live sensor readings\n";
        msg += "/actuators — Actuator ON/OFF states\n\n";
        msg += "🔔 You will also receive automatic alerts for:\n";
        msg += "• High/Low temperature\n";
        msg += "• High/Low humidity\n";
        msg += "• Dry soil\n";
        msg += "• Gas/Smoke detection\n";
        msg += "• Emergency events\n";

        esp_task_wdt_reset();
        bot.sendMessage(chat_id, msg, "");
        telegramNeedsReconnect = true;
      }
      else if (text.startsWith("/status")) {
        String sensorName = useBME680 ? "BME680" : (useBMP280 ? "BMP280" : "DUMMY");
        String msg = "📊 SENSOR STATUS\n";
        msg += "📡 Sensor: " + sensorName + "\n\n";
        msg += "🌡️ Temperature: " + String(temperature, 1) + " °C\n";
        msg += "💧 Humidity: " + String(humidity, 1) + " %";
        if (useBMP280) msg += " (fake)";
        if (useDummy)  msg += " (dummy)";
        msg += "\n";
        msg += "🌱 Soil Moisture: " + String(soilMoisture, 0) + " %\n";
        msg += "☀️ Light Level: " + String(lightLevel, 0) + " %\n";
        msg += "💨 Gas/Smoke: " + String(gasLevel) + " (Raw)\n\n";
        msg += "⚙️ Mode: " + String(operatingMode == 0 ? "AUTO" : "MANUAL");

        esp_task_wdt_reset();
        bot.sendMessage(chat_id, msg, "");
        telegramNeedsReconnect = true;
      }
      else if (text.startsWith("/actuators")) {
        String on  = "🟢 ON";
        String off = "🔴 OFF";
        String msg = "⚙️ ACTUATOR STATUS\n\n";
        msg += "❄️ Cooling: " + String(coolingState ? on : off) + "\n";
        msg += "🔥 Heater: " + String(heaterState ? on : off) + "\n";
        msg += "🌬️ Fan: " + String(fanState ? on : off) + "\n";
        msg += "🌫️ Humidifier: " + String(humidifierState ? on : off) + "\n";
        msg += "🚰 Pump: " + String(pumpState ? on : off) + "\n";
        msg += "💡 Grow Light: " + String(growLightState ? on : off);

        esp_task_wdt_reset();
        bot.sendMessage(chat_id, msg, "");
        telegramNeedsReconnect = true;
      }
    }
    esp_task_wdt_reset();
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    esp_task_wdt_reset();
  }
}