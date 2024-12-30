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
#include <NeoPixelBusLg.h>
#include <ArduinoJson.h>
#include <Ticker.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

// Settings ------------------------------------------------

// WiFi
#define SSID "--YOUR SSID--"
#define PASSWORD "--PASSWORD--"

// Put your location here.
#define LAT "52.52"
#define LON "13.40"

// Pin where the LED stripe data port is connected.
// Note: on ESP8266, only GPIO 3 will work (NeoPixelBus limitation, see https://github.com/Makuna/NeoPixelBus/wiki/ESP8266-NeoMethods for workarounds).
#define LED_PIN 3

// Normal or reversed mode.
#define REVERSED false

// Brightness, 0 to 100.
#define BRIGHTNESS 90

// ---------------------------------------------------------

#define NUM_DAYS 8

#define API_URL "http://api.open-meteo.com/v1/forecast?latitude=" LAT "&longitude=" LON \
  "&daily=weather_code,daylight_duration,sunshine_duration,precipitation_sum,wind_speed_10m_max" \
  "&forecast_days=" STR(NUM_DAYS) \
  "&timezone=auto"

#define ARDUINOJSON_DECODE_UNICODE 1

#define NTP_SERVER_1 "0.pool.ntp.org"
#define NTP_SERVER_2 "1.pool.ntp.org"
#define NTP_SERVER_3 "2.pool.ntp.org"

#define min(a, b) ((a)<(b)?(a):(b))
#define max(a, b) ((a)>(b)?(a):(b))

NeoPixelBusLg<NeoGrbFeature, Neo800KbpsMethod> leds(NUM_DAYS, LED_PIN);
WiFiClient client;
HTTPClient http;
Ticker updateTicker;

struct conditions {
  unsigned int weather_code{}; // WMO weather code
  double sunshine_ratio{}; // 0..1
  double wind{}; // max wind speed (km/h)
  double precipitation{}; // precipitation sum (mm)
};

struct conditions days[NUM_DAYS];
boolean needsUpdate = true;
unsigned long currentTime = millis();
unsigned long lastAnimationTime = currentTime;

conditions d;
int phase;
int level;


void updateWeatherData() {
  Serial.println("Getting weather data ...");
  Serial.println(API_URL);

  http.useHTTP10(true);
  http.begin(client, API_URL);
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
  filter["daily"]["time"][0] = true;
  filter["daily"]["weather_code"][0] = true;
  filter["daily"]["daylight_duration"][0] = true;
  filter["daily"]["sunshine_duration"][0] = true;
  filter["daily"]["precipitation_sum"][0] = true;
  filter["daily"]["wind_speed_10m_max"][0] = true;

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

    day.weather_code   = jsonDoc["daily"]["weather_code"][i].as<uint>();
    day.sunshine_ratio = jsonDoc["daily"]["sunshine_duration"][i].as<double>() / jsonDoc["daily"]["daylight_duration"][i].as<double>();
    day.precipitation  = jsonDoc["daily"]["precipitation_sum"][i].as<double>();
    day.wind           = jsonDoc["daily"]["wind_speed_10m_max"][i].as<double>();

    days[i] = day;

    Serial.println(
      String(i) + " " + jsonDoc["daily"]["time"][i].as<std::string>().c_str()
      + ": weather_code: " + day.weather_code + ", sunshine_ratio: " + day.sunshine_ratio + ", wind: " + day.wind + ", precipitation: " + day.precipitation
    );
  }

  Serial.println("Weather data updated.");
  client.stop();
}


void requestUpdate() {
  needsUpdate = true;
}


void animateLed(unsigned int led, unsigned long time) {
  d = REVERSED ? days[NUM_DAYS - led - 1] : days[led];
  phase = 128 + 127 * cos(2 * PI / 1500 * (time + (1000 * (double(led) / NUM_DAYS))));

  switch (d.weather_code) {

    // Clear
    case 0:
      leds.SetPixelColor(led, RgbColor(255, 200, 0));
      break;

    // Clouds
    case 1:
    case 2:
    case 3:
    leds.SetPixelColor(led, RgbColor(
      map(phase, 0, 255, 100, 255),
      map(phase, 0, 255, 100, 200 + (1.0 - d.sunshine_ratio) * 55),
      map(phase, 0, 255, 100, (1.0 - d.sunshine_ratio) * 255)
    ));
    break;

    // Fog
    case 45:
    case 48:
      leds.SetPixelColor(led, RgbColor(100, 100, 100));
    break;

    // Drizzle
    case 51:
    case 53:
    case 55:
      leds.SetPixelColor(led, RgbColor(
        random(110, 130),
        random(110, 130),
        random(200, 255)
      ));
    break;

    // Rain
    case 61:
    case 63:
    case 65:
    case 80:
    case 81:
    case 82:
      leds.SetPixelColor(led, RgbColor(
        random(0, 32),
        random(0, 32),
        random(map(d.precipitation, 0.0, 20.0, 255, 0), 255)
      ));
      break;

    // Snow
    case 56:
    case 57:
    case 66:
    case 67:
    case 71:
    case 73:
    case 75:
    case 77:
    case 85:
    case 86:
      level = random(map(d.precipitation, -3.0, 20.0, 255, 100), 255);
      leds.SetPixelColor(led, RgbColor(level, level, level));
      break;

    // Thunderstorm
    case 95:
    case 96:
    case 99:
      level = map(
        128 + 127 * cos(2 * PI / max(300.0, 1500 - 10 * map(d.wind, 0.0, 75.0, 0.0, 1.0)) * (time + (1000 * (double(led) / NUM_DAYS)))),
        0, 255, 100, 200
      );
      leds.SetPixelColor(led, RgbColor(level, floor(level * 0.5), floor(level * 0.5)));
      break;

    // Unknown
    default:
      leds.SetPixelColor(led, RgbColor(
        map(phase, 0, 255, 100, 150), 0, 0
      ));
  }
}


void setup() {
  Serial.begin(9600);
  Serial.println("Hi.");

  leds.Begin();
  leds.SetLuminance(map(0, 100, 0, 255, BRIGHTNESS));
  leds.ClearTo(RgbColor(0, 0, 0));
  leds.Show();

  Serial.print("Connecting WiFi ...");
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

  // Update every hour.
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
