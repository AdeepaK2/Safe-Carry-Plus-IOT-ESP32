#define BLYNK_TEMPLATE_ID "TMPL6pV_GZrck"
#define BLYNK_TEMPLATE_NAME "Pow"
#define BLYNK_AUTH_TOKEN "r67qUU-4qVY8Iv1M1udj5pBTV8dTgk-m"

#include <WiFi.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <BluetoothSerial.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Adafruit_Fingerprint.h>
#include <Preferences.h>
#include <LiquidCrystal_I2C.h>
//Siyol-4G3D4A77 CED3C707
// WiFi and Blynk credentials
#define WIFI_SSID "Gala"
#define WIFI_PASSWORD "12345678"

// Pin definitions
#define tonePin 14
#define VOLTAGE_SENSOR_PIN 32

bool wifiPaused = false;
unsigned long wifiPauseStartTime = 0;
uint8_t id = 0;
bool buzzerState = false; // Buzzer state flag
bool authEnabled = true;
int c = 1;
unsigned long lastMotionTime = 0;

HardwareSerial mySerial(1); // Using hardware serial port 1 for fingerprint sensor
Adafruit_Fingerprint finger(&mySerial); // Create an instance of the fingerprint sensor
Preferences preferences;

LiquidCrystal_I2C lcd(0x27, 20, 4);

byte bar0[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
byte bar1[8] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10};
byte bar2[8] = {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18};
byte bar3[8] = {0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C};
byte bar4[8] = {0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E};
byte bar5[8] = {0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F};

BluetoothSerial SerialBT;


// Blynk 
BLYNK_WRITE(V2) {
    int buttonState = param.asInt();
    if (buttonState == 1) {
        displayMessage("Remote Open", 1);
        digitalWrite(13, HIGH);
        delay(6000);
        digitalWrite(13, LOW);
    }
}

BLYNK_WRITE(V4) {
     int BuzzState = param.asInt(); // Get the state from Blynk app
    if (BuzzState == 1 && !buzzerState) {
        buzzerState = true;
        c = 2;
        Serial.println("Buzzer ON");
    } else if (BuzzState == 0 && buzzerState) {
        buzzerState = false;
        digitalWrite(tonePin,LOW);
        c = 1;
        Serial.println("Buzzer OFF");
    }
}

BLYNK_WRITE(V5) {
    int buttonState = param.asInt();
    if (buttonState == 1) {
        Serial.println("Added a fingerprint");
        enrollFingerprint();
    }
}

BLYNK_WRITE(V6) {
    int buttonState = param.asInt();
    if (buttonState == 1) {
        Serial.println("Deleted Fingerprints");
        deleteAllFingerprints();
    }
}

void connectToWiFi() {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < 10) {
        delay(500);
        Serial.println("Connecting...");
        retryCount++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
    } else {
        Serial.println("Failed to connect to WiFi");
        displayMessage("No WIFI found", 4);
    }
}


void checkBluetoothConnection() {
    if (SerialBT.hasClient()) {
        Blynk.virtualWrite(V3, 1);
         Serial.println("Bluetooth conected");
    } else {
        Blynk.logEvent("you_are_leaving_without_bag");
         Blynk.virtualWrite(V3, 0);
         Serial.println("you_are_leaving_without_bag");
    }
}


void enrollFingerprint() {
    authEnabled = false;
    id++;
    if (id > 1000) {
        Serial.println("Memory Full, ID is out of range!");
        displayMessage("Memory Full", 1);
        authEnabled = true;
        return;
    }
    Serial.print("Automatically enrolling to ID #");
    Serial.println(id);
    displayMessage("Enrolling...", 1);

    if (getFingerprintEnroll()) {
        Serial.println("Fingerprint enrolled successfully.");
        displayMessage("Enrolled", 1);
    } else {
        displayMessage("Enrollment Failed", 1);
    }
    authEnabled = true;
    delay(2000);
}

