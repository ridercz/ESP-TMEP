/*****************************************************************************
* ESP-TMEP - ESP8266 firmware for sending temperatures to TMEP.CZ
******************************************************************************
* (c) Michal A. Valasek, 2022 | Licensed under terms of the MIT license
* www.rider.cz | www.altair.blog | github.com/ridercz/ESP-TMEP
*****************************************************************************/

#include <FS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

#define VERSION "1.0.0"                   // Version string
#define MEASURE_INTERVAL 60000            // Interval in ms in which temperature is measured
#define PIN_ONEWIRE D2                    // Pin where sensors are connected
#define PIN_LED LED_BUILTIN               // Pin where LED is connected
#define LED_INTERVAL 250                  // LED blink interval
#define LOOP_INTERVAL 2000                // Loop delay interval
#define TMEP_TIMEOUT 5000                 // TMEP.CZ response timeout
#define TMEP_PORT 443                     // TMEP.CZ HTTPS port
#define TMEP_HOST_DEFAULT "demo.tmep.cz"  // TMEP.CZ server name
#define TMEP_GUID_DEFAULT "temp"          // TMEP.CZ parameter name (GUID)
#define JSON_CONFIG_FILE "/config.json"   // SPIFFS configuration file name
//#define WIFIMANAGER_RESET_SETTINGS        // Uncomment for reset of Wi-Fi settings
//#define WIFIMANAGER_DEBUG                 // Uncomment for showing WiFiManager debug messages

// Define configuration variables
char tmepHost[50] = TMEP_HOST_DEFAULT;
char tmepGuid[50] = TMEP_GUID_DEFAULT;

// Define library static instances
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature sensors(&oneWire);
WiFiManager wm;

// Define variables
DeviceAddress lastDeviceAddress;  // Device address (used for enumeration and checking)
bool shouldSaveConfig = false;
unsigned long nextMeasureTime = 0;

void setup() {
  // Init serial port
  Serial.begin(9600);
  Serial.println();
  Serial.println();
  Serial.println("ESP-TMEP/" VERSION " | https://github.com/ridercz/ESP-TMEP");
  Serial.println("(c) Michal A. Valasek, 2022 | www.altair.blog | www.rider.cz");
  Serial.println();

  // Get device ID
  char deviceId[20];
  sprintf(deviceId, "ESP-TMEP-%08X", ESP.getChipId());
  Serial.printf("Device ID: %s\n\n", deviceId);

  // Setup LED
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, false);  // LED on

  // Read configuration from SPIFFS
  bool configLoaded = loadConfigFile();

  // Configure WiFiManager options
#ifdef WIFIMANAGER_DEBUG
  wm.setDebugOutput(true);
#else
  wm.setDebugOutput(false);
#endif
#ifdef WIFIMANAGER_RESET_SETTINGS
  Serial.println("Resetting WiFi manager settings...");
  wm.resetSettings();
#endif

  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setAPCallback(configModeCallback);

  WiFiManagerParameter tmepHostTB("tmep_host", "TMEP host name", tmepHost, 50);
  WiFiManagerParameter tmepGuidTB("tmep_guid", "TMEP GUID", tmepGuid, 50);
  wm.addParameter(&tmepHostTB);
  wm.addParameter(&tmepGuidTB);

  if (!configLoaded) {
    Serial.println("Config load failed, starting configuration portal...");
    if (!wm.startConfigPortal(deviceId)) {
      Serial.println("Config portal failed, rebooting.");
      ESP.restart();
    }
  } else {
    Serial.println("Config load successful, connecting to WiFi...");
    if (!wm.autoConnect(deviceId)) {
      Serial.println("Config portal failed, rebooting.");
      ESP.restart();
    }
  }

  // We are connected
  Serial.print("Connected to network ");
  Serial.print(WiFi.SSID());
  Serial.print(", IP ");
  Serial.println(WiFi.localIP());
    
  // Save configuration if needed
  if (shouldSaveConfig) {
    strncpy(tmepHost, tmepHostTB.getValue(), sizeof(tmepHost));
    strncpy(tmepGuid, tmepGuidTB.getValue(), sizeof(tmepGuid));
    saveConfigFile();
  }

  // Start the DallasTemperature library
  sensors.begin();
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
  BearSSL::WiFiClientSecure client;
  client.setInsecure(); // Ignore invalid certificates, we are not able to validate chain correctly anyway

  Serial.printf("Requesting https://%s/?%s=%.2f...", tmepHost, tmepGuid, t);
  if (!client.connect(tmepHost, TMEP_PORT)) {
    Serial.println("Connect failed!");
    blinkLed(3);
    return false;
  }
  Serial.print("Connected...");
  client.printf("GET /?%s=%.2f HTTP/1.1\r\n", tmepGuid, t);
  client.printf("Host: %s\r\n", tmepHost);
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

void saveConfigFile() {
  // Create a JSON document
  StaticJsonDocument<512> json;
  json["tmepHost"] = tmepHost;
  json["tmepGuid"] = tmepGuid;

  // Open/create JSON file
  Serial.print("Opening " JSON_CONFIG_FILE "...");
  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
  if (!configFile) {
    Serial.println("Failed!");
    while (true) {
      blinkLed(1);
    };
  }
  Serial.println("OK");

  // Serialize JSON data to file
  Serial.print("Saving configuration...");
  if (serializeJson(json, configFile) == 0) {
    Serial.println("Failed!");
    while (true) {
      blinkLed(1);
    };
  }
  Serial.println("OK");

  // Close file
  configFile.close();
}

bool loadConfigFile() {
  // Read configuration from FS json
  Serial.print("Opening SPIFFS...");
  if (!SPIFFS.begin()) {
    Serial.println("Failed!");
    while (true) {
      blinkLed(1);
    };
  }
  Serial.println("OK");

  // Read existing file
  if (!SPIFFS.exists(JSON_CONFIG_FILE)) {
    Serial.println("Configuraiton file " JSON_CONFIG_FILE " not found.");
    return false;
  }
  Serial.print("Opening " JSON_CONFIG_FILE "...");
  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
  if (!configFile) {
    Serial.println("Failed!");
    return false;
  }
  Serial.println("OK");

  // Parse JSON
  Serial.print("Parsing JSON file...");
  StaticJsonDocument<512> json;
  DeserializationError error = deserializeJson(json, configFile);
  if (error) {
    Serial.println("Failed!");
    return false;
  }
  strcpy(tmepHost, json["tmepHost"]);
  strcpy(tmepGuid, json["tmepGuid"]);
  Serial.println("OK");
  return true;
}

void saveConfigCallback() {
  shouldSaveConfig = true;
}

void configModeCallback(WiFiManager* myWiFiManager) {
  Serial.println("Configuration mode:");
  Serial.print("  SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.print("  IP Address: ");
  Serial.println(WiFi.softAPIP());
}