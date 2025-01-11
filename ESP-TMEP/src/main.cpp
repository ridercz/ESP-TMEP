/*****************************************************************************
 * ESP-TMEP - ESP8266 firmware for sending temperatures to TMEP.CZ
 ******************************************************************************
 * (c) Michal A. Valasek, 2022-2025 | Licensed under terms of the MIT license
 * www.rider.cz | www.altair.blog | github.com/ridercz/ESP-TMEP
 *****************************************************************************/

#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include "WebServerConfig.h"

#ifdef SENSOR_DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

#define VERSION "3.0.0"                    // Version string
#define PIN_ONEWIRE D2                     // Pin where sensors are connected
#define PIN_LED LED_BUILTIN                // Pin where LED is connected
#define LED_INTERVAL 250                   // LED blink interval in ms
#define LOOP_INTERVAL 2000                 // Loop delay interval in ms
#define REBOOT_INTERVAL 104400000          // "Daily" reboot interval (29 hours, first prime after 24)
#define REMOTE_TIMEOUT 5000                // Remote server response timeout in ms
#define REMOTE_PORT 443                    // Remote server HTTPS port
#define REMOTE_HOST_DEFAULT "demo.tmep.cz" // Remote server name
#define REMOTE_SEND_INTERVAL 60000         // Interval in ms in which temperature is sent to server
#define JSON_CONFIG_FILE "/config-v4.json" // Configuration file name and version
#define PIN_LOCKOUT_LIMIT 3                // Number of PIN tries until lockout
#define WIFIMANAGER_DEBUG false            // Set to true to show WiFiManager debug messages
#define WIFIMANAGER_TIMEOUT 300            // Set timeout of the config portal in s (5 minutes)
#define ROLAVG_COUNT 30                    // Set number of rolling average measurements

// Declare function prototypes
void measureValues();
void handleHome();
void handleCss();
void handleApi();
void handleReset();
void handle404();
void sendCommonHttpHeaders();
bool sendValuesToRemoteServer(char *remoteHost);
void blinkLed(int count);
void saveConfigFile();
bool loadConfigFile();
void deleteConfigFile();
void saveParamsCallback();
void configModeCallback(WiFiManager *myWiFiManager);

// Define configuration variables
char remoteHost1[100] = REMOTE_HOST_DEFAULT;
char remoteHost2[100] = "";
char remoteHost3[100] = "";
char configPin[20];

// Define library static instances
#ifdef SENSOR_DS18B20
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature sensors(&oneWire);
#endif
WiFiManager wm;
ESP8266WebServer server(HTTP_PORT);

// Define variables
unsigned long nextSendTime = 0;
unsigned long nextLoopTime = 0;
unsigned long resetTime = 0;
bool resetRequested = false;
char deviceId[20];
int pinTriesRemaining = PIN_LOCKOUT_LIMIT;
int rolavg_index = 0;
bool rolavg_first = true;
float rolavg_values_temperature[ROLAVG_COUNT];
float avgTemp;
#ifdef SUPPORT_HUMIDITY
float rolavg_values_humidity[ROLAVG_COUNT];
float avgHumidity;
#endif
#ifdef SUPPORT_PRESSURE
float rolavg_values_pressure[ROLAVG_COUNT];
float avgPressure;
#endif

// WiFiManager parameters
WiFiManagerParameter remoteHost1TB("remote_host_1", "Remote host #1", remoteHost1, sizeof(remoteHost1));
WiFiManagerParameter remoteHost2TB("remote_host_2", "Remote host #2", remoteHost2, sizeof(remoteHost2));
WiFiManagerParameter remoteHost3TB("remote_host_3", "Remote host #3", remoteHost3, sizeof(remoteHost3));
WiFiManagerParameter configPinTB("config_pin", "Configuration PIN", configPin, sizeof(configPin));

// Initialization

