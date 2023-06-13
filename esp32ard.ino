#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "HX711.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <BlynkSimpleEsp32.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);  // Set the LCD address and dimensions (change address if necessary)

#define DOUT  32
#define CLK  33
#define BUTTON_PIN 13
#define BUZZER_PIN 12

HX711 scale;

const char* BLYNK_TEMPLATE_ID = "TMPL3UWKXwkbf";
const char* BLYNK_TEMPLATE_NAME = "load cell";

char auth[] = "KYY7zrOb7PaI99joDJ3v2qqt8BeKOCj_";  // Blynk authentication token
char ssid[] = "vivo 1818";   // WiFi SSID
char password[] = "123456789";  // WiFi password

float calibration_factor = -96650;

WiFiMulti wifiMulti;

BlynkTimer timer;

bool isScaleOn = true;  // Variable to track the scale on/off state
bool prevWifiStatus = false;  // Previous Wi-Fi connection status
bool prevScaleState = false;  // Previous scale state

float thresholdWeight = 10.0;  // Default threshold weight in kg

void setup() {
  Serial.begin(115200);
  Serial.println("Press T to tare");
  lcd.init(); 
  scale.begin(DOUT, CLK);  // Initialize the HX711 instance with the pin numbers

  scale.set_scale(-96650);
  scale.tare();

  Blynk.begin(auth, ssid, password);  // Connect to Blynk using WiFi and Template

  // Set up Wi-Fi network connection
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(ssid, password);

  // Configure timer to read weight and sync with Blynk every second
  timer.setInterval(1000L, myTimer);

  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Set the push button pin as INPUT_PULLUP
  pinMode(BUZZER_PIN, OUTPUT);  // Set the buzzer pin as OUTPUT

  // Initialize threshold weight from Blynk app or default value
  Blynk.syncVirtual(V4);  // Synchronize the threshold weight from the Blynk app

  lcd.begin(16, 2);  // Initialize the LCD display
  lcd.backlight();  // Turn on the backlight
  lcd.setCursor(0, 0);
  lcd.print("Weight: Initializing...");
  lcd.setCursor(0, 1);
  lcd.print("Threshold: ");
  lcd.print(thresholdWeight);
  lcd.print(" kg");

}

void loop() {
  Blynk.run();
  timer.run();

  // Check if there is any data available to read from the serial monitor
  if (Serial.available()) {
    char input = Serial.read();

    // Perform actions based on the user input
    if (input == 't' || input == 'T') {
      scale.tare();
      Serial.println("Tared");
      Blynk.virtualWrite(V1, 1); // Send a signal to Blynk to trigger the tare action
    } else if (input == 'w' || input == 'W') {
      Serial.println("Enter new threshold weight:");
      while (Serial.available() == 0); // Wait until input is received
      String weightInput = Serial.readString();
      thresholdWeight = weightInput.toFloat();
      Serial.print("New threshold weight set to: ");
      Serial.print(thresholdWeight);
      lcd.setCursor(0, 1);
      lcd.print("Threshold: ");
      lcd.print(thresholdWeight);
      lcd.print("kg");
      Serial.println(" kg");
      Blynk.virtualWrite(V4, thresholdWeight); // Update threshold weight in Blynk app
    }
  }

  // Check the state of the push button
  if (digitalRead(BUTTON_PIN) == LOW) {
    isScaleOn = !isScaleOn;  // Toggle the scale on/off state
    delay(200);  // Add a small delay to debounce the button
    Blynk.virtualWrite(V3, isScaleOn ? 1 : 0); // Update the on/off button state in the Blynk app/website
  }

  // Check Wi-Fi connection and display available networks
  bool currentWifiStatus = WiFi.status() == WL_CONNECTED;
  if (currentWifiStatus != prevWifiStatus) {
    prevWifiStatus = currentWifiStatus;
    if (prevWifiStatus) {
      Serial.println("Connected to Wi-Fi");
    } else {
      Serial.println("Disconnected from Wi-Fi");
    }
  }

  if (prevWifiStatus && isScaleOn != prevScaleState) {
    if (isScaleOn) {
      scale.power_up();  // Power up the scale
      Serial.println("Scale: ON");
    } else {
      scale.power_down();  // Power down the scale
      Serial.println("Scale: OFF");
      lcd.clear();
    }
    prevScaleState = isScaleOn;
  }
}

void myTimer() {
  if (prevWifiStatus && isScaleOn) {
    float weight = scale.get_units();
    Serial.print("Weight: ");
    Serial.print(weight, 3);
    Serial.println(" kg");
    checkThreshold(weight);  // Check if weight exceeds the threshold

    // Update LCD display
    lcd.setCursor(0, 0);
    lcd.print("Weight: ");
    lcd.print(weight, 3);
    lcd.print(" kg");

    // Send weight data to Blynk virtual pin V2 (connected to gauge widget)
    Blynk.virtualWrite(V2, weight);
  }
}

void checkThreshold(float weight) {
  if (weight >= thresholdWeight) {
    digitalWrite(BUZZER_PIN, HIGH);  // Turn on the buzzer
    Blynk.virtualWrite(V5, 1);  // Send alert to Blynk app (virtual pin V5)
    Blynk.logEvent("alert","The Threshold is reached");
  } else {
    digitalWrite(BUZZER_PIN, LOW);  // Turn off the buzzer
    Blynk.virtualWrite(V5, 0);  // Clear alert in Blynk app (virtual pin V5)
  }
}

BLYNK_WRITE(V1) {
  if (param.asInt() == 1) {
    scale.tare();
    Serial.println("Tared");
  }
}

BLYNK_READ(V2) {
    float weight = scale.get_units();
    Blynk.virtualWrite(V2, weight);
}

BLYNK_WRITE(V3) {
  int buttonState = param.asInt();
  if (buttonState == 1) {
    isScaleOn = true;  // Turn on the scale
  } else {
    isScaleOn = false;  // Turn off the scale
  }
}

BLYNK_WRITE(V4) {
  thresholdWeight = param.asFloat();  // Update threshold weight from Blynk app
  Serial.print("New threshold weight set to: ");
  Serial.print(thresholdWeight);
  Serial.println(" kg");

  // Update LCD display with new threshold weight
  lcd.setCursor(0, 1);
  lcd.print("Threshold: ");
  lcd.print(thresholdWeight);
  lcd.print(" kg");
}
