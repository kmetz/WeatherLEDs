#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#else
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#endif
#include <Arduino.h>
#include <NeoPixelBus.h>
#include <ArduinoJson.h>
#include <Ticker.h>

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))


// Settings ------------------------------------------------
const char* ssid     = "--SSID--";
const char* password = "--PASSWORD--";

// Pin where the LED stripe data port is connected.
#define LED_PIN 14

// Put your location here (lat,lon).
#define LAT_LON "52.52,13.40" // Berlin, Germany

// Register for the darsky API at https://darksky.net/dev
#define DARKSKY_APIKEY  = "--YOUR API KEY--";
// ---------------------------------------------------------


#define API_URL "https://api.darksky.net/forecast/" DARKSKY_APIKEY "/" LAT_LON "/?exclude=currently,hourly,flags,alerts&units=ca"
#define API_FINGERPRINT "EB:C2:67:D1:B1:C6:77:90:51:C1:4A:0A:BA:83:E1:F0:6D:73:DD:B8"
#define NUM_DAYS 8


WiFiClient wifiClient;
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> leds(NUM_DAYS, LED_PIN);
Ticker updateTicker;

struct conditions {
  char icon[32];
  double cloudCover;
  double windSpeed;
  double precipIntensity;
};

struct conditions days[NUM_DAYS];
boolean needsUpdate = true;
unsigned long updatedAt = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Hi.");

  leds.Begin();  
  leds.Show();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi Connected.");
  
  // Update every 2 hours. TODO
  updateTicker.attach(60*60*2, requestUpdate);
}

unsigned long currentTime = millis();
unsigned long lastAnimationTime = currentTime;

void loop() {
  currentTime = millis();

  // Update when neccesary.
  if (needsUpdate) {
    Serial.println("Updating ...");
    updateWeatherData();
    updatedAt = currentTime;
    needsUpdate = false;
  }

  // Animate LEDs at 60hz.
  if (currentTime - lastAnimationTime > 1000/60) {
    for (int i=0; i<NUM_DAYS; i++) {
      animateLed(i, currentTime);
    }
    leds.Show();
    
    lastAnimationTime = currentTime;
  } 
}


void requestUpdate() {
  needsUpdate = true;
}


HTTPClient http;
DynamicJsonBuffer jsonBuffer;

void updateWeatherData() {
  Serial.println("Getting weather data ...");

  Serial.println(API_URL);
  http.begin(API_URL);
  int httpCode = http.GET();
  Serial.println(httpCode);
  if (httpCode != 200) {
    Serial.println("An HTTP Error occured.");
    Serial.println(http.errorToString(httpCode));
  } else {
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


conditions day;
int phase;
int level;

void animateLed(unsigned int led, unsigned long time) {
  day = days[led];
  phase = 128 + 127 * cos(2 * PI / 1500 * (time + (1000 * (double(led) / NUM_DAYS))));

  // icon
  // A machine-readable text summary of this data point, suitable for selecting an icon for display.
  // If defined, this property will have one of the following values:
  // clear-day, clear-night, rain, snow, sleet, wind, fog, cloudy, partly-cloudy-day, or partly-cloudy-night.
  // (Developers should ensure that a sensible default is defined, as additional values, such as hail,
  // thunderstorm, or tornado, may be defined in the future.)

  // Clear
  if (strcmp(day.icon, "clear-day") == 0
   || strcmp(day.icon, "clear-night") == 0) {
    leds.SetPixelColor(led, RgbColor(255, 200, 0));
  }

  // Rain
  else if (strcmp(day.icon, "rain") == 0) {
    leds.SetPixelColor(led, RgbColor(
      random(0, 32),
      random(0, 32),
      random(map(day.precipIntensity, 0.0, 1.0, 255, 0), 255)
    ));
  }

  // Snow
  else if (strcmp(day.icon, "snow") == 0) {
    level = random(200, 255);
    leds.SetPixelColor(led, RgbColor(level, level, level));
  }
  else if (strcmp(day.icon, "sleet") == 0) {
    leds.SetPixelColor(led, RgbColor(
      random(110, 130),
      random(110, 130),
      random(200, 255)
    ));
  }

  // Wind
  else if (strcmp(day.icon, "wind") == 0) {
    level = map(
      128 + 127 * cos(2 * PI / max(300.0, 1500 - 10*day.windSpeed) * (time + (1000 * (double(led) / NUM_DAYS)))),
      0, 255, 100, 200);
    leds.SetPixelColor(led, RgbColor(level, level, level));
  }

  // Fog
  else if (strcmp(day.icon, "fog") == 0) { 
    leds.SetPixelColor(led, RgbColor(32, 32, 32));
  }

  // Cloudy
  else if (strcmp(day.icon, "cloudy") == 0) {
    level = map(phase, 0, 255, 100, 200);
    leds.SetPixelColor(led,  RgbColor(level, level, level));
  }

  // Partly cloudy
  else if (strcmp(day.icon, "partly-cloudy-day") == 0
        || strcmp(day.icon, "partly-cloudy-night") == 0) {
    leds.SetPixelColor(led, RgbColor(
      map(phase, 0, 255, 100, 255),
      map(phase, 0, 255, 100, 200 + day.cloudCover*55),
      map(phase, 0, 255, 100, day.cloudCover*255)
    ));
  }

  // Unknown
  else {
    leds.SetPixelColor(led, RgbColor(255, 0, 0));
  }
}
