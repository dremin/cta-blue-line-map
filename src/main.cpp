#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_PWMServoDriver.h>
#include <ArduinoJson.h>
#include "time.h"
#include "secrets.h"

const bool debug = true;

// API parameters
const int timeout = 10000;
const int interval = 10000;

// LED parameters
const int brightness = 100; // max 100
const int maxLeds = 2;

// Time parameters
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -21600; // Chicago
const int daylightOffset_sec = 3600;

// Primary PWM driver
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// TODO: Daisy chained PWM drivers
Adafruit_PWMServoDriver pwm2 = Adafruit_PWMServoDriver(0x41);
Adafruit_PWMServoDriver pwm3 = Adafruit_PWMServoDriver(0x42);
Adafruit_PWMServoDriver pwm4 = Adafruit_PWMServoDriver(0x43);
Adafruit_PWMServoDriver pwm5 = Adafruit_PWMServoDriver(0x44);
Adafruit_PWMServoDriver pwm6 = Adafruit_PWMServoDriver(0x45);
Adafruit_PWMServoDriver pwm7 = Adafruit_PWMServoDriver(0x46);

const char* stations[] = {
  "40890", // O'Hare
  "40820",
  "40230",
  "40750",
  "41280",
  "41330",
  "40550",
  "41240",
  "40060",
  "41020",
  "40570",
  "40670",
  "40590",
  "40320",
  "41410",
  "40490",
  "40380",
  "40370",
  "40790",
  "40070",
  "41340",
  "40430",
  "40350",
  "40470",
  "40810",
  "40220",
  "40250",
  "40920",
  "40970",
  "40010",
  "40180",
  "40980",
  "40390" // Forest Park
};
// Names are only used for debug output
const char* stationNames[] = {
  "O'Hare",
  "Rosemont",
  "Cumberland",
  "Harlem (O'Hare Branch)",
  "Jefferson Park",
  "Montrose",
  "Irving Park",
  "Addison",
  "Belmont",
  "Logan Square",
  "California",
  "Western (O'Hare Branch)",
  "Damen",
  "Division",
  "Chicago",
  "Grand",
  "Clark/Lake",
  "Washington",
  "Monroe",
  "Jackson",
  "LaSalle",
  "Clinton",
  "UIC-Halsted",
  "Racine",
  "Illinois Medical District",
  "Western (Forest Park Branch)",
  "Kedzie-Homan",
  "Pulaski",
  "Cicero",
  "Austin",
  "Oak Park",
  "Harlem (Forest Park Branch)",
  "Forest Park",
};
const size_t numStations = sizeof(stations) / sizeof(stations[0]);
enum Classification {
  NoTrain = 0,
  OHareBound = 1,
  FPBound = 2,
  Series5000 = 3,
  Series7000 = 4,
  BothDirections = 5,
  JPBound = 6,
  UICBound = 7,
  HolidayTrain = 8
};
Classification trainState[numStations] = {};


// prototype functions so we can reference them anywhere
void connectWiFi();
void turnOffRgb(uint8_t ledNum);
void setRgb(uint8_t ledNum, uint8_t red, uint8_t green, uint8_t blue);
void getPositions();
void webRequest(String url);
void parseJson(HTTPClient& client);
void parseTrain(JsonObject train);
int getStationIndex(const char* station);
Classification getTrainClassification(const char* run, const char* destStation, const char* destName, const char* direction);
void displayTrains();
uint8_t getLedForDriver(uint8_t ledNum);
Adafruit_PWMServoDriver getDriverForLed(uint8_t ledNum);



void setup() {
  Serial.begin(115200);
  delay(10);


  Serial.println();
  Serial.println();
  

  connectWiFi();
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);


  pwm.begin();
  pwm.setPWMFreq(1600);  // This is the maximum PWM frequency

  // TODO: Init the other PWMs
  /*pwm2.begin();
  pwm2.setPWMFreq(1600);
  pwm3.begin();
  pwm3.setPWMFreq(1600);
  pwm4.begin();
  pwm4.setPWMFreq(1600);
  pwm5.begin();
  pwm5.setPWMFreq(1600);
  pwm6.begin();
  pwm6.setPWMFreq(1600);
  pwm7.begin();
  pwm7.setPWMFreq(1600);*/
}

void loop() {
  if (debug) {
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    // disconnected; attempt reconnect
    connectWiFi();
  }

  getPositions();
  displayTrains();

  delay(interval);
}

// Network functions

void connectWiFi() {
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    if (millis() - startTime > timeout) {
      Serial.println("Timed out, trying again...");

      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

      startTime = millis();
    }
  }

  Serial.println("");
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
}

