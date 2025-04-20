#include <WiFi.h>
#include <LiquidCrystal.h>
#include <TM1637Display.h>
#include <DHT.h>
#include <time.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

// ===== WiFi Credentials =====
const char* ssid = "Moto";
const char* password = "Abhinav0";

// ===== Firebase Configuration =====
#define FIREBASE_HOST "your host url"
#define FIREBASE_API_KEY "you api key" // From Firebase Project Settings > General > Web API Key
#define FIREBASE_EMAIL "your email" // Replace with your registered email
#define FIREBASE_PASSWORD "your password" // Replace with your password


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

  // ===== Firebase Authentication =====
  config.database_url = FIREBASE_HOST;
  config.api_key = FIREBASE_API_KEY;
  config.token_status_callback = tokenStatusCallback;
  
  auth.user.email = FIREBASE_EMAIL;
  auth.user.password = FIREBASE_PASSWORD;

  Serial.print("Authenticating...");
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Success!");
  } else {
    Serial.print("FAILED: ");
    Serial.println(fbdo.errorReason().c_str()); // Fixed error reporting
  }
// Inside setup(), after Firebase.signUp():
Serial.print("User UID: ");
Serial.println(auth.token.uid.c_str());


  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
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

  // ===== Firebase Data Push with User UID =====
  if (Firebase.ready()) { 
    FirebaseJson json;
    json.set("temperature", temp);
    json.set("humidity", humidity);
    json.set("date", dateBuffer);
    json.set("time", timeBuffer);
    json.set("day", String(timeinfo.tm_wday));

    // Add the user's UID to the data
    json.set("user_uid", auth.token.uid.c_str()); // <-- Added UID field

    // Push data to Firebase
    if (Firebase.RTDB.pushJSON(&fbdo, "/sensor_data", &json)) {
      Serial.println("Data sent!");
    } else {
      Serial.print("Firebase error: ");
      Serial.println(fbdo.errorReason().c_str());
    }
  }

  // Token management
  if (Firebase.isTokenExpired()) {
    Serial.println("Refreshing token...");
    Firebase.refreshToken(&config);
  }

  delay(1000); 
}



// Rules for the firebase you can do this when testing.
// {
//   "rules": {
//     "sensor_data": {
//       ".read": "auth != null",
//       ".write": "auth != null"
//     }
//   }
// }


// Rules for the user can see/read only date
// {
//   "rules": {
//     "sensor_data": {
//       "$uid": {
//         ".read": "auth != null && auth.uid == $uid",
//         ".write": "auth != null && auth.uid == $uid"
//       }
//     }
//   }
// }