void setup()
{
  // Init serial port and print welcome message
  Serial.begin(9600);
  Serial.println();
  delay(1000);
  Serial.println();
  Serial.println("ESP-TMEP/" VERSION " | https://github.com/ridercz/ESP-TMEP");
  Serial.println("(c) Michal A. Valasek, 2022-2025 | www.altair.blog | www.rider.cz");
  Serial.println();

  // Show device ID
  sprintf(deviceId, "ESP-TMEP-%08X", ESP.getChipId());
  Serial.printf("Device ID: %s\n", deviceId);

  // Show sensor type
#ifdef SENSOR_DS18B20
  Serial.println("Sensor type: DS18B20");
#endif
  Serial.println();

  // Setup LED
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, false); // LED on

  // Generate random config PIN
  itoa(random(1000000, 99999999), configPin, DEC);
  configPinTB.setValue(configPin, sizeof(configPin));

  // Read configuration from LittleFS
  bool configLoaded = loadConfigFile();

  // Configure WiFiManager options
  wm.setDebugOutput(WIFIMANAGER_DEBUG);
  wm.setSaveParamsCallback(saveParamsCallback);
  wm.setAPCallback(configModeCallback);
  wm.addParameter(&remoteHost1TB);
  wm.addParameter(&remoteHost2TB);
  wm.addParameter(&remoteHost3TB);
  wm.addParameter(&configPinTB);
  const char *menu[] = {"wifi"};
  wm.setMenu(menu, 1);

  // Set configuration portal timeout
  wm.setConfigPortalTimeout(WIFIMANAGER_TIMEOUT);

  if (!configLoaded)
  {
    Serial.println("Config load failed, starting configuration portal...");
    if (!wm.startConfigPortal(deviceId))
    {
      Serial.println("Config portal failed, rebooting.");
      ESP.restart();
    }
    Serial.println("Rebooting after exiting configuration portal...");
    ESP.restart();
  }
  else
  {
    Serial.println("Config load successful, connecting to WiFi...");
    if (!wm.autoConnect(deviceId))
    {
      Serial.println("Config portal failed or timed out, rebooting.");
      ESP.restart();
    }
  }

  // We are connected
  Serial.print("Connected to network ");
  Serial.print(WiFi.SSID());
  Serial.print(", IP ");
  Serial.println(WiFi.localIP());

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

// Main loop

void loop()
{
  unsigned long millisNow = millis();

  // Process HTTP requests
  server.handleClient();

  // Reset if requested or if it's after reboot interval
  if (millisNow >= REBOOT_INTERVAL || (resetRequested && millisNow > resetTime))
    ESP.reset();

  // Check if it's time to do more work
  if (millisNow < nextLoopTime)
    return;

  // Blink LED once
  blinkLed(1);

// Measure values
  measureValues();

  // Send values to remote server(s)
  if (millisNow > nextSendTime)
  {
    sendValuesToRemoteServer(remoteHost1);
    sendValuesToRemoteServer(remoteHost2);
    sendValuesToRemoteServer(remoteHost3);

    // Schedule next send
    nextSendTime = millisNow + REMOTE_SEND_INTERVAL;
  }

  // Schedule next loop
  nextLoopTime = millis() + LOOP_INTERVAL;
}

// Measurement and averaging

#ifdef SENSOR_DS18B20
void measureValues()
{
  // Measure temperature
  DeviceAddress addr;
  sensors.requestTemperatures();
  if (sensors.getAddress(addr, 0))
  {
    float curTemp = sensors.getTempC(addr);

    // Compute rolling average value
    rolavg_values_temperature[rolavg_index] = curTemp;
    int valueCount = rolavg_first ? rolavg_index + 1 : ROLAVG_COUNT;
    float valueTotal = 0;
    for (int i = 0; i < valueCount; i++)
    {
      valueTotal += rolavg_values_temperature[i];
    }
    avgTemp = valueTotal / valueCount;
    rolavg_index++;
    if (rolavg_index == ROLAVG_COUNT)
    {
      rolavg_index = 0;
      rolavg_first = false;
    }

    Serial.printf("Current temperature = %.2f C, Rolling average = %.2f C across %i values\n", curTemp, avgTemp, valueCount);
  }
  else
  {
    // Failed to measure temperature - blink twice
    Serial.print("Temperature: Error!\n");
    blinkLed(1);
    return;
  }
}
#endif

// HTTP server URI handlers