void webRequest(String url) {
  WiFiClient client;
  HTTPClient http;

  if (debug) {
    Serial.print("Requesting URL: ");
    Serial.println(url);
  }
  
  // This will send the request to the server
  http.useHTTP10(true);
  http.begin(client, url);
  int httpCode = http.GET();
  
  if (httpCode >= 200 && httpCode < 300) {
    // request was successful
    // reset state
    for (int i = 0; i < numStations; i++) {
      trainState[i] = NoTrain;
    }
    // parse response
    parseJson(http);
  } else {
    Serial.println("Request failed");
  }
  
  http.end();
}

// LED helper functions

void turnOffRgb(uint8_t ledNum) {
  if (ledNum >= maxLeds) {
    return;
  }

  uint8_t actualLedNum = getLedForDriver(ledNum);
  Adafruit_PWMServoDriver driver = getDriverForLed(ledNum);

  driver.setPWM(actualLedNum, 0, 0);
  driver.setPWM(actualLedNum + 1, 0, 0);
  driver.setPWM(actualLedNum + 2, 0, 0);
}

void setRgb(uint8_t ledNum, uint8_t red, uint8_t green, uint8_t blue) {
  if (ledNum >= maxLeds) {
    return;
  }

  uint8_t actualLedNum = getLedForDriver(ledNum);
  Adafruit_PWMServoDriver driver = getDriverForLed(ledNum);

  uint16_t redPwm = 4095 * ((double)red / 255) * ((double)brightness / 100);
  uint16_t greenPwm = 4095 * ((double)green / 255) * ((double)brightness / 100);
  uint16_t bluePwm = 4095 * ((double)blue / 255) * ((double)brightness / 100);

  // For each LED, the first leg is red, second is green, third is blue
  driver.setPWM(actualLedNum, 0, redPwm);
  driver.setPWM(actualLedNum + 1, 0, greenPwm);
  driver.setPWM(actualLedNum + 2, 0, bluePwm);
}

void displayTrains() {
  for (int i = 0; i < numStations; i++) {
    if (debug) {
      Serial.print("Station ");
      Serial.print(stations[i]);
      Serial.print(" state: ");
    }

    // Placeholder variable to allow testing on the wrong LED
    int useLed = i;

    switch (trainState[i]) {
      case NoTrain:
        if (debug) Serial.print("No train               ");
        turnOffRgb(useLed);
        break;
      case Series5000:
        if (debug) Serial.print("5000-series train      ");
        setRgb(useLed, 0, 80, 0);
        break;
      case Series7000:
        if (debug) Serial.print("7000-series train      ");
        setRgb(useLed, 0, 80, 0);
        break;
      case OHareBound:
        if (debug) Serial.print("O'Hare-bound train     ");
        setRgb(useLed, 0, 0, 255);
        break;
      case FPBound:
        if (debug) Serial.print("Forest Park-bound train");
        setRgb(useLed, 255, 0, 0);
        break;
      case JPBound:
        if (debug) Serial.print("Jefferson Park-bound   ");
        setRgb(useLed, 0, 80, 255);
        break;
      case UICBound:
        if (debug) Serial.print("UIC-Halsted-bound train");
        setRgb(useLed, 255, 20, 0);
        break;
      case BothDirections:
        if (debug) Serial.print("Trains each direction  ");
        setRgb(useLed, 255, 0, 255);
        break;
      case HolidayTrain:
        if (debug) Serial.print("Holiday train          ");
        setRgb(useLed, 255, 80, 255);
        break;
    }

    if (debug) {
      Serial.print("   (");
      Serial.print(stationNames[i]);
      Serial.println(")");
    }
  }
}

uint8_t getLedForDriver(uint8_t ledNum) {
  // ledNum is provided with the assumption each LED is one, not three, so calculate the actual value
  // each driver can handle 5 RGB LEDs

  if (ledNum < 5) {
    return ledNum * 3;
  }

  if (ledNum < 10) {
    return (ledNum - 5) * 3;
  }

  if (ledNum < 15) {
    return (ledNum - 10) * 3;
  }

  if (ledNum < 20) {
    return (ledNum - 15) * 3;
  }

  if (ledNum < 25) {
    return (ledNum - 20) * 3;
  }

  if (ledNum < 30) {
    return (ledNum - 25) * 3;
  }

  return (ledNum - 30) * 3;
}

Adafruit_PWMServoDriver getDriverForLed(uint8_t ledNum) {
  if (ledNum < 5) {
    return pwm;
  }

  if (ledNum < 10) {
    return pwm2;
  }

  if (ledNum < 15) {
    return pwm3;
  }

  if (ledNum < 20) {
    return pwm4;
  }

  if (ledNum < 25) {
    return pwm5;
  }

  if (ledNum < 30) {
    return pwm6;
  }

  return pwm7;
}

// Parsing functions

