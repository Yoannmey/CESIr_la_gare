#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Stepper.h>
#include "DFRobotDFPlayerMini.h"
// ===== SON =====
#define KLAXON 13
#define JINGLE 12
#define TERPROVENANCE 9
#define TERDESTINATION 8
#define TGVPROVENANCE 11
#define TGVDESTINATION 10
#define PARIS 3
#define MORLAIX 2
#define LANDERNEAU 1
#define RENNES 7
#define QUIMPER 6
#define PARTIR 4
#define RENTRER 5
// ===== STEPPER =====
#define IN1 25
#define IN2 26
#define IN3 27
#define IN4 14

const int stepsPerRevolution = 2048;
const int steps180 = 1024;

Stepper myStepper(stepsPerRevolution, IN1, IN3, IN2, IN4);

// ===== DFPLAYER =====
#define PIN_MP3_TX 17
#define PIN_MP3_RX 16

HardwareSerial mySerial(2);
DFRobotDFPlayerMini player;

int currentTrack = 1;

// ===== LEDs ======
#define LED1 33  // VERT
#define LED2 32  // JAUNE
#define LED3 18  // ROUGE



unsigned long previousMillis = 0;
const int interval = 200;
int ledState = 0;
const char* ssid = "TRISCL";
const char* password = "Rentree2024";
const char* apiKey = "1a0b777d-c257-45ad-bdbd-dc3daa85bbc5";

struct TrainInfo {
  int hour;
  int minute;
  String type;
  String direction;
  bool isDeparture;
};

TrainInfo trains[100];
int trainCount = 0;

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
  Serial.println(String(buffer));
}

String buildUrl(String dateTime, String request) {
  String uri = "https://api.sncf.com/v1/coverage/sncf/stop_areas/stop_area:SNCF:87474007/";
  if (request == "departure") uri += "departures?";
  else uri += "arrivals?";
  uri += "from_datetime=" + dateTime + "&duration=3600";
  return uri;
}

bool isNow(int h, int m, int th, int tm) {
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
    StaticJsonDocument<20000> doc;
    deserializeJson(doc, http.getString());
    trainCount = 0;
    for (JsonObject dep : doc["departures"].as<JsonArray>()) {
      const char* t = dep["stop_date_time"]["departure_date_time"];
      String dir = dep["display_informations"]["direction"].as<const char*>();
      String provenance = convertStopArea(dir.c_str());
      const char* type = dep["display_informations"]["commercial_mode"];
      int th = String(t).substring(9, 11).toInt();
      int tm = String(t).substring(11, 13).toInt();
      trains[trainCount++] = { th, tm, String(type), provenance, true };
    }
  }
  http.end();

  url = buildUrl(buildFromDatetime(), "arrival");
  http.begin(url);
  http.addHeader("Authorization", apiKey);
  code = http.GET();
  if (code == 200) {
    StaticJsonDocument<20000> doc;
    deserializeJson(doc, http.getString());
    for (JsonObject arr : doc["arrivals"].as<JsonArray>()) {
      const char* t = arr["stop_date_time"]["arrival_date_time"];
      const char* type = arr["display_informations"]["commercial_mode"];
      String dir = arr["display_informations"]["links"][0]["id"].as<const char*>();
      String provenance = convertStopArea(dir.c_str());

      int th = String(t).substring(9, 11).toInt();
      int tm = String(t).substring(11, 13).toInt();
      trains[trainCount++] = { th, tm, String(type), provenance, false };
    }
  }
  http.end();
}

void checkTrains() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  int h = timeinfo.tm_hour;
  int m = timeinfo.tm_min;
  for (int i = 0; i < trainCount; i++) {
    // if (isNow(h, m, trains[i].hour, trains[i].minute)) {
      if(true){
      digitalWrite(LED2, HIGH);
      delay(5000);
      player.play(JINGLE);
      delay(2000);
      if (trains[i].isDeparture) {
        
        depart(i);
        delay(5000);
        player.play(KLAXON);
        myStepper.setSpeed(10);  // 10 RPM (essayez 10 à 15)
        myStepper.step(-steps180 * 2);
      } else {
        
        arrive(i);
        delay(5000);
        player.play(KLAXON);
        myStepper.setSpeed(10);  // 10 RPM (essayez 10 à 15)
        myStepper.step(-steps180 * 2);
      }
      digitalWrite(LED1, LOW);
      digitalWrite(LED2, LOW);
      digitalWrite(LED3, LOW);
      trains[i] = {0, 0, " ", " ", false};
    }
  }
}

