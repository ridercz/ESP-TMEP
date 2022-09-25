# ESP-TMEP

ESP-TMEP is firmware for ESP8266, which can measure temperature using DS18B20 and send it to the [TMEP.CZ](https://www.tmep.cz/) online service.

## Features and limitations

### Features

* Support for common DS18B20 temperature sensors.
* User-friendly deployment (no need to modify code, easy configuration via web browser).

### Limitations

* No server certificate validation is performed, because on ESP8266 it's impossible.
* After initial configuration there is no way how to force the config UI, if the sensor can connecto to WiFi.

## LED status

Device status is indicated by the builtin LED. There is a number of 250 ms flashes rougly once per second:

Flashes       | Meaning
------------- | -----------------------------------
1             | normal operation
2             | sensor error
3             | TMEP server connection error
4             | TMEP server connection timeout
permanent on  | Wi-Fi connecting or config required
fast blinking | internal error in SPIFFS

## Hardware

Any board with ESP8266 chip can be used. I'm using LOLIN/Wemos D1 mini.

Hardware connections (the _wire color_ column refers to common wire colors of the waterproofed version of DS18B20):

D1 Mini | DS18B20 | Wire color
------- | ------- | ----------
G       | GND     | black
5V      | VDD     | red
D2      | DQ      | yellow

Additionally connect 4k7 resistor between VDD/5V and DQ/D2.

## Case

I'm using this [WeMos D1 mini Enclosure](https://www.printables.com/model/44083-wemos-d1-mini-enclosure) by [100prznt](https://www.printables.com/social/23641-100prznt/about).

## Software

This firmware is using ESP8266 Arduino Core 2.0. The following external libraries are required:

* [OneWire](https://www.pjrc.com/teensy/td_libs_OneWire.html)
* [DallasTemperature](https://github.com/milesburton/Arduino-Temperature-Control-Library)
* [ArduinoJson](https://arduinojson.org/)
* [WiFiManager](https://github.com/tzapu/WiFiManager)