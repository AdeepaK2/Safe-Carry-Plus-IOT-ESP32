#include "esp_task_wdt.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <UrlEncode.h>
#include <DHT.h>
#include <TinyGPS++.h>

#include "time.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WIFI_SSID "Gala"
#define WIFI_PASSWORD "12345678"

#define DHTPIN 26
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

TinyGPSPlus gps; 
HardwareSerial neogps(1);

#define DOOR_SENSOR_PIN 19
#define pirPin 14
#define LDR_PIN 34
#define LED_PIN 13
#define button_led 27
#define button_pir 23
#define timeout 30000

unsigned long lastMotionTime = 0;

const char* THINGSPEAK_API_KEY = "UPXKBN7UMQ6BC8SW"; // Replace with your ThingSpeak API Key

// NTP Server to get time
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;  // UTC +5:30 for Colombo
const int daylightOffset_sec = 0;

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 10) {
    delay(500);
    Serial.println("Connecting..");
    retryCount++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
  } else {
    Serial.println("Failed to connect to WiFi");
  }
}

void readDHTSensor(float &temperature, float &humidity) {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
  }
}

void sendDataToThingSpeak(float latitude, float longitude, float temperature, float humidity, float speed) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String server = "http://api.thingspeak.com/update?api_key=" + String(THINGSPEAK_API_KEY);
    server += "&field1=" + String(latitude, 6);
    server += "&field2=" + String(longitude, 6);
    server += "&field3=" + String(temperature);
    server += "&field4=" + String(humidity);
    server += "&field5=" + String(speed);

    Serial.print("Sending data to ThingSpeak: ");
    Serial.println(server);

    http.begin(server);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      if (httpResponseCode == 200) {
        Serial.println("Data sent successfully to ThingSpeak");
      } else {
        Serial.print("Error sending data to ThingSpeak. HTTP error code: ");
        Serial.println(httpResponseCode);
      }
    } else {
      Serial.print("Error in HTTP request: ");
      Serial.println(http.errorToString(httpResponseCode).c_str());
    }
    http.end();
  } else {
    Serial.println("WiFi not connected. Data not sent.");
  }
}

void gpsAll(float &latitude, float &longitude, float &speed) {
  boolean newData = false;
  for (unsigned long start = millis(); millis() - start < 1000;) {
    while (neogps.available()) {
      if (gps.encode(neogps.read())) {
        newData = true;
      }
    }
  }
  if (newData) {
    if (gps.location.isValid()) {
      latitude = gps.location.lat();
      longitude = gps.location.lng();
      speed = gps.speed.kmph();
    }
  } else {
    Serial.println("No new data");
  }
}



bool door() {
  static int currentDoorState = digitalRead(DOOR_SENSOR_PIN);
  int lastDoorState;
  lastDoorState = currentDoorState;
  currentDoorState = digitalRead(DOOR_SENSOR_PIN);

  if (lastDoorState == HIGH && currentDoorState == LOW) {
    Serial.println("The door-opening event is detected");
    return true;
  } else if (lastDoorState == LOW && currentDoorState == HIGH) {
    Serial.println("The door-closing event is detected");
    lastMotionTime = millis();
    return false;
  }
}

//pir_motion sensor
void pir_motion() {
    if(pirbutton()){
         digitalWrite(pirPin,HIGH);
         Serial.println("outside led strip on");
    }
    else{
        digitalWrite(pirPin,LOW);
        Serial.println("outside led strip off");

    }
}

bool pirbutton() {
  int buttonState = digitalRead(button_pir);
  if (buttonState == LOW) {
    Serial.println("The pirbutton is pressed");
    return true;
  } else if (buttonState == HIGH) {
    Serial.println("The pirbutton is released");
    return false;
  }
}

// Function to control LED based on LDR sensor
void controlLedBasedOnLdr() {
  int THRESHOLD = 4095;
  int ldrValue = analogRead(LDR_PIN);
  Serial.print("LDR Value: ");
  Serial.println(ldrValue);
  if (door() && (ldrValue >= THRESHOLD)) {
    digitalWrite(LED_PIN, HIGH);
  } else if (ledbutton() && door()) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}

bool ledbutton() {
  int buttonState = digitalRead(button_led);
  if (buttonState == LOW) {
    Serial.println("The ledbutton is pressed");
    return true;
  } else if (buttonState == HIGH) {
    Serial.println("The ledbutton is released");
    return false;
  }
}

void temperatureAndLocationTask(void *parameter) {
  float temperature, humidity, longitude, latitude, speed;
  
  for (;;) {
    readDHTSensor(temperature, humidity);
    gpsAll(latitude, longitude, speed);
    sendDataToThingSpeak(latitude, longitude, temperature, humidity, speed);
    vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay to yield
  }
}

void ldrMotiontask(void *parameters) {
  for (;;) {
    pir_motion();
    controlLedBasedOnLdr();
    vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay to yield
  }
}

void setup() {
  Serial.begin(9600);
  dht.begin();
  
  neogps.begin(9600, SERIAL_8N1, 16, 17);
  pinMode(pirPin, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(button_led, INPUT_PULLUP);
  pinMode(button_pir, INPUT_PULLUP);
  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP);
  connectToWiFi();

  // Initialize NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Debugging print for NTP time synchronization

  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 60000, // 60 seconds
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

  xTaskCreatePinnedToCore(temperatureAndLocationTask, "TempLocTask", 50000, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(ldrMotiontask, "LdrMotionT", 50000, NULL, 1, NULL, 1);
}

void loop() {
  esp_task_wdt_reset();
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}