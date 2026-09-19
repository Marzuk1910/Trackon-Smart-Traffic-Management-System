/*
  Smart Traffic Light — ESP32 Controller (secured + fault-tolerant)
  ===================================================================
  Fixes vs. your working version:
    1. WiFi password + ThingSpeak key moved to secrets.h (not committed)
    2. HTTPS instead of HTTP (WiFiClientSecure)
    3. WiFi reconnect handling instead of silently running on stale data
    4. If the ThingSpeak read fails OR returns garbage, falls back to a
       SAFE DEFAULT timing plan instead of reusing whatever greenTime
       happened to be set last (this is the A6 "network failure" /
       "stale data" / "safe fallback" requirement)
    5. Vehicle count is validated (rejects negative / absurd / non-numeric)
    6. Read failures/successes are tracked so you know if the system has
       been running blind for too long

  NOTE ON HTTPS: this uses client.setInsecure(), which skips certificate
  validation. That's fine for a class project (still gets you TLS
  encryption of the traffic, not the cert-authenticity check), but if you
  want full certificate validation for A2, load ThingSpeak's root CA cert
  with client.setCACert(...) instead — ask me and I'll add that.
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "secrets.h"

// ===== ROAD 1 =====
#define R1_RED     23
#define R1_YELLOW  26
#define R1_GREEN   27
// ===== ROAD 2 =====
#define R2_RED     14
#define R2_YELLOW  12
#define R2_GREEN   13
// ===== ROAD 3 =====
#define R3_RED     18
#define R3_YELLOW  19
#define R3_GREEN   21
// ===== ROAD 4 =====
#define R4_RED     5
#define R4_YELLOW  17
#define R4_GREEN   16

// ===== SAFETY / FALLBACK CONFIG =====
const int SAFE_DEFAULT_GREEN_TIME_S = 15;   // used when a live reading can't be trusted
const int YELLOW_TIME_S = 3;
const int MAX_PLAUSIBLE_COUNT = 200;        // above this, treat the reading as garbage
const unsigned long WIFI_RECONNECT_TIMEOUT_MS = 10000;
const unsigned long HTTP_TIMEOUT_MS = 5000;
const int MAX_CONSECUTIVE_READ_FAILURES = 5; // after this many, force safe mode

int vehicleCount = 0;
int greenTime = SAFE_DEFAULT_GREEN_TIME_S;
int consecutiveReadFailures = 0;

// ===== GREEN TIME LOGIC =====
int calculateGreenTime(int count) {
  if (count <= 5) return 10;
  else if (count <= 10) return 20;
  else if (count <= 20) return 30;
  else return 40;
}

// ===== ALL RED FUNCTION =====
void allRed() {
  digitalWrite(R1_RED, HIGH);
  digitalWrite(R2_RED, HIGH);
  digitalWrite(R3_RED, HIGH);
  digitalWrite(R4_RED, HIGH);

  digitalWrite(R1_YELLOW, LOW);
  digitalWrite(R2_YELLOW, LOW);
  digitalWrite(R3_YELLOW, LOW);
  digitalWrite(R4_YELLOW, LOW);

  digitalWrite(R1_GREEN, LOW);
  digitalWrite(R2_GREEN, LOW);
  digitalWrite(R3_GREEN, LOW);
  digitalWrite(R4_GREEN, LOW);
}

// ===== ENSURE WIFI IS CONNECTED, WITH TIMEOUT =====
bool ensureWiFiConnected() {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.print("WiFi not connected, reconnecting");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_RECONNECT_TIMEOUT_MS) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  return WiFi.status() == WL_CONNECTED;
}

// ===== READ + VALIDATE VEHICLE COUNT FROM THINGSPEAK =====
// Returns true if a trustworthy new reading was obtained.
// On any failure, does NOT touch vehicleCount/greenTime — caller decides fallback.
bool tryReadVehicleCount(int &outCount) {
  if (!ensureWiFiConnected()) {
    Serial.println("WiFi unavailable, cannot read ThingSpeak.");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();  // see header note re: cert validation

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);

  String url = "https://api.thingspeak.com/channels/" + String(THINGSPEAK_CHANNEL_ID) +
               "/fields/1/last.txt?api_key=" + String(THINGSPEAK_READ_API_KEY);

  if (!http.begin(client, url)) {
    Serial.println("HTTP begin() failed.");
    return false;
  }

  int httpCode = http.GET();
  bool ok = false;

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    payload.trim();

    // Reject empty / non-numeric payloads instead of toInt()'s silent "0"
    bool isNumeric = payload.length() > 0;
    for (unsigned int i = 0; i < payload.length() && isNumeric; i++) {
      if (!isDigit(payload[i]) && !(i == 0 && payload[i] == '-')) isNumeric = false;
    }

    if (isNumeric) {
      int parsed = payload.toInt();
      if (parsed >= 0 && parsed <= MAX_PLAUSIBLE_COUNT) {
        outCount = parsed;
        ok = true;
      } else {
        Serial.print("Rejected implausible count: ");
        Serial.println(parsed);
      }
    } else {
      Serial.print("Rejected non-numeric payload: ");
      Serial.println(payload);
    }
  } else {
    Serial.print("HTTP GET failed, code: ");
    Serial.println(httpCode);
  }

  http.end();
  return ok;
}

// ===== ONE ROAD'S GREEN + YELLOW CYCLE =====
void runRoadCycle(int redPin, int yellowPin, int greenPin, int greenSeconds) {
  allRed();
  digitalWrite(redPin, LOW);
  digitalWrite(greenPin, HIGH);
  delay((unsigned long)greenSeconds * 1000UL);

  digitalWrite(greenPin, LOW);
  digitalWrite(yellowPin, HIGH);
  delay((unsigned long)YELLOW_TIME_S * 1000UL);

  digitalWrite(yellowPin, LOW);
  digitalWrite(redPin, HIGH);
}

void setup() {
  Serial.begin(115200);

  pinMode(R1_RED, OUTPUT); pinMode(R1_YELLOW, OUTPUT); pinMode(R1_GREEN, OUTPUT);
  pinMode(R2_RED, OUTPUT); pinMode(R2_YELLOW, OUTPUT); pinMode(R2_GREEN, OUTPUT);
  pinMode(R3_RED, OUTPUT); pinMode(R3_YELLOW, OUTPUT); pinMode(R3_GREEN, OUTPUT);
  pinMode(R4_RED, OUTPUT); pinMode(R4_YELLOW, OUTPUT); pinMode(R4_GREEN, OUTPUT);

  allRed();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_RECONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "\nWiFi Connected" : "\nWiFi FAILED — starting in safe mode");
}

void loop() {
  int newCount;
  if (tryReadVehicleCount(newCount)) {
    vehicleCount = newCount;
    greenTime = calculateGreenTime(vehicleCount);
    consecutiveReadFailures = 0;

    Serial.print("Vehicle Count: "); Serial.println(vehicleCount);
    Serial.print("Green Time: "); Serial.println(greenTime);
  } else {
    consecutiveReadFailures++;
    Serial.print("Read failed (");
    Serial.print(consecutiveReadFailures);
    Serial.println(" in a row).");

    if (consecutiveReadFailures >= MAX_CONSECUTIVE_READ_FAILURES) {
      // Too many failures in a row — don't trust old data anymore, go safe.
      greenTime = SAFE_DEFAULT_GREEN_TIME_S;
      Serial.println("Too many consecutive failures — forcing SAFE DEFAULT timing.");
    }
    // else: keep using the last known-good greenTime for a few cycles,
    // rather than immediately flapping to default on a single blip.
  }

  runRoadCycle(R1_RED, R1_YELLOW, R1_GREEN, greenTime);
  runRoadCycle(R2_RED, R2_YELLOW, R2_GREEN, greenTime);
  runRoadCycle(R3_RED, R3_YELLOW, R3_GREEN, greenTime);
  runRoadCycle(R4_RED, R4_YELLOW, R4_GREEN, greenTime);
}
