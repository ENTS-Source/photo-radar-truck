#include <SPI.h>
#include <SD.h>
#include <SD_t3.h>

#define TRACKS 1
#define DISTANCE_MM 100
#define TOO_FAST_SPEED 4 // mm/s
#define CAMERA_FLASH_PIN 2
#define CAR_SCALE 32

#define SD_DIN 12
#define SD_DOUT 11
#define SD_CS 10
#define SD_SCLK 13
#define SD_CD A9
#define SD_PRESENT_THRESHOLD 350 // Testing shows that 1 ~= not present, 650 ~= present
#define SD_ENABLED false

#define SIGN_SERIAL Serial3
#define DEBUG_SERIAL Serial

int pins[][2] = {{A0, A1}, {A2, A3}, {A4, A5}, {A6, A7}}; // Pairs of pins, matching number of tracks

double triggerLevels[TRACKS][2];
long triggered[TRACKS];

void findTrackBaseline(int trackId) {
  int maxRounds = 5;
  int rounds[maxRounds][2];

  // Do rounds to get values
  for (int r = 0; r < maxRounds; r++) {
    rounds[r][0] = analogRead(pins[trackId][0]);
    rounds[r][1] = analogRead(pins[trackId][1]);
    delay(150);
  }

  // Find the mins and maxes for the sensors
  int sensorOneMin = 1024;
  int sensorTwoMin = 1024;
  int sensorOneMax = 0;
  int sensorTwoMax = 0;
  for (int r = 0; r < maxRounds; r++) {
    int val = rounds[r][0];
    DEBUG_SERIAL.println("Track " + String(trackId) + " round " + String(r) + " sensor 0 has value " + String(val));
    if (val > sensorOneMax) { sensorOneMax = val; }
    if (val < sensorOneMin) { sensorOneMin = val; }

    val = rounds[r][1];
    DEBUG_SERIAL.println("Track " + String(trackId) + " round " + String(r) + " sensor 1 has value " + String(val));
    if (val > sensorTwoMax) { sensorTwoMax = val; }
    if (val < sensorTwoMin) { sensorTwoMin = val; }
  }

  triggerLevels[trackId][0] = sensorOneMin - ((sensorOneMin + sensorOneMax) / 4.0); // over 4 because we want half the range
  triggerLevels[trackId][1] = sensorTwoMin - ((sensorTwoMin + sensorTwoMax) / 4.0); // over 4 because we want half the range
  DEBUG_SERIAL.println("Track " + String(trackId) + " has min (" + String(sensorOneMin) + ", " + String(sensorTwoMin) + ")");
  DEBUG_SERIAL.println("Track " + String(trackId) + " has max (" + String(sensorOneMax) + ", " + String(sensorTwoMax) + ")");
  DEBUG_SERIAL.println("Track " + String(trackId) + " has trigger level (" + String(triggerLevels[trackId][0]) + ", " + String(triggerLevels[trackId][1]) + ")");
}

void recordSpeed(int trackId, double timeDeltaMs, double distanceMm, double mmPerSecond) {
  DEBUG_SERIAL.println("Track " + String(trackId) + " moving at " + String(mmPerSecond) + " mm/s (in " + String(timeDeltaMs) + " ms)");
  if (analogRead(SD_CD) < SD_PRESENT_THRESHOLD) {
    DEBUG_SERIAL.println("Cannot write speed: SD Card not present");
    return;
  }

  const char* fileName = ("track" + String(trackId) + ".csv").c_str();
  File dataFile = SD.open(fileName, FILE_WRITE);
  if (dataFile) {
    dataFile.println(String(trackId) + "," + String(timeDeltaMs) + "," + String(distanceMm) + "," + String(mmPerSecond));
    dataFile.close();
    DEBUG_SERIAL.println("Speed recorded in csv");
  } else {
    DEBUG_SERIAL.println("Error writing speed: File error");
  }
}

