/*****************************************************************************
* ESP-TMEP PoC 1 
******************************************************************************
* (c) Michal A. Valasek, 2022 | Licensed under terms of the MIT license
* www.rider.cz | www.altair.blog | github.com/ridercz/ESP-TMEP
******************************************************************************
* This is proof of concept sketch to test temperature measuring with ESP8266 
* and DS18B20 sensor.
*****************************************************************************/

#include <OneWire.h>
#include <DallasTemperature.h>

#define MEASURE_INTERVAL 1000  // Interval in ms in which temperature is measured
#define PIN_ONEWIRE D2         // Pin where sensors are connected
#define PIN_LED LED_BUILTIN    // Pin where LED is connected

// Define library static instances
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature sensors(&oneWire);

// Define variables
int deviceCount;                  // Number of temp sensors
DeviceAddress lastDeviceAddress;  // Device address (used for enumeration and checking)
bool nextBlinkState = false;      // Next state of LED

void setup() {
  // Init serial port
  Serial.begin(9600);
  Serial.println();
  Serial.println();

  // Start the DallasTemperature library
  sensors.begin();

  // Enumerate devices
  Serial.print("Enumerating devices...");
  deviceCount = sensors.getDeviceCount();
  Serial.printf("OK, %i found:\n", deviceCount);
  for (int i = 0; i < deviceCount; i++) {
    Serial.printf("  #%02i: ", i);
    if (sensors.getAddress(lastDeviceAddress, i)) {
      Serial.print("0x");
      for (int i = 0; i < 8; i++) {
        if (lastDeviceAddress[i] < 16) Serial.print("0");
        Serial.print(lastDeviceAddress[i], HEX);
      }
      Serial.println();
    } else {
      Serial.println("Error!");
    }
  }

  // Setup LED
  pinMode(PIN_LED, OUTPUT);
}

void loop() {
  // Measure temperatures in all sensors
  sensors.requestTemperatures();

  // Display all values
  for (int i = 0; i < deviceCount; i++) {
    if(i > 0) Serial.print(", ");
    Serial.printf("Sensor #%02i (°C): ", i);
    if (sensors.getAddress(lastDeviceAddress, i)) {
      float t = sensors.getTempC(lastDeviceAddress);
      Serial.printf("%.2f", t);
    } else {
      Serial.print("Error!");
    }
    Serial.println();
  }

  // Blink LED
#ifdef PIN_LED
  nextBlinkState = !nextBlinkState;
  digitalWrite(PIN_LED, nextBlinkState);
#endif

  // Wait till next measure interval
  delay(MEASURE_INTERVAL);
}