void handleHome()
{
  Serial.println("Serving URI /");

  String html = HTML_HEADER;
  html += "<h1>Current values</h1>\n";
  html += "<article>\n\t<header>Temperature</header>\n\t<p>";
  html += avgTemp;
  html += " &deg;C</p>\n</article>";

#ifdef SUPPORT_HUMIDITY
  html += "<article>\n\t<header>Humidity</header>\n\t<p>";
  html += avgHumidity;
  html += " % RH</p>\n</article>";
#endif

#ifdef SUPPORT_PRESSURE
  html += "<article>\n\t<header>Pressure</header>\n\t<p>";
  html += avgPressure;
  html += " hPa</p>\n</article>";
#endif

  html += "<article>\n\t<header>Signal Strength</header>\n\t<p>";
  html += WiFi.RSSI();
  html += " dBm</p>\n</article>";
  html += HTML_HOME;
  html += HTML_FOOTER;

  sendCommonHttpHeaders();
  server.send(200, "text/html", html);
}

void handleCss()
{
  Serial.println("Serving URI /styles.css");

  sendCommonHttpHeaders();
  server.send(200, "text/css", HTML_CSS);
}

void handleApi()
{
  Serial.println("Serving URI /api");

  // Add temperature to JSON
  String json = "{\n\t\"temp\" : ";
  json += avgTemp;

  // Add signal strength
  int32_t rssi = WiFi.RSSI();
  json += ",\n\t\"rssi\" : ";
  json += rssi;

// Add optional values
#ifdef SUPPORT_HUMIDITY
  json += ",\n\t\"humi\" : ";
  json += avgHumidity;
#endif

#ifdef SUPPORT_PRESSURE
  json += ",\n\t\"pres\" : ";
  json += avgPressure;
#endif

  // Add sensor type
#ifdef SENSOR_DS18B20
  json += ",\n\t\"sensorType\" : \"DS18B20\"";
#endif

  // Add device ID and version
  json += ",\n\t\"deviceId\" : \"";
  json += deviceId;
  json += "\",\n\t\"version\" : \"" VERSION "\"\n}";

  Serial.println("Serving URI /api");
  sendCommonHttpHeaders();
  server.send(200, "application/json", json);
}

void handle404()
{
  Serial.print("Serving 404 for URI ");
  Serial.println(server.uri());

  String html = HTML_HEADER;
  html += HTML_404;
  html += HTML_FOOTER;

  sendCommonHttpHeaders();
  server.send(404, "text/html", html);
}

