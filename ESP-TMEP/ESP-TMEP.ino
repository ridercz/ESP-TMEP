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
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include "WebServerConfig.h"

#define VERSION "2.0.0"                              // Version string
#define PIN_ONEWIRE D2                               // Pin where sensors are connected
#define PIN_LED LED_BUILTIN                          // Pin where LED is connected
#define LED_INTERVAL 250                             // LED blink interval
#define LOOP_INTERVAL 2000                           // Loop delay interval
#define REMOTE_TIMEOUT 5000                          // Remote server response timeout
#define REMOTE_PORT 443                              // Remote server HTTPS port
#define REMOTE_HOST_DEFAULT "demo.tmep.cz"           // Remote server name
#define REMOTE_PATH_DEFAULT "/?temp="                // Remote server path prefix
#define REMOTE_SEND_INTERVAL 60000                   // Interval in ms in which temperature is sent to server
#define JSON_CONFIG_FILE "/config-" VERSION ".json"  // SPIFFS configuration file name
#define PIN_LOCKOUT_LIMIT 3                          // Number of PIN tries until lockout
#define WIFIMANAGER_DEBUG false                      // Set to true to show WiFiManager debug messages

// Define configuration variables
char remoteHost[100] = REMOTE_HOST_DEFAULT;
char remotePath[100] = REMOTE_PATH_DEFAULT;
char configPin[20];

// Define library static instances
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature sensors(&oneWire);
WiFiManager wm;
ESP8266WebServer server(HTTP_PORT);

// Define variables
DeviceAddress lastDeviceAddress;
bool shouldSaveConfig = false;
unsigned long nextSendTime = 0;
unsigned long nextLoopTime = 0;
unsigned long resetTime = 0;
bool resetRequested = false;
float lastTemp;
char deviceId[20];
int pinTriesRemaining = PIN_LOCKOUT_LIMIT;

void setup() {
  // Init serial port
  Serial.begin(9600);
  Serial.println();
  Serial.println();
  Serial.println("ESP-TMEP/" VERSION " | https://github.com/ridercz/ESP-TMEP");
  Serial.println("(c) Michal A. Valasek, 2022 | www.altair.blog | www.rider.cz");
  Serial.println();

  // Get device ID
  sprintf(deviceId, "ESP-TMEP-%08X", ESP.getChipId());
  Serial.printf("Device ID: %s\n\n", deviceId);

  // Setup LED
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, false);  // LED on

  // Generate random config PIN
  itoa(random(1000000, 99999999), configPin, DEC);

  // Read configuration from SPIFFS
  bool configLoaded = loadConfigFile();

  // Configure WiFiManager options
  wm.setDebugOutput(WIFIMANAGER_DEBUG);
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setAPCallback(configModeCallback);
  WiFiManagerParameter remoteHostTB("remote_host", "Remote host name", remoteHost, sizeof(remoteHost));
  WiFiManagerParameter remotePathTB("remote_path", "Remote path", remotePath, sizeof(remotePath));
  WiFiManagerParameter configPinTB("config_pin", "Configuration PIN", configPin, sizeof(configPin));
  wm.addParameter(&remoteHostTB);
  wm.addParameter(&remotePathTB);
  wm.addParameter(&configPinTB);

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
    strncpy(remoteHost, remoteHostTB.getValue(), sizeof(remoteHost));
    strncpy(remotePath, remotePathTB.getValue(), sizeof(remotePath));
    strncpy(configPin, configPinTB.getValue(), sizeof(configPin));
    saveConfigFile();

    // Restart after configuration changes
    ESP.restart();
  }

  // Start the DallasTemperature library
  sensors.begin();

  // Configure HTTP server
  Serial.print("Starting HTTP server...");
  server.on("/", handleHome);
  server.on("/styles.css", handleCss);
  server.on("/api", handleApi);
  server.on("/reset", handleReset);
  server.onNotFound(handle404);
  server.begin();
  Serial.println("OK");
  Serial.printf("Configuration PIN: %s\n", configPin);
}

void loop() {
  // Process HTTP requests
  server.handleClient();

  // Reset if requested
  if (resetRequested && millis() > resetTime) ESP.reset();

  // Check if it's time to do more work
  if (millis() < nextLoopTime) return;

  // Blink LED once
  blinkLed(1);

  // Measure temperature
  DeviceAddress addr;
  sensors.requestTemperatures();
  if (sensors.getAddress(addr, 0)) {
    lastTemp = sensors.getTempC(addr);
    Serial.printf("Temperature: %.2f\n", lastTemp);
  } else {
    // Failed to measure temperature - blink twice
    Serial.print("Temperature: Error!\n");
    blinkLed(1);
    return;
  }

  // Send temperature to remote server
  if (millis() > nextSendTime) {
    if (!sendValueToRemoteServer()) return;
    nextSendTime = millis() + REMOTE_SEND_INTERVAL;
  }

  // Schedule next loop
  nextLoopTime = millis() + LOOP_INTERVAL;
}

