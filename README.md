# ESP-TMEP

ESP-TMEP is firmware for ESP8266, which can measure temperature using DS18B20 and send it to the [TMEP.CZ](https://www.tmep.cz/) online service or any other service that accepts HTTP GET requests.

> Build video and basic information about this project (in Czech) is [on my blog](https://www.altair.blog/2022/09/esp-tmep).

## Features and limitations

### Features

* Support for common DS18B20 temperature sensors.
* User-friendly deployment (no need to modify code, easy configuration via web browser).

### Limitations

* No server certificate validation is performed, because on ESP8266 it's practically impossible.
* Internal web server is HTTP only; again, because proper HTTPS is almost impossible on ESP8266.
* Some parameters are still hardcoded:
    * Remote server protocol (always uses HTTPS).
    * Interval in which data are measured and sent (and if they are to be sent at all).
    * Remote server timeout.

## Usage

### Configuration mode

After first startup, the device boots into configuration mode. It creates an Wi-Fi access point named `ESP-TMEP-xxxxxxxx` (where `xxxxxxxx` stands for unique chip ID). After connecting, you'll see the configuration UI. In addition to network name and password, you can configure the following options:

Name              | Example        | Size | Description
----------------- | -------------- | ---: | ------------------------------------
Remote host name  | `demo.tmep.cz` | 100  | Host name part of the target URI
Remote host path  | `/?temp=`      | 100  | Path part of the target URI
Configuration PIN | `AbXy1234`     | 20   | PIN/password for configuration reset

The target URI is constructed by joining the parameters and measured value in °C:

Scheme     | Host           | Path      | Value
---------- | -------------- | --------- | ------
`https://` | `demo.tmep.cz` | `/?temp=` | `22.63`

### Operational mode

Device then restarts to operational mode. In this mode, in addition to sending the measurement to remote servers, runt a local web server, offering the following services:

URI              | Description
---------------- | ------------------------------------
`/`              | Homepage showing current temperature
`/api`           | JSON API endpoint
`/reset?pin=XXX` | Switch to configuration mode

### JSON API

The response has the following format:

```json
{
    "temp" : 22.63,
    "deviceId" : "ESP-TMEP-xxxxxxxx",
    "version" : "2.0.0"
}
```

* `temp` is last measured temperature in °C
* `deviceId` is unique device identifier (`xxxxxxxx` is chip ID)
* `version` is firmware version

### LED status

Device status is indicated by the builtin LED. There is a number of 250 ms flashes rougly once per second:

Flashes       | Meaning
------------- | -----------------------------------
1             | Normal operation
2             | Sensor error
3             | Remote server connection error
4             | Remote server connection timeout
permanent on  | Wi-Fi connecting or config required
fast blinking | Internal error in SPIFFS

## Hardware

Any board with ESP8266 chip can be used. I'm using LOLIN/Wemos D1 mini.

Hardware connections (the _wire color_ column refers to common wire colors of the waterproofed version of DS18B20):

D1 Mini | DS18B20 | Wire color
------- | ------- | ----------
G       | GND     | black
5V      | VDD     | red
D2      | DQ      | yellow

Additionally connect 4k7 resistor between VDD/5V and DQ/D2.

![Board photo](ESP-TMEP-photo-01.jpg)

![Board photo](ESP-TMEP-photo-02.jpg)

## Case

I'm using this [WeMos D1 mini Enclosure](https://www.printables.com/model/44083-wemos-d1-mini-enclosure) by [100prznt](https://www.printables.com/social/23641-100prznt/about).

![Enclosure photo](ESP-TMEP-photo-03.jpg)

![Enclosure photo](ESP-TMEP-photo-04.jpg)

![Enclosure photo](ESP-TMEP-photo-05.jpg)

## Software

This firmware is using ESP8266 Arduino Core 2.0. The following external libraries are required:

* [OneWire](https://www.pjrc.com/teensy/td_libs_OneWire.html)
* [DallasTemperature](https://github.com/milesburton/Arduino-Temperature-Control-Library)
* [ArduinoJson](https://arduinojson.org/)
* [WiFiManager](https://github.com/tzapu/WiFiManager)