/*****************************************************************************
* ESP-TMEP PoC 2
******************************************************************************
* (c) Michal A. Valasek, 2022 | Licensed under terms of the MIT license
* www.rider.cz | www.altair.blog | github.com/ridercz/ESP-TMEP
******************************************************************************
* This is proof of concept sketch to test sending data to TMEP.CZ with 
* hardcoded parameters
*****************************************************************************/

#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>

#define VERSION "PoC2"                        // Version string
#define MEASURE_INTERVAL 60000                // Interval in ms in which temperature is measured
#define PIN_ONEWIRE D2                        // Pin where sensors are connected
#define PIN_LED LED_BUILTIN                   // Pin where LED is connected
#define LED_INTERVAL 250                      // LED blink interval
#define LOOP_INTERVAL 2000                    // Loop delay interval
#define WIFI_SSID "test"                      // Wi-Fi network
#define WIFI_PASS "test"                      // Wi-Fi password
#define TMEP_HOST "test.tmep.cz"              // TMEP.CZ server name
#define TMEP_PORT 80                          // TMEP.CZ HTTP port
#define TMEP_GUID "temp"                      // TMEP.CZ parameter name (GUID)
#define TMEP_TIMEOUT 5000                     // TMEP.CZ response timeout

// Define library static instances
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature sensors(&oneWire);

// Define variables
DeviceAddress lastDeviceAddress;  // Device address (used for enumeration and checking)
unsigned long nextMeasureTime = 0;

void setup() {
  // Init serial port
  Serial.begin(9600);
  Serial.println();
  Serial.println();
  Serial.println("ESP-TMEP/" VERSION " | https://github.com/ridercz/ESP-TMEP");
  Serial.println("(c) Michal A. Valasek, 2022 | www.altair.blog | www.rider.cz");
  Serial.println();
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());

  // Start the DallasTemperature library
  sensors.begin();

  // Setup LED
  pinMode(PIN_LED, OUTPUT);

  // Connect to WiFi
  Serial.printf("Connecting to %s...", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    blinkLed(1);
  }
  Serial.print("OK, IP ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (millis() > nextMeasureTime) {
    // Measure temperature
    DeviceAddress addr;
    float t;
    sensors.requestTemperatures();
    if (sensors.getAddress(addr, 0)) {
      t = sensors.getTempC(addr);
      Serial.printf("Temperature: %.2f\n", t);
    } else {
      // Failed to measure temperature - blink twice
      Serial.print("Temperature: Error!\n");
      blinkLed(2);
      return;
    }

    // Send it to TMEP service
    if (!sendValue(t)) return;

    // Schedule next measurement
    nextMeasureTime = millis() + MEASURE_INTERVAL;
  }

  // Blink LED onced
  blinkLed(1);

  // Wait for next loop
  delay(LOOP_INTERVAL);
}

bool sendValue(float t) {
  WiFiClient client;

  Serial.printf("Connecting to %s:%i...", TMEP_HOST, TMEP_PORT);
  if (!client.connect(TMEP_HOST, TMEP_PORT)) {
    Serial.println("Failed!");
    blinkLed(3);
    return false;
  }
  Serial.println("OK");

  Serial.printf("Requesting /?%s=%.2f...", TMEP_GUID, t);
  client.printf("GET /?%s=%.2f HTTP/1.1\r\n", TMEP_GUID, t);
  client.printf("Host: %s\r\n", TMEP_HOST);
  client.print("Connection: close\r\n");
  client.print("User-Agent: ESP-TMEP/" VERSION " (https://github.com/ridercz/ESP-TMEP)\r\n");
  client.print("\r\n");
  unsigned long timeout = millis() + TMEP_TIMEOUT;
  while (client.available() == 0) {
    if (millis() > timeout) {
      Serial.println("Timeout!");
      client.stop();
      blinkLed(4);
      return false;
    }
  }
  Serial.println("OK");
  return true;
}

void blinkLed(int count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(PIN_LED, false);
    delay(LED_INTERVAL);
    digitalWrite(PIN_LED, true);
    delay(LED_INTERVAL);
  }
}