String convertStopArea(const char* stopArea) {
  if (strcmp(stopArea, "stop_area:SNCF:87474239") == 0) return "Landerneau";
  if (strcmp(stopArea, "stop_area:SNCF:87474098") == 0) return "Quimper";
  if (strcmp(stopArea, "stop_area:SNCF:87391003") == 0) return "Montparnasse";
  if (strcmp(stopArea, "stop_area:SNCF:87474338") == 0) return "Morlaix";
  if (strcmp(stopArea, "stop_area:SNCF:87471003") == 0) return "Rennes";
  if (strcmp(stopArea, "Landerneau (Landerneau)") == 0) return "Landerneau";
  if (strcmp(stopArea, "Quimper (Quimper)") == 0) return "Quimper";
  if (strcmp(stopArea, "Morlaix (Morlaix)") == 0) return "Morlaix";
  if (strcmp(stopArea, "Rennes (Rennes)") == 0) return "Rennes"; 
  if (strcmp(stopArea, "Paris - Montparnasse - Hall 1 & 2 (Paris)") == 0) return "Montparnasse";
  return String(stopArea);
}

void depart(int i) {
  digitalWrite(LED2, LOW);
  digitalWrite(LED1, HIGH);
  if (trains[i].type == "TGV INOUI") {
    player.play(TGVDESTINATION);
    delay(6000);
    check_city(i);
  } else {
    player.play(TERDESTINATION);
    delay(7200);
    check_city(i);
  }
  delay(2000);
  player.play(PARTIR);
}

void arrive(int i) {
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, HIGH);
  if (trains[i].type == "TGV INOUI") {
    player.play(TGVPROVENANCE);
    delay(6000);
    check_city(i);
  } else {
    player.play(TERPROVENANCE);
    delay(7200);
    check_city(i);
  }
  delay(2000);
  player.play(RENTRER);
}


void check_city(int i) {
  if (trains[i].direction == "Landerneau") {
    player.play(LANDERNEAU);

  } else if (trains[i].direction == "Quimper") {
    player.play(QUIMPER);

  } else if (trains[i].direction == "Montparnasse") {
    player.play(PARIS);

  } else if (trains[i].direction == "Morlaix") {
    player.play(MORLAIX);

  } else if (trains[i].direction == "Rennes") {
    player.play(RENNES);
  }
}
void testMateriel() {

  Serial.println("===== TEST MATERIEL =====");

  // ===== TEST LEDS =====
  Serial.println("Test LEDs");

  digitalWrite(LED1, HIGH);
  delay(500);
  digitalWrite(LED1, LOW);

  digitalWrite(LED2, HIGH);
  delay(500);
  digitalWrite(LED2, LOW);

  digitalWrite(LED3, HIGH);
  delay(500);
  digitalWrite(LED3, LOW);

  // Allumage simultané
  digitalWrite(LED1, HIGH);
  digitalWrite(LED2, HIGH);
  digitalWrite(LED3, HIGH);
  delay(1000);

  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);

  // ===== TEST SON =====
  Serial.println("Test DFPlayer");
  player.play(JINGLE);  // joue une piste courte
  delay(4000);     // attendre fin du son

  // ===== TEST MOTEUR =====
  Serial.println("Test moteur - 1 tour complet");
  myStepper.setSpeed(10);
  myStepper.step(stepsPerRevolution);  // 1 tour complet
  delay(1000);
  myStepper.step(-stepsPerRevolution);  // retour position initiale

  Serial.println("===== FIN TEST =====");
}

void displayTrains(){
  for (int i = 0; i < trainCount; i++) {
    Serial.print(trains[i].isDeparture);
    Serial.print(" ");
    Serial.print(trains[i].direction);
    Serial.print(" ");
    Serial.println(trains[i].type);
  }
}
void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println(WiFi.status());
  configTzTime("CET-1CEST,M3.5.0/02,M10.5.0/03", "pool.ntp.org", "time.nist.gov");


  // LEDs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);

  // DFPlayer
  mySerial.begin(9600, SERIAL_8N1, PIN_MP3_RX, PIN_MP3_TX);
  if (!player.begin(mySerial)) {
    Serial.println("Erreur DFPlayer !");
  } else {
    player.volume(25);
  }

  //  testMateriel();
  //  fetchJson();
  //  displayTrains();

  int th = 17;
  int tm = 01;
  trains[0] = { th, tm, "TGV INOUI", "Morlaix", true };
  trainCount++;
  displayTrains();
  checkTrains();
  trains[0] = { th, tm, "TER", "Montparnasse", false };
  trainCount++;
  checkTrains();
}

void loop() {
  unsigned long now = millis();
  if (now - lastHttpFetch >= HTTP_INTERVAL || trains[0].direction == "direction") {
    lastHttpFetch = now;
    fetchJson();
    displayTrains();
  }
  if (now - lastMinuteCheck >= MINUTE_INTERVAL) {
    lastMinuteCheck = now;
    checkTrains();
  }
}