void publishSpeed(int trackId, double timeDeltaMs, double distanceMm, double mmPerSecond) {
  // TODO: Check math is actually working
  double deltaHours = ((timeDeltaMs / 1000) * CAR_SCALE) / 60 / 60;
  double distanceKm = distanceMm / 1000000;
  double kmPerHour = distanceKm / deltaHours;
  int kmPerHourInt = round(kmPerHour);
  DEBUG_SERIAL.println("Publishing speed as " + String(kmPerHourInt) + " km/h");
  SIGN_SERIAL.println(String(kmPerHourInt));
}

void cameraFlash() {
  digitalWrite(CAMERA_FLASH_PIN, HIGH);
  delay(100);
  digitalWrite(CAMERA_FLASH_PIN, LOW);
  delay(25);
  digitalWrite(CAMERA_FLASH_PIN, HIGH);
  delay(100);
  digitalWrite(CAMERA_FLASH_PIN, LOW);
}

void errorLoop() {
  while(true) { 
    digitalWrite(CAMERA_FLASH_PIN, HIGH);
    delay(250);
    digitalWrite(CAMERA_FLASH_PIN, LOW);
    delay(250);
  }
}

void checkTrackTrigger(int trackId) {
  // Read the first sensor to determine if we tripped it or not
  int val = analogRead(pins[trackId][0]);
  int triggerVal = triggerLevels[trackId][0];
  long lastTriggeredTime = triggered[trackId];
  if (lastTriggeredTime == 0 && val <= triggerVal) {
    DEBUG_SERIAL.println("Track " + String(trackId) + " triggered on sensor 0");
    triggered[trackId] = millis();
    lastTriggeredTime = millis();
  }

  if (lastTriggeredTime == 0) { return; } // don't check second sensor if we're not triggered

  val = analogRead(pins[trackId][1]);
  triggerVal = triggerLevels[trackId][1];
  long currentTime = millis();
  if (currentTime - lastTriggeredTime >= 30000) {
    DEBUG_SERIAL.println("Track " + String(trackId) + " timed out");
    triggered[trackId] = 0;
    return;
  }
  if (val <= triggerVal) {
    DEBUG_SERIAL.println("Track " + String(trackId) + " triggered on sensor 1");
    double delta = (currentTime - lastTriggeredTime) * 1.0;
    if (delta == 0) { delta = 1; }

    double carSpeed = DISTANCE_MM / (delta / 1000); // mm/s
    recordSpeed(trackId, delta, DISTANCE_MM, carSpeed);
    publishSpeed(trackId, delta, DISTANCE_MM, carSpeed);
    
    triggered[trackId] = 0;

    if (carSpeed > TOO_FAST_SPEED) {
      DEBUG_SERIAL.println("Speeding detected on track " + String(trackId));
      cameraFlash();
    }
  }
}

void setup() {
  DEBUG_SERIAL.begin(9600);
  SIGN_SERIAL.begin(9600);
  pinMode(CAMERA_FLASH_PIN, OUTPUT);
  pinMode(SD_CD, INPUT);

  // Use camera flash as indicator that we're setting up
  digitalWrite(CAMERA_FLASH_PIN, HIGH);
  
  for (int i = 0; i < TRACKS; i++) {
    pinMode(pins[i][0], INPUT); // A0
    pinMode(pins[i][1], INPUT); // A1
    findTrackBaseline(i);
  }

  // Setup SD card
  if (SD_ENABLED && !SD.begin(SD_CS)) {
    DEBUG_SERIAL.println("SD Card failed or not present");
    errorLoop();
    return;
  } else if (SD_ENABLED) {
    DEBUG_SERIAL.println("SD Card enabled");
  } else {
    DEBUG_SERIAL.println("SD Card disabled by code");
  }
  
  digitalWrite(CAMERA_FLASH_PIN, LOW); // done setup
}

void loop() {
  for (int i = 0; i < TRACKS; i++) {
    checkTrackTrigger(i);
  }
}