bool getFingerprintEnroll() {
    int p = -1;
    Serial.print("Waiting for valid finger to enroll as #");
    Serial.println(id);
    displayMessage("Place Finger", 1);
    while (p != FINGERPRINT_OK) {
        p = finger.getImage();
        switch (p) {
            case FINGERPRINT_OK:
                Serial.println("Image taken");
                break;
            case FINGERPRINT_NOFINGER:
                Serial.println(".");
                delay(50);
                break;
            case FINGERPRINT_PACKETRECIEVEERR:
                Serial.println("Communication error");
                displayMessage("Comm Error", 1);
                id=max(0,id-1);
                break;
            case FINGERPRINT_IMAGEFAIL:
                Serial.println("Imaging error");
                displayMessage("Image Error", 1);
                id=max(0,id-1);
                break;
            default:
                Serial.println("Unknown error");
                displayMessage("Unknown Error", 1);
                id=max(0,id-1);
                return false;
        }
    }

    p = finger.image2Tz(1);
    if (p != FINGERPRINT_OK) {
        Serial.println("Error converting image");
        displayMessage("Image Conv. Error", 1);
        return false;
    }

    Serial.println("Remove finger");
    displayMessage("Remove Finger", 1);
    delay(2000);
    while (finger.getImage() != FINGERPRINT_NOFINGER);

    Serial.println("Place same finger again");
    displayMessage("Place Same Finger", 1);
    while (finger.getImage() != FINGERPRINT_OK);

    p = finger.image2Tz(2);
    if (p != FINGERPRINT_OK) {
        Serial.println("Error converting image");
        displayMessage("Image Conv. Error", 1);
        return false;
    }

    p = finger.createModel();
    if (p != FINGERPRINT_OK) {
        Serial.println("Error creating model");
        displayMessage("Model Error", 1);
        return false;
    }

    p = finger.storeModel(id);
    if (p != FINGERPRINT_OK) {
        Serial.println("Error storing model");
        displayMessage("Store Error", 1);
        return false;
    }

    return true;
}

void authenticateFingerprint() {
    if (!authEnabled) return;

    int p = finger.getImage();
    if (p == FINGERPRINT_OK) {
        p = finger.image2Tz();
        if (p != FINGERPRINT_OK) {
            return;
        }
        p = finger.fingerFastSearch();
        if (p == FINGERPRINT_OK) {
            Serial.println("Fingerprint matched!");
            Serial.print("Found ID #");
            Serial.println(finger.fingerID);
            Serial.print("With confidence of ");
            Serial.println(finger.confidence);
            displayMessage("Fingerprint Matched!", 1);
            digitalWrite(13, HIGH);
            delay(5000);
            digitalWrite(13, LOW);
            Blynk.logEvent("fp");
        } else {
            if (p == FINGERPRINT_NOTFOUND) {
                Serial.println("Did not find a match");
                displayMessage("No Match Found", 1);
                delay(2000);
            }
        }
    }
}

void deleteAllFingerprints() {
    authEnabled = false;
    Serial.println("Deleting all fingerprints from sensor...");
    displayMessage("Deleting All...", 1);
    uint8_t p = finger.emptyDatabase();
    if (p == FINGERPRINT_OK) {
        Serial.println("All fingerprints deleted successfully!");
        id = 0;
        displayMessage("All Deleted", 1);
    } else {
        Serial.println("Failed to delete fingerprints!");
        displayMessage("Delete Failed", 1);
    }
    authEnabled = true;
    delay(2000);
}