void handleReset()
{
  Serial.println("Serving URI /reset");

  // Generate page header
  String html = HTML_HEADER;
  html += "<h1>Configuration reset</h1>";

  // Compare PIN
  String candidatePin = server.arg("pin");
  String pin = String(configPin);
  bool performReset = pinTriesRemaining > 0 && pin.equals(candidatePin);

  if (performReset)
  {
    Serial.println("Correct PIN entered, reseting to configuration mode...");
    html += "<p>System will reset to configuration mode. Connect to the following WiFi network:</p><p><code>";
    html += deviceId;
    html += "</code></p>";
  }
  else if (pinTriesRemaining == 0)
  {
    Serial.println("Incorrect PIN entered, system locked");
    html += "<p>Incorrect PIN was entered. System is locked until next reboot.</p>";
    html += "<p class=\"link\"><a href=\"/\">Back</a></p>";
  }
  else
  {
    pinTriesRemaining--;
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
  if (performReset)
  {
    deleteConfigFile();
    resetRequested = true;
    resetTime = millis() + 5000; // Reset in 5 s
  }
}

void sendCommonHttpHeaders()
{
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Server", "ESP-TMEP/" VERSION);
}

// Remote server communication

bool sendValuesToRemoteServer(char *remoteHost)
{
  if (*remoteHost == '\0')
    return true;

  // Measure WiFi signal strength
  int32_t rssi = WiFi.RSSI();

  // Construct the URL path
  char remotePath[100];
  sprintf(remotePath, "/?temp=%.2f&rssi=%i", avgTemp, rssi);
#ifdef SUPPORT_HUMIDITY
  char humPath[20];
  sprintf(humPath, "&humi=%.2f", avgHumidity);
  strcat(remotePath, humPath);
#endif
#ifdef SUPPORT_PRESSURE
  char pressPath[20];
  sprintf(pressPath, "&pres=%.2f", avgPressure);
  strcat(remotePath, pressPath);
#endif

  // Create a new client
  BearSSL::WiFiClientSecure client;

  // Ignore invalid certificates, we are not able to validate chain correctly anyway on ESP8266
  client.setInsecure();

  Serial.printf("Requesting https://%s%s...", remoteHost, remotePath);
  if (!client.connect(remoteHost, REMOTE_PORT))
  {
    Serial.println("Connect failed!");
    blinkLed(2);
    return false;
  }
  Serial.print("Connected...");
  client.printf("GET %s HTTP/1.1\r\n", remotePath);
  client.printf("Host: %s\r\n", remoteHost);
  client.printf("Device-Id: %s\r\n", deviceId);
  client.print("User-Agent: ESP-TMEP/" VERSION " (https://github.com/ridercz/ESP-TMEP)\r\n");
  client.print("Connection: close\r\n");
  client.print("\r\n");
  unsigned long timeout = millis() + REMOTE_TIMEOUT;
  while (client.available() == 0)
  {
    if (millis() > timeout)
    {
      Serial.println("Timeout!");
      client.stop();
      blinkLed(3);
      return false;
    }
  }
  Serial.println("OK");
  return true;
}

// Helper methods

void blinkLed(int count)
{
  for (int i = 0; i < count; i++)
  {
    digitalWrite(PIN_LED, false);
    delay(LED_INTERVAL);
    digitalWrite(PIN_LED, true);
    delay(LED_INTERVAL);
  }
}

// Configuration file operations

void saveConfigFile()
{
  // Create a JSON document
  JsonDocument json;
  json["remoteHost1"] = remoteHost1;
  json["remoteHost2"] = remoteHost2;
  json["remoteHost3"] = remoteHost3;
  json["configPin"] = configPin;

  // Open/create JSON file
  Serial.print("Opening " JSON_CONFIG_FILE "...");
  File configFile = LittleFS.open(JSON_CONFIG_FILE, "w");
  if (!configFile)
  {
    Serial.println("Failed!");
    while (true)
    {
      blinkLed(1);
    };
  }
  Serial.println("OK");

  // Serialize JSON data to file
  Serial.print("Saving configuration...");
  if (serializeJson(json, configFile) == 0)
  {
    Serial.println("Failed!");
    while (true)
    {
      blinkLed(1);
    };
  }
  Serial.println("OK");

  // Close file
  configFile.close();
}

bool loadConfigFile()
{
  // Read configuration from FS json
  Serial.print("Opening LittleFS...");
  if (!LittleFS.begin())
  {
    Serial.println("Failed!");
    while (true)
    {
      blinkLed(1);
    };
  }
  Serial.println("OK");

  // Read existing file
  if (!LittleFS.exists(JSON_CONFIG_FILE))
  {
    Serial.println("Configuration file " JSON_CONFIG_FILE " not found.");
    return false;
  }
  Serial.print("Opening " JSON_CONFIG_FILE "...");
  File configFile = LittleFS.open(JSON_CONFIG_FILE, "r");
  if (!configFile)
  {
    Serial.println("Failed!");
    return false;
  }
  Serial.println("OK");

  // Parse JSON
  Serial.print("Parsing JSON file...");
  JsonDocument json;
  DeserializationError error = deserializeJson(json, configFile);
  if (error)
  {
    Serial.println("Failed!");
    return false;
  }
  strcpy(remoteHost1, json["remoteHost1"]);
  strcpy(remoteHost2, json["remoteHost2"]);
  strcpy(remoteHost3, json["remoteHost3"]);
  strcpy(configPin, json["configPin"]);
  Serial.println("OK");
  return true;
}

void deleteConfigFile()
{
  Serial.print("Opening LittleFS...");
  if (!LittleFS.begin())
  {
    Serial.println("Failed!");
    while (true)
    {
      blinkLed(1);
    };
  }
  Serial.println("OK");

  Serial.print("Deleting " JSON_CONFIG_FILE "...");
  LittleFS.remove(JSON_CONFIG_FILE);
  Serial.println("OK");
}

// WiFiManager callbacks

void saveParamsCallback()
{
  Serial.println("Getting configuration from portal...");
  strncpy(remoteHost1, remoteHost1TB.getValue(), sizeof(remoteHost1));
  strncpy(remoteHost2, remoteHost2TB.getValue(), sizeof(remoteHost2));
  strncpy(remoteHost3, remoteHost3TB.getValue(), sizeof(remoteHost3));
  strncpy(configPin, configPinTB.getValue(), sizeof(configPin));

  Serial.println("Saving configuration to JSON...");
  saveConfigFile();
}

void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Configuration mode:");
  Serial.print("  SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.print("  IP Address: ");
  Serial.println(WiFi.softAPIP());
}