void handleHome() {
  Serial.println("Serving URI /");

  String html = HTML_HEADER;
  html += "<h1>Current temperature</h1>";
  html += "<p class=\"curtemp\">";
  html += lastTemp;
  html += " &deg;C</p>";
  html += HTML_HOME;
  html += HTML_FOOTER;

  sendCommonHttpHeaders();
  server.send(200, "text/html", html);
}

void handleCss() {
  Serial.println("Serving URI /styles.css");

  sendCommonHttpHeaders();
  server.send(200, "text/css", HTML_CSS);
}

void handleApi() {
  Serial.println("Serving URI /api");

  String json = "{\n\t\"temp\" : ";
  json += lastTemp;
  json += ",\n\t\"deviceId\" : \"";
  json += deviceId;
  json += "\",\n\t\"version\" : \"" VERSION "\"\n}";

  Serial.println("Serving URI /api");
  sendCommonHttpHeaders();
  server.send(200, "application/json", json);
}

void handle404() {
  Serial.print("Serving 404 for URI ");
  Serial.println(server.uri());
  String html = HTML_HEADER;
  html += HTML_404;
  html += HTML_FOOTER;

  sendCommonHttpHeaders();
  server.send(404, "text/html", html);
}

void handleReset() {
  Serial.print("Serving URI /reset");

  // Generate page header
  String html = HTML_HEADER;
  html += "<h1>Configuration reset</h1>";

  // Compare PIN
  String candidatePin = server.arg("pin");
  String pin = String(configPin);
  bool performReset = pinTriesRemaining > 0 && pin.equals(candidatePin);

  if (performReset) {
    Serial.print("Correct PIN entered, reseting to configuration mode...");
    html += "<p>System will reset to configuration mode. Connect to the following WiFi network:</p><p><code>";
    html += deviceId;
    html += "</code></p>";
  } else {
    if (pinTriesRemaining > 0) pinTriesRemaining--;
    Serial.printf("Incorrect PIN entered, %i tries remaining\n", pinTriesRemaining);
    html += "<p>Incorrect PIN was entered. ";
    html += pinTriesRemaining;
    html += " tries remaining.</p>";
    html += "<p class=\"link\"><a href=\"/\">Back</a></p>";
  }

  // Generate page footer
  html += HTML_FOOTER;

  // Send response
  sendCommonHttpHeaders();
  server.send(200, "text/html", html);

  // Reset configuration if successfull
  if (performReset) {
    deleteConfigFile();
    resetRequested = true;
    resetTime = millis() + 5000;  // Reset in 5 s
  }
}

void sendCommonHttpHeaders() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Server", "ESP-TMEP/" VERSION);
}

bool sendValueToRemoteServer() {
  BearSSL::WiFiClientSecure client;
  client.setInsecure();  // Ignore invalid certificates, we are not able to validate chain correctly anyway

  Serial.printf("Requesting https://%s%s%.2f...", remoteHost, remotePath, lastTemp);
  if (!client.connect(remoteHost, REMOTE_PORT)) {
    Serial.println("Connect failed!");
    blinkLed(2);
    return false;
  }
  Serial.print("Connected...");
  client.printf("GET %s%.2f HTTP/1.1\r\n", remotePath, lastTemp);
  client.printf("Host: %s\r\n", remoteHost);
  client.print("Connection: close\r\n");
  client.print("User-Agent: ESP-TMEP/" VERSION " (https://github.com/ridercz/ESP-TMEP)\r\n");
  client.print("\r\n");
  unsigned long timeout = millis() + REMOTE_TIMEOUT;
  while (client.available() == 0) {
    if (millis() > timeout) {
      Serial.println("Timeout!");
      client.stop();
      blinkLed(3);
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
  json["remoteHost"] = remoteHost;
  json["remotePath"] = remotePath;
  json["configPin"] = configPin;

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
    Serial.println("Configuration file " JSON_CONFIG_FILE " not found.");
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
  strcpy(remoteHost, json["remoteHost"]);
  strcpy(remotePath, json["remotePath"]);
  strcpy(configPin, json["configPin"]);
  Serial.println("OK");
  return true;
}

void deleteConfigFile() {
  Serial.print("Opening SPIFFS...");
  if (!SPIFFS.begin()) {
    Serial.println("Failed!");
    while (true) {
      blinkLed(1);
    };
  }
  Serial.println("OK");

  Serial.print("Deleting " JSON_CONFIG_FILE "...");
  SPIFFS.remove(JSON_CONFIG_FILE);
  Serial.println("OK");
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