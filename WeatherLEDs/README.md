# WeatherLEDs

Display daily weather forecasts on a LED stripe connected to an ESP8266. [Powered by Dark Sky](https://darksky.net/poweredby/) and [WiFiManager](https://github.com/tzapu/WiFiManager).

## Requirements
- An ESP8266 board such as NodeMCU, Adafruit Huzzah Feather or WeMos D1 mini.
- A [NeoPixel](https://learn.adafruit.com/adafruit-neopixel-uberguide/neopixel-strips) compatible LED stripe with 8 LEDs.
- Arduino IDE
- A [darksky API account](https://darksky.net/dev)
- [ESP8266 core for Arduino](https://github.com/esp8266/Arduino)
- Arduino libraries (install thru library manager):
  - Adafruit NeoPixel
  - ArduinoJson 5.x

## How to use
1. Enter your location and the API key into the code.
1. Connect the LEDs to pin 14 (or change the pin in code).
1. Flash the code with Arduino.
1. Connect to the WiFi network WEATHERLEDS and set up your local WiFi.

## Colors
Blue means rain, it flickers more for heavier rain. Fading white is cloudy, a darker steady white means fog. It should be pretty self-explanatory.