// Function to draw the bar graph on the LCD
void drawBarGraph(int row, int col, int percentage) {
    int fullBlocks = percentage / 5; 
    int partialBlock = (percentage % 5); // Adjusted for bar graph

    for (int i = 0; i < 20; i++) {
        lcd.setCursor(col + i, row);
        if (i < fullBlocks) {
            lcd.write(5);
        } else if (i == fullBlocks && partialBlock > 0) {
            lcd.write(partialBlock);
        } else {
            lcd.write(0);
        }
    }
}
void buzzer() {
    if (c == 2) {
        Despacito();
    }
}
// Buzzer melody functions
void Despacito() {
 if(!buzzerState){return;}
  tone(tonePin, 587, 709.720327982);
  delay(788.578142202);
  delay(10.3082110092);
  if(!buzzerState){return;}
  tone(tonePin, 554, 709.720327982);
  delay(788.578142202);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 493, 273.683002294);
  delay(304.092224771);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 369, 273.683002294);
  delay(304.092224771);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 369, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
  if(!buzzerState){return;}
  tone(tonePin, 369, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 369, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 369, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 369, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 493, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 493, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 493, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 493, 273.683002294);
  delay(304.092224771);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 440, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 493, 273.683002294);
  delay(304.092224771);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 391, 412.843850917);
  delay(458.715389908);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 391, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 391, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 391, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 391, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
  if(!buzzerState){return;}
  tone(tonePin, 391, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
  if(!buzzerState){return;}
  tone(tonePin, 493, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
   if(!buzzerState){return;}
  tone(tonePin, 493, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
   if(!buzzerState){return;}
  tone(tonePin, 493, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
  if(!buzzerState){return;}
  tone(tonePin, 493, 273.683002294);
  delay(304.092224771);
  delay(5.15410550459);
   if(!buzzerState){return;}
  tone(tonePin, 554, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
  if(!buzzerState){return;}
  tone(tonePin, 587, 273.683002294);
  delay(304.092224771);
  delay(5.15410550459);
   if(!buzzerState){return;}
  tone(tonePin, 440, 412.843850917);
  delay(458.715389908);
  delay(5.15410550459);
   if(!buzzerState){return;}
  tone(tonePin, 440, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
   if(!buzzerState){return;}
  tone(tonePin, 440, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
   if(!buzzerState){return;}
  tone(tonePin, 440, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
   if(!buzzerState){return;}
  tone(tonePin, 440, 41.7482545872);
  delay(46.3869495413);
  delay(36.0787385321);
  if(!buzzerState){return;}
  tone(tonePin, 440, 37.109559633);
  delay(41.2328440367);
  delay(30.9246330275);
  if(!buzzerState){return;}
  tone(tonePin, 440, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
   if(!buzzerState){return;}
  tone(tonePin, 587, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
   if(!buzzerState){return;}
  tone(tonePin, 587, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
  if(!buzzerState){return;}
  tone(tonePin, 587, 46.3869495413);
  delay(51.5410550459);
  delay(30.9246330275);
 if(!buzzerState){return;}
  tone(tonePin, 587, 46.3869495413);
  delay(51.5410550459);
  delay(20.6164220183);
  if(!buzzerState){return;}
  tone(tonePin, 587, 273.683002294);
  delay(304.092224771);
  delay(5.15410550459);
 if(!buzzerState){return;}
  tone(tonePin, 659, 134.52215367);
  delay(149.469059633);
  delay(5.15410550459);
   if(!buzzerState){return;}
  tone(tonePin, 659, 273.683002294);
  delay(304.092224771);
  delay(5.15410550459);
   if(!buzzerState){return;}
  tone(tonePin, 554, 691.165548165);
   if(!buzzerState){return;}
 
}

void displayBatteryLevel() {
    int sensorValue = analogRead(VOLTAGE_SENSOR_PIN);
    float voltage = sensorValue * (3.3 / 4095.0) * 11; // Adjust multiplier for voltage divider if needed
    
    // Debugging output
    Serial.print("Analog Read: "); Serial.println(sensorValue);
    Serial.print("Voltage: "); Serial.println(voltage);

    // Assuming 0V as 0% and 12V as 100% for a 12V battery
    int batteryLevel = map(voltage * 100, 900, 1200, 0, 100);
    if (batteryLevel > 100) batteryLevel = 100;
    if (batteryLevel < 0) batteryLevel = 0;

    lcd.setCursor(0, 2);
    lcd.print("Battery Level: ");
    lcd.print(batteryLevel);
    lcd.print(" %");
    
    // Debugging output
    Serial.print("Battery Level: ");
    Serial.print(batteryLevel);
    Serial.println(" %");

    drawBarGraph(3, 0, batteryLevel);
}

void displayMessage(const char *message, int row) {
    lcd.clear();
    lcd.setCursor(0, row);
    lcd.print(message);
}

void buzzerTask(void *parameter) {
    for (;;) {
        buzzer();       
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
void fingerprint_bluetoothTask(void *parameter) {
    for (;;) {
        checkBluetoothConnection();
        if (authEnabled) {
            authenticateFingerprint();
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
void battery_blynkTask(void *parameter) {
    for (;;) {
      //lastupdate
        // if (WiFi.status() != WL_CONNECTED) {
        //     connectToWiFi();
        // }
      // last update
        Blynk.run();
        displayBatteryLevel();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


void setup() {
    Serial.begin(9600);
    SerialBT.begin("ESP32");
    connectToWiFi();
    pinMode(tonePin, OUTPUT);
    Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);\

    mySerial.begin(57600, SERIAL_8N1, 16, 17);
     Serial.println("\n\nAdafruit Fingerprint sensor enrollment"); // Print a startup message

    if (finger.verifyPassword()) {
        Serial.println("Found fingerprint sensor!"); // Print success message
    } else {
        Serial.println("Did not find fingerprint sensor :("); // Print failure message
        while (1) { delay(1); } // Enter an infinite loop if sensor is not found
    }
    pinMode(13, OUTPUT); // Set digital pin 4 as an output for controlling an LED
    digitalWrite(13, LOW);

        preferences.begin("fingerprint", false); // Open NVS namespace 'fingerprint' for storing IDs
    id = preferences.getUInt("lastID", 0); // Retrieve the last stored fingerprint ID, defaulting to 0
    Serial.print("Starting from fingerprint ID #");
    Serial.println(id + 1); // Print the next ID to be used for new fingerprint
    

    //wire init
    Wire.begin(21, 22);
    lcd.init();
    lcd.backlight();

    // Create custom characters
    lcd.createChar(0, bar0);
    lcd.createChar(1, bar1);
    lcd.createChar(2, bar2);
    lcd.createChar(3, bar3);
    lcd.createChar(4, bar4);
    lcd.createChar(5, bar5);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Battery Monitor");
    delay(2000);  // Display initial message for 2 seconds
    lcd.clear();

    xTaskCreatePinnedToCore(fingerprint_bluetoothTask, "Fingerprint_bluetoothTask", 10000, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(buzzerTask, "buzzerTask", 10000, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(battery_blynkTask, "battery_blynkTask", 10000, NULL, 1, NULL, 0);
}

void loop() {
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}