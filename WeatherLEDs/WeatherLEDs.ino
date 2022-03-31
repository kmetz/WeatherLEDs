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


// Settings ------------------------------------------------

// WiFi
#define SSID "--YOUR SSID--"
#define PASSWORD "--PASSWORD--"

// Register for the OpenWeatherMap API at https://openweathermap.org/api
#define OWM_API_KEY "--YOUR API KEY--"

// Put your location here (lat,lon).
#define LAT "52.52"
#define LON "13.40"

// Pin where the LED stripe data port is connected.
// Note: on ESP8266, only GPIO 3 will work (NeoPixelBus limitation, see https://github.com/Makuna/NeoPixelBus/wiki/ESP8266-NeoMethods for workarounds).
#define LED_PIN 3

// Normal or reversed mode.
#define REVERSED false

// ---------------------------------------------------------


#define NUM_DAYS 8

#define OWM_API_URL "http://api.openweathermap.org/data/2.5/onecall?lat=" LAT "&lon=" LON "&exclude=current,minutely,hourly&appid=" OWM_API_KEY
#define ARDUINOJSON_DECODE_UNICODE 1

#define min(a, b) ((a)<(b)?(a):(b))
#define max(a, b) ((a)>(b)?(a):(b))


WiFiClient wifiClient;
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> leds(NUM_DAYS, LED_PIN);
Ticker updateTicker;

struct conditions {
  std::string weather;
  double clouds{};
  double windSpeed{};
  double rain{};
  double snow{};
};

struct conditions days[NUM_DAYS];
boolean needsUpdate = true;
unsigned long currentTime = millis();
unsigned long lastAnimationTime = currentTime;


void updateWeatherData() {
  Serial.println("Getting weather data ...");
  Serial.println(OWM_API_URL);

  WiFiClient client;
  HTTPClient http;

  http.begin(client, OWM_API_URL);
  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.println("An HTTP Error occured.");
    Serial.println(httpCode);
    Serial.println(HTTPClient::errorToString(httpCode));
    client.stop();
    http.end();
    return;
  }

  // Define filter.
  StaticJsonDocument<512> filter;
  filter["daily"][0]["weather"][0]["main"] = true;
  filter["daily"][0]["clouds"] = true;
  filter["daily"][0]["wind_speed"] = true;
  filter["daily"][0]["rain"] = true;
  filter["daily"][0]["snow"] = true;

  // Parse JSON response.
  DynamicJsonDocument jsonDoc(16384);
  DeserializationError dsError = deserializeJson(jsonDoc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (dsError) {
    Serial.printf("deserializeJson() failed: %s\r\n", dsError.c_str());
    return;
  }

  for (unsigned int i = 0; i < NUM_DAYS; i++) {
    conditions day;

    day.weather    = jsonDoc["daily"][i]["weather"][0]["main"].as<std::string>();
    day.clouds     = jsonDoc["daily"][i]["clouds"].as<double>();
    day.windSpeed  = jsonDoc["daily"][i]["wind_speed"].as<double>();
    day.rain       = jsonDoc["daily"][i]["rain"].as<double>();
    day.snow       = jsonDoc["daily"][i]["snow"].as<double>();

    days[i] = day;

    Serial.println(String(i) + ": " + day.weather.c_str() + ", clouds:" + day.clouds + ", wind:" + day.windSpeed + ", rain:" + day.rain + ", snow:" + day.snow);
  }

  Serial.println("Weather data updated.");
  client.stop();
}


void requestUpdate() {
  needsUpdate = true;
}


conditions d;
int phase;
int level;

void animateLed(unsigned int led, unsigned long time) {
  d = REVERSED ? days[NUM_DAYS - led - 1] : days[led];
  phase = 128 + 127 * cos(2 * PI / 1500 * (time + (1000 * (double(led) / NUM_DAYS))));

  if (d.weather == "Clear") {
    leds.SetPixelColor(led, RgbColor(255, 200, 0));
  }

  else if (d.weather == "Rain") {
    leds.SetPixelColor(led, RgbColor(
      random(0, 32),
      random(0, 32),
      random(map(d.rain, 0.0, 20.0, 255, 0), 255)
    ));
  }

  else if (d.weather == "Drizzle") {
    leds.SetPixelColor(led, RgbColor(
      random(110, 130),
      random(110, 130),
      random(200, 255)
    ));
  }

  else if (d.weather == "Snow") {
    level = random(map(d.snow, 0.0, 20.0, 255, 0), 255);
    leds.SetPixelColor(led, RgbColor(level, level, level));
  }

  else if (d.weather == "Thunderstorm") {
    level = map(
      128 + 127 * cos(2 * PI / max(300.0, 1500 - 10 * map(d.windSpeed, 0.0, 100.0, 0.0, 1.0)) * (time + (1000 * (double(led) / NUM_DAYS)))),
      0, 255, 100, 200
    );
    leds.SetPixelColor(led, RgbColor(level, floor(level * 0.5), floor(level * 0.5)));
  }

  else if (d.weather == "Fog" || d.weather == "Mist") {
    leds.SetPixelColor(led, RgbColor(32, 32, 32));
  }

  else if (d.weather == "Clouds") {
    leds.SetPixelColor(led, RgbColor(
      map(phase, 0, 255, 100, 255),
      map(phase, 0, 255, 100, 200 + (d.clouds / 100.0) * 55),
      map(phase, 0, 255, 100, (d.clouds / 100.0) * 255)
    ));
  }

  // Unknown
  else {
    leds.SetPixelColor(led, RgbColor(
      map(phase, 0, 255, 100, 255), 0, 0
    ));
  }
}


void setup() {
  Serial.begin(115200);
  Serial.println("Hi.");

  leds.Begin();
  leds.ClearTo(RgbColor(0, 0, 0));
  leds.Show();

  Serial.print("Connecting WiFi ..");
  WiFi.mode(WIFI_STA);

  wifiMulti.addAP(SSID, PASSWORD);
  // You can add multiple APs here:
  // wifiMulti.addAP(SSID2, PASSWORD2);

  leds.SetPixelColor(0, RgbColor(100, 50, 0));
  leds.Show();

  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    // Flash orange while connecting.
    leds.SetPixelColor(0, RgbColor(0, 0, 0));
    leds.Show();
    delay(100);
    leds.SetPixelColor(0, RgbColor(100, 50, 0));
    leds.Show();
  }
  Serial.println("WiFi Connected.");

  leds.ClearTo(RgbColor(0, 0, 0));
  leds.Show();

  // Update every 2 hours.
  updateTicker.attach(60 * 60 * 2, requestUpdate);
}


void loop() {
  currentTime = millis();

  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("WiFi not connected.");
    // Flash red when connection was lost.
    leds.ClearTo(RgbColor(0, 0, 0));
    leds.SetPixelColor(0, RgbColor(100, 0, 0));
    leds.Show();
    delay(100);
    leds.ClearTo(RgbColor(0, 0, 0));
    leds.Show();
    delay(900);
    return;
  }

  // Update when neccesary.
  if (needsUpdate) {
    Serial.println("Updating ...");
    updateWeatherData();
    needsUpdate = false;
  }

  // Animate LEDs at 60hz.
  if (currentTime - lastAnimationTime > 1000 / 60) {
    for (int i = 0; i < NUM_DAYS; i++) {
      animateLed(i, currentTime);
    }
    leds.Show();
    lastAnimationTime = currentTime;
  }
}