void parseJson(HTTPClient& client) {
  StaticJsonDocument<192> filter;

  JsonObject filter_train = filter["ctatt"]["route"][0]["train"].createNestedObject();
  filter_train["rn"] = true;
  filter_train["destSt"] = true;
  filter_train["destNm"] = true;
  filter_train["trDr"] = true;
  filter_train["nextStaId"] = true;
  filter_train["nextStaNm"] = true;
  filter_train["isApp"] = true;

  DynamicJsonDocument doc(6144);
  DeserializationError error = deserializeJson(doc, client.getStream(), DeserializationOption::Filter(filter));

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());

    if (strcmp(error.c_str(), "IncompleteInput") == 0) {
      // retry
      delay(interval);
      getPositions();
    }

    return;
  }

  for (JsonObject train : doc["ctatt"]["route"][0]["train"].as<JsonArray>()) {
    parseTrain(train);
  }
}

void parseTrain(JsonObject train) {
  const char* run = train["rn"]; // "134", "136", "137", "223", ...
  const char* destStation = train["destSt"]; // "30171", "30171", ...
  const char* destName = train["destNm"]; // "O'Hare", "O'Hare", ...
  const char* direction = train["trDr"]; // "1", "1", "1", "5", "5"
  const char* nextStation = train["nextStaId"]; // "40750", ...
  const char* nextStationName = train["nextStaNm"]; // "Harlem ...
  const char* isApp = train["isApp"];

  if (debug) {
    Serial.print("Found run ");
    Serial.print(run);
    Serial.print(" to ");
    Serial.print(destName);
    if (strcmp(isApp, "1") == 0) {
      Serial.print(". Approaching: ");
    } else {
      Serial.print(". Next stop: ");
    }
    Serial.println(nextStationName);
  }

  int ledIndex = getStationIndex(nextStation);

  if (ledIndex < 0) {
    // station wasn't found
    Serial.print("Unable to find station ");
    Serial.println(nextStation);
    return;
  }

  Classification trainClass = getTrainClassification(run, destStation, destName, direction);

  if (strcmp(isApp, "0") == 0) {
    // not approaching; use previous station
    // previous station depends on train direction
    if (strcmp(direction, "5") == 0) {
      // Southbound, remove 1 from index
      if (ledIndex > 0) {
        ledIndex -= 1;
      }
    } else {
      // Northbound, add 1
      if (ledIndex < numStations) {
        ledIndex += 1;
      }
    }
  }

  switch (trainState[ledIndex]) {
    case NoTrain:
      trainState[ledIndex] = trainClass;
      break;
    case OHareBound:
    case JPBound:
      if (trainClass == FPBound || trainClass == UICBound) {
        trainState[ledIndex] = BothDirections;
      }
      break;
    case FPBound:
    case UICBound:
      if (trainClass == OHareBound || trainClass == JPBound) {
        trainState[ledIndex] = BothDirections;
      }
      break;
  }
  
  // Non-directional classifications take precedence
  if (trainClass == Series5000 || trainClass == Series7000 || trainClass == HolidayTrain) {
    trainState[ledIndex] = trainClass;
  }
}

// Train helper functions

void getPositions() {
  String url = "http://lapi.transitchicago.com/api/1.0/ttpositions.aspx?key=";
  url += CTA_API_KEY;
  url += "&rt=blue&outputType=JSON";

  webRequest(url);
}

int getStationIndex(const char* station) {
  for (int i = 0; i < numStations; i++) {
    if (strcmp(stations[i], station) == 0) {
      return i;
    }
  }

  return -1;
}

Classification getTrainClassification(const char* run, const char* destStation, const char* destName, const char* direction) {
  // 5000-series from 54/Cermak
  if (strlen(run) > 0 && run[0] == '3') {
    return Series5000;
  }

  // Holiday Train
  if (strcmp(run, "1225") == 0) {
    return HolidayTrain;
  }

  // O'Hare
  if (strcmp(destStation, "30171") == 0 || strcmp(destName, "O'Hare") == 0) {
    return OHareBound;
  }

  // Forest Park
  if (strcmp(destStation, "30077") == 0 || strcmp(destName, "Forest Park") == 0) {
    return FPBound;
  }

  // Jefferson Park, Rosemont
  if (strcmp(destStation, "30247") == 0 || strcmp(destStation, "30159") == 0 || strcmp(destName, "Jefferson Park") == 0 || strcmp(destName, "Rosemont") == 0) {
    return JPBound;
  }

  // UIC-Halsted, Racine
  if (strcmp(destStation, "30069") == 0 || strcmp(destStation, "30093") == 0 || strcmp(destName, "UIC-Halsted") == 0 || strcmp(destName, "Racine") == 0) {
    return UICBound;
  }

  Serial.print("Defaulting on destination ");
  Serial.println(destStation);
  
  if (strcmp(direction, "5") == 0) {
    return FPBound;
  } else {
    return OHareBound;
  }
}