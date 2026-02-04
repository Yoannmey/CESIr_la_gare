#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

const char* ssid = "iPhone (9)";
const char* password = "aaaaaaaa";
const char* apiKey = "1a0b777d-c257-45ad-bdbd-dc3daa85bbc5";

StaticJsonDocument<20000> doc;

unsigned long lastHttpFetch = 0;
unsigned long lastMinuteCheck = 0;

const unsigned long HTTP_INTERVAL = 3600000;
const unsigned long MINUTE_INTERVAL = 60000;

String buildFromDatetime() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  char buffer[20];
  sprintf(buffer, "%04d%02d%02dT%02d0000",
          timeinfo.tm_year + 1900,
          timeinfo.tm_mon + 1,
          timeinfo.tm_mday,
          timeinfo.tm_hour);
  return String(buffer);
}

String buildUrl(String dateTime, String request) {
  String uri = "https://api.sncf.com/v1/coverage/sncf/stop_areas/stop_area:SNCF:87474007/";
  if (request == "departure") uri += "departures?";
  else uri += "arrivals?";
  uri += "from_datetime=" + dateTime + "&duration=3600";
  return uri;
}

bool isNow(const char* dateTime, int h, int m) {
  String dt = dateTime;
  int th = dt.substring(9, 11).toInt();
  int tm = dt.substring(11, 13).toInt();
  if (th != h) return false;
  return abs(tm - m) <= 1;
}

void fetchJson() {
  HTTPClient http;
  String url = buildUrl(buildFromDatetime(), "departure");
  http.begin(url);
  http.addHeader("Authorization", apiKey);
  int code = http.GET();
  if (code == 200) {
    deserializeJson(doc, http.getString());
  }
  http.end();
}

void checkTrains() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  int h = timeinfo.tm_hour;
  int m = timeinfo.tm_min;

  for (JsonObject dep : doc["departures"].as<JsonArray>()) {
    const char* t = dep["stop_date_time"]["departure_date_time"];
    const char* dir = dep["display_informations"]["direction"];
    const char* type = dep["display_informations"]["commercial_mode"];
    if (isNow(t, h, m)) {
     
     // mettre le code en cas de départ
    }
  }

  for (JsonObject arr : doc["arrivals"].as<JsonArray>()) {
    const char* t = arr["stop_date_time"]["arrival_date_time"];
    const char* dir = arr["display_informations"]["direction"];
    const char* type = arr["display_informations"]["commercial_mode"];
    if (isNow(t, h, m)) {
      // mettre le code en cas d'arrivée en gare
    }
  }
}

void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  configTzTime("CET-1CEST,M3.5.0/02,M10.5.0/03", "pool.ntp.org", "time.nist.gov");
  fetchJson();
}

void loop() {
  unsigned long now = millis();
  if (now - lastHttpFetch >= HTTP_INTERVAL) {
    lastHttpFetch = now;
    fetchJson();
  }
  if (now - lastMinuteCheck >= MINUTE_INTERVAL) {
    lastMinuteCheck = now;
    checkTrains();
  }
}
