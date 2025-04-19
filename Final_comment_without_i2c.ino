#include <WiFi.h>                   // For WiFi connectivity
#include <LiquidCrystal.h>         // For 16x2 LCD display
#include <TM1637Display.h>         // For 4-digit 7-segment display
#include <DHT.h>                   // For DHT11 temperature/humidity sensor
#include <time.h>                  // For getting time via NTP

// ===== WiFi Credentials =====
const char* ssid = "Wifi Name";         // Replace with your WiFi name
const char* password = "Your Password"; // Replace with your WiFi password

// ===== DHT Sensor Setup =====
#define DHTPIN 4                // DHT11 data pin connected to GPIO 4
#define DHTTYPE DHT11           // Using DHT11 sensor
DHT dht(DHTPIN, DHTTYPE);       // Initialize DHT object

// ===== TM1637 Display Pins =====
#define CLK 5                   // CLK pin of 4-digit display to GPIO 5
#define DIO 18                  // DIO pin of 4-digit display to GPIO 18
TM1637Display display(CLK, DIO); // Initialize 4-digit display object

// ===== Switch Setup =====
#define SWITCH_PIN 19           // Toggle switch connected to GPIO 19 (use HIGH = temp, LOW = humidity)

// ===== LCD Pins (RS, E, D4, D5, D6, D7) =====
LiquidCrystal lcd(14, 27, 26, 25, 33, 32); // Initialize 16x2 LCD with defined GPIO pins

// ===== Custom Characters for 4-digit Display =====
const uint8_t DEGREE[] = {SEG_A | SEG_B | SEG_G | SEG_F};       // °
const uint8_t CELSIUS[] = {SEG_A | SEG_D | SEG_E | SEG_F};      // C
const uint8_t PERCENT[] = {SEG_G | SEG_C | SEG_E};              // % symbol
const uint8_t HUMI_H[] = {SEG_B | SEG_C | SEG_E | SEG_F | SEG_G}; // H for Humidity
const uint8_t ERROR[] = {
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G, // E
  SEG_E | SEG_G,                         // r
  SEG_E | SEG_G,                         // r
  0x00                                   // Blank
};

// ===== SETUP FUNCTION =====
void setup() {
  Serial.begin(115200);         // Start serial communication for debugging
  dht.begin();                  // Start the DHT sensor
  display.setBrightness(7);     // Set brightness for 4-digit display
  pinMode(SWITCH_PIN, INPUT_PULLUP); // Set switch pin with internal pull-up
  lcd.begin(16, 2);             // Initialize LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi"); // Show WiFi status on LCD

  // ===== Connect to WiFi =====
  WiFi.begin(ssid, password);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  // Show result on LCD
  lcd.setCursor(0, 1);
  if (WiFi.status() == WL_CONNECTED) {
    lcd.print("WiFi: Connected ");
    Serial.println("WiFi connected");
  } else {
    lcd.print("WiFi: Failed     ");
    Serial.println("WiFi failed");
  }

  // ===== Time Configuration for IST =====
  configTime(19800, 0, "pool.ntp.org", "time.nist.gov"); // GMT + 5:30 (IST)

  // Wait for time to sync from NTP server
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Waiting for NTP...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Time Syncing...");
    lcd.setCursor(0, 1);
    lcd.print("Wait NTP Server");
    delay(1000);
  }
}

// ===== LOOP FUNCTION =====
void loop() {
  // ===== Get Current Time =====
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to get time");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("NTP Failed");
    lcd.setCursor(0, 1);
    lcd.print("Retrying...");
    configTime(19800, 0, "pool.ntp.org", "time.nist.gov"); // Re-try NTP
    delay(1000);
    return;
  }

  // Format date and time strings
  char dateBuffer[17];
  char timeBuffer[17];
  strftime(dateBuffer, sizeof(dateBuffer), "%a %d/%m/%Y", &timeinfo); // Example: Sun 06/04/2025
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);    // Example: 21:41:08

  // Display on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(dateBuffer);
  lcd.setCursor(0, 1);
  lcd.print(timeBuffer);

  // ===== Read DHT Sensor =====
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  int switchState = digitalRead(SWITCH_PIN); // Read switch to determine what to display

  // Error handling for sensor
  if (isnan(temp) || isnan(humidity)) {
    Serial.println("DHT11 Error!");
    display.setSegments(ERROR); // Show "Err " on 4-digit display
    delay(1000);
    return;
  }

  // ===== Show Temperature or Humidity on 4-digit display =====
  if (switchState == HIGH) {
    // Display temperature in format: 25°C
    int t = (int)temp;
    uint8_t segments[] = {
      display.encodeDigit(t / 10),
      display.encodeDigit(t % 10),
      DEGREE[0],
      CELSIUS[0]
    };
    display.setSegments(segments);
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.println("°C");
  } else {
    // Display humidity in format: 52%H
    int h = (int)humidity;
    uint8_t segments[] = {
      display.encodeDigit(h / 10),
      display.encodeDigit(h % 10),
      PERCENT[0],
      HUMI_H[0]
    };
    display.setSegments(segments);
    Serial.print("Humidity: ");
    Serial.print(h);
    Serial.println("%");
  }

  delay(1000); // Refresh interval (you set to 1000ms)
}
