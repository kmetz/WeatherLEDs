#include <ESP8266WiFi.h>          // ESP8266 Core WiFi Library
#include <ESP8266HTTPClient.h>    // ESP8266 HTTP Client Library
#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <Adafruit_NeoPixel.h>    // LED
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson
#include <Ticker.h>


// Settings ------------------------------------------------

// Pin where the LED stripe data port is connected.
#define LED_PIN 14

// Overall brightness of the LEDs.
#define BRIGHTNESS 100 // 255 max.

// Put your location here (lat,lon).
String latlon = "52.52,13.40"; // Berlin, Germany

// Register for the darsky API at https://darksky.net/dev
String darksky_apikey = "--YOUR API KEY--";

// ---------------------------------------------------------


String api_url = "https://api.darksky.net/forecast/" + darksky_apikey + "/" + latlon + "/?exclude=currently,hourly,flags,alerts&units=ca";
String api_fingerprint = "EB:C2:67:D1:B1:C6:77:90:51:C1:4A:0A:BA:83:E1:F0:6D:73:DD:B8";
#define NUM_DAYS 8

WiFiManager wifiManager;
WiFiClient wifiClient;
Adafruit_NeoPixel leds;
Ticker updateTicker;
HTTPClient http;

struct conditions {
  char icon[32];
  double cloudCover;
  double windSpeed;
  double precipIntensity;
};

struct conditions days[NUM_DAYS];


void setup() {
  Serial.begin(115200);
  Serial.println("Hi.");
  
  leds = Adafruit_NeoPixel(NUM_DAYS, LED_PIN, NEO_GRB + NEO_KHZ800);
  leds.setBrightness(BRIGHTNESS);
  leds.begin();
  leds.clear();
  leds.show();
  
  wifiManager.autoConnect("WEATHERLEDS");
  Serial.println("WiFi Connected.");

  updateWeatherData();

  // Update every 2 hours.
  // TODO fix, actually crashes and triggers a wdt reset.
  updateTicker.attach(60*60*2, updateWeatherData);
}


void loop() {
  unsigned long time = millis();
  for (int i=0; i<NUM_DAYS; i++) {
    animateLed(i, time);
  }
  leds.show();
}


void updateWeatherData() {
  Serial.println("Getting weather data ...");

  Serial.println(api_url);
  http.begin(api_url, api_fingerprint);
  int httpCode = http.GET();
  Serial.println(httpCode);
  if (httpCode != 200) {
    Serial.println("An HTTP Error occured.");
  } else {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(http.getString());
    JsonObject& daily = root["daily"];
    JsonArray& data = daily["data"];
    
    for (unsigned int i=0; i<NUM_DAYS; i++) {
      JsonObject& dayData = data[i];
      conditions day;
      
      strncpy(day.icon, dayData["icon"], sizeof(day.icon));
      day.cloudCover = dayData.get<double>("cloudCover");
      day.windSpeed = dayData.get<double>("windSpeed");
      day.precipIntensity = dayData.get<double>("precipIntensity");
      days[i] = day;
      
      Serial.println(String(i) + ": " + day.icon + "|cc:" + day.cloudCover + "|ws:" + day.windSpeed + "|pi:" + day.precipIntensity);
    }
    Serial.println("Weather data updated.");
  }
  http.end();
}


void animateLed(unsigned int led, unsigned long time) {
  conditions day = days[led];

  // icon
  // A machine-readable text summary of this data point, suitable for selecting an icon for display.
  // If defined, this property will have one of the following values:
  // clear-day, clear-night, rain, snow, sleet, wind, fog, cloudy, partly-cloudy-day, or partly-cloudy-night.
  // (Developers should ensure that a sensible default is defined, as additional values, such as hail,
  // thunderstorm, or tornado, may be defined in the future.)

  // Clear
  if (strcmp(day.icon, "clear-day") == 0
   || strcmp(day.icon, "clear-night") == 0) {
    leds.setPixelColor(led, leds.Color(255, 200, 0));
  }

  // Rain
  else if (strcmp(day.icon, "rain") == 0) {
    leds.setPixelColor(led, leds.Color(
      random(0, 32),
      random(0, 32),
      random(map(day.precipIntensity, 0.0, 1.0, 255, 0), 255)
    ));
  }

  // Snow
  else if (strcmp(day.icon, "snow") == 0) {
    int level = random(200, 255);
    leds.setPixelColor(led, leds.Color(level, level, level));
  }
  else if (strcmp(day.icon, "sleet") == 0) {
    leds.setPixelColor(led, leds.Color(
      random(110, 130),
      random(110, 130),
      random(200, 255)
    ));
  }

  // Wind
  else if (strcmp(day.icon, "wind") == 0) {
    int phase = 128 + 127 * cos(2 * PI / max(300.0, 1500 - 10*day.windSpeed) * (time + (1000 * (double(led) / NUM_DAYS))));
    int level = map(phase, 0, 255, 100, 200);
    leds.setPixelColor(led, leds.Color(level, level, level));
  }

  // Fog
  else if (strcmp(day.icon, "fog") == 0) { 
    leds.setPixelColor(led, leds.Color(32, 32, 32));
  }

  // Cloudy
  else if (strcmp(day.icon, "cloudy") == 0) {
    int phase = 128 + 127 * cos(2 * PI / 1500 * (time + (1000 * (double(led) / NUM_DAYS))));
    int level = map(phase, 0, 255, 100, 200);
    leds.setPixelColor(led, leds.Color(level, level, level));
  }

  // Partly cloudy
  else if (strcmp(day.icon, "partly-cloudy-day") == 0
        || strcmp(day.icon, "partly-cloudy-night") == 0) {
    int phase = 128 + 127 * cos(2 * PI / 1500 * time + (1000 * (double(led) / NUM_DAYS)));
    double cc = day.cloudCover;
    leds.setPixelColor(led, leds.Color(
      map(phase, 0, 255, 100, 255),
      map(phase, 0, 255, 100, 200 + cc*55),
      map(phase, 0, 255, 100, cc*255)
    ));
  }

  // Unknown
  else {
    leds.setPixelColor(led, leds.Color(255, 0, 0));
  }
}
