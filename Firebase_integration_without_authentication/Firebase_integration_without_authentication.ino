#include <WiFi.h>
#include <LiquidCrystal.h>
#include <TM1637Display.h>
#include <DHT.h>
#include <time.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// ===== WiFi Credentials =====
const char* ssid = "Your wifi";
const char* password = "Your password";

// ===== Firebase Configuration =====
#define FIREBASE_HOST "you firebase_host url"
#define FIREBASE_SECRET "you secret key" // Replace with your secret
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ===== DHT Sensor Setup =====
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ===== TM1637 Display =====
#define CLK 5
#define DIO 18
TM1637Display display(CLK, DIO);

// ===== Switch Setup =====
#define SWITCH_PIN 19

// ===== LCD Pins =====
LiquidCrystal lcd(14, 27, 26, 25, 33, 32);

// ===== Custom Characters =====
const uint8_t DEGREE[] = {SEG_A | SEG_B | SEG_G | SEG_F};
const uint8_t CELSIUS[] = {SEG_A | SEG_D | SEG_E | SEG_F};
const uint8_t PERCENT[] = {SEG_G | SEG_C | SEG_E};
const uint8_t HUMI_H[] = {SEG_B | SEG_C | SEG_E | SEG_F | SEG_G};
const uint8_t ERROR[] = {
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,
  SEG_E | SEG_G,
  SEG_E | SEG_G,
  0x00
};

void setup() {
  Serial.begin(115200);
  dht.begin();
  display.setBrightness(7);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  lcd.begin(16, 2);
  
  // Connect to WiFi
  lcd.clear();
  lcd.print("Connecting WiFi");
  WiFi.begin(ssid, password);
  
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  lcd.setCursor(0, 1);
  if (WiFi.status() == WL_CONNECTED) {
    lcd.print("WiFi: Connected ");
    Serial.println("\nWiFi connected");
  } else {
    lcd.print("WiFi: Failed     ");
    Serial.println("\nWiFi failed");
    return;
  }

  // Configure NTP
  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");

  // Initialize Firebase
  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_SECRET;
  Firebase.reconnectNetwork(true);
  Firebase.begin(&config, &auth);
}

void loop() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Time sync failed");
    lcd.clear();
    lcd.print("NTP Failed");
    lcd.setCursor(0, 1);
    lcd.print("Retrying...");
    configTime(19800, 0, "pool.ntp.org", "time.nist.gov");
    delay(1000);
    return;
  }

  // Format time/date
  char dateBuffer[17], timeBuffer[17];
  strftime(dateBuffer, sizeof(dateBuffer), "%a %d/%m/%Y", &timeinfo);
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);

  // Update LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(dateBuffer);
  lcd.setCursor(0, 1);
  lcd.print(timeBuffer);

  // Read sensor
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  int switchState = digitalRead(SWITCH_PIN);

  if (isnan(temp) || isnan(humidity)) {
    Serial.println("DHT11 Error!");
    display.setSegments(ERROR);
    delay(1000);
    return;
  }

  // Update 7-segment display
  if (switchState == HIGH) {
    int t = (int)temp;
    uint8_t segments[] = {
      display.encodeDigit(t / 10),
      display.encodeDigit(t % 10),
      DEGREE[0],
      CELSIUS[0]
    };
    display.setSegments(segments);
  } else {
    int h = (int)humidity;
    uint8_t segments[] = {
      display.encodeDigit(h / 10),
      display.encodeDigit(h % 10),
      PERCENT[0],
      HUMI_H[0]
    };
    display.setSegments(segments);
  }

  // Send to Firebase
  if (Firebase.ready() && WiFi.status() == WL_CONNECTED) {
    FirebaseJson json;
    json.set("temperature", temp);
    json.set("humidity", humidity);
    json.set("date", dateBuffer);
    json.set("time", timeBuffer);
    json.set("day", String(timeinfo.tm_wday)); // 0=Sunday

    if (Firebase.RTDB.pushJSON(&fbdo, "/sensor_data", &json)) {
      Serial.println("Firebase: Data sent!");
    } else {
      Serial.println("Firebase error: " + fbdo.errorReason());
    }
  }

  delay(1000);
}
// set the rules for the firebase
// {
//   "rules": {
//     ".read": true,
//     ".write": true
//   }
// }