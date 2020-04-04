#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
ESP8266WiFiMulti wifiMulti;
#else
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <HTTPClient.h>
WiFiMulti wifiMulti;
#endif
#include <Arduino.h>
#include <NeoPixelBus.h>
#include <ArduinoJson.h>
#include <Ticker.h>

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))


// Settings ------------------------------------------------
// WiFi
#define SSID "--YOUR SSID--"
#define PASSWORD "--PASSWORD--"

// Pin where the LED stripe data port is connected.
#define LED_PIN D2

// Put your location here (lat,lon).
#define LAT_LON "52.52,13.40" // Berlin, Germany

// Register for the darsky API at https://darksky.net/dev
#define DARKSKY_APIKEY "--YOUR API KEY--"

// Normal or reversed mode.
#define REVERSED false
// ---------------------------------------------------------


#define API_URL "https://api.darksky.net/forecast/" DARKSKY_APIKEY "/" LAT_LON "/?exclude=currently,hourly,flags,alerts&units=ca"
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

unsigned long currentTime = millis();
unsigned long lastAnimationTime = currentTime;

conditions day;
int phase;
int level;


void setup() {
  Serial.begin(115200);
  Serial.println("Hi.");

  leds.Begin();  
  leds.Show();

  Serial.print("Connecting WiFi ..");
  WiFi.mode(WIFI_STA);
  
  wifiMulti.addAP(SSID, PASSWORD);
  // You can add multiple APs here:
  // wifiMulti.addAP(SSID2, PASSWORD2);
  
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi Connected.");
  
  // Update every 2 hours.
  updateTicker.attach(60*60*2, requestUpdate);
}


void loop() {
  currentTime = millis();

  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("WiFi not connected.");
    delay(500);
  }

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


void updateWeatherData() {
  Serial.println("Getting weather data ...");
  Serial.println(API_URL);
  
  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  
  https.begin(client, API_URL);
  int httpCode = https.GET();
  
  if (httpCode != 200) {
    Serial.println("An HTTP Error occured.");
    Serial.println(httpCode);
    Serial.println(https.errorToString(httpCode));
    client.stop();
    https.end();
    return;
  }

  DynamicJsonDocument jsonDoc(16384);
  DeserializationError error = deserializeJson(jsonDoc, https.getStream());
  
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }
  
  for (unsigned int i=0; i<NUM_DAYS; i++) {
    conditions day;

    String icon = jsonDoc["daily"]["data"][i]["icon"].as<String>();
    icon.toCharArray(day.icon, 32);
    
    day.cloudCover      = jsonDoc["daily"]["data"][i]["cloudCover"].as<double>();
    day.windSpeed       = jsonDoc["daily"]["data"][i]["windSpeed"].as<double>();
    day.precipIntensity = jsonDoc["daily"]["data"][i]["precipIntensity"].as<double>();
    days[i] = day;
    
    Serial.println(String(i) + ": " + day.icon + "|cc:" + day.cloudCover + "|ws:" + day.windSpeed + "|pi:" + day.precipIntensity);
  }

  Serial.println("Weather data updated.");
  client.stop();
  https.end();
}


void animateLed(unsigned int led, unsigned long time) {
  day = REVERSED ? days[NUM_DAYS - led - 1] : days[led];
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
