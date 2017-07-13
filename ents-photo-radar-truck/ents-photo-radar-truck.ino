#define TRACKS 1
#define DISTANCE_MM 10
#define TOO_FAST_SPEED 4 // mm/s
#define CAMERA_FLASH_PIN 10
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
    delay(100);
  }

  // Find the mins and maxes for the sensors
  int sensorOneMin = 1024;
  int sensorTwoMin = 1024;
  int sensorOneMax = 0;
  int sensorTwoMax = 0;
  for (int r = 0; r < maxRounds; r++) {
    int val = rounds[r][0];
    Serial.println("Track " + String(trackId) + " round " + String(r) + " sensor 0 has value " + String(val));
    if (val > sensorOneMax) { sensorOneMax = val; }
    if (val < sensorOneMin) { sensorOneMin = val; }

    val = rounds[r][1];
    Serial.println("Track " + String(trackId) + " round " + String(r) + " sensor 1 has value " + String(val));
    if (val > sensorTwoMax) { sensorOneMax = val; }
    if (val < sensorTwoMin) { sensorOneMin = val; }
  }

  triggerLevels[trackId][0] = sensorOneMin - ((sensorOneMin + sensorOneMax) / 4.0); // over 4 because we want half the range
  triggerLevels[trackId][1] = sensorTwoMin - ((sensorTwoMin + sensorTwoMax) / 4.0); // over 4 because we want half the range
  Serial.println("Track " + String(trackId) + " has min (" + String(sensorOneMin) + ", " + String(sensorTwoMin) + ")");
  Serial.println("Track " + String(trackId) + " has max (" + String(sensorOneMax) + ", " + String(sensorTwoMax) + ")");
  Serial.println("Track " + String(trackId) + " has trigger level (" + String(triggerLevels[trackId][0]) + ", " + String(triggerLevels[trackId][1]) + ")");
}

void recordSpeed(int trackId, double timeDeltaMs, double mmPerSecond) {
  Serial.println("Track " + String(trackId) + " moving at " + String(mmPerSecond) + " mm/s (in " + String(timeDeltaMs) + " ms)");
}

void cameraFlash() {
  digitalWrite(CAMERA_FLASH_PIN, HIGH);
  delay(100);
  digitalWrite(CAMERA_FLASH_PIN, LOW);
  delay(100);
  digitalWrite(CAMERA_FLASH_PIN, HIGH);
  delay(100);
  digitalWrite(CAMERA_FLASH_PIN, LOW);
}

void checkTrackTrigger(int trackId) {
  // Read the first sensor to determine if we tripped it or not
  int val = analogRead(pins[trackId][0]);
  int triggerVal = triggerLevels[trackId][0];
  long lastTriggeredTime = triggered[trackId];
  if (lastTriggeredTime == 0 && val <= triggerVal) {
    Serial.println("Track " + String(trackId) + " triggered on sensor 0");
    triggered[trackId] = millis();
    lastTriggeredTime = millis();
  }

  if (lastTriggeredTime == 0) { return; } // don't check second sensor if we're not triggered

  val = analogRead(pins[trackId][1]);
  triggerVal = triggerLevels[trackId][1];
  long currentTime = millis();
  if (currentTime - lastTriggeredTime >= 30000) {
    Serial.println("Track " + String(trackId) + " timed out");
    triggered[trackId] = 0;
    return;
  }
  if (val <= triggerVal) {
    double delta = (currentTime - lastTriggeredTime) * 1.0;
    if (delta == 0) { delta = 1; }

    double carSpeed = DISTANCE_MM / (delta / 1000); // mm/s
    recordSpeed(trackId, delta, carSpeed);
    
    triggered[trackId] = 0;

    if (carSpeed > TOO_FAST_SPEED) {
      Serial.println("Speeding detected on track " + String(trackId));
      cameraFlash();
    }
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(CAMERA_FLASH_PIN, OUTPUT);

  // Use camera flash as indicator that we're setting up
  digitalWrite(CAMERA_FLASH_PIN, HIGH);
  
  for (int i = 0; i < TRACKS; i++) {
    pinMode(pins[i][0], INPUT); // A0
    pinMode(pins[i][1], INPUT); // A1
    findTrackBaseline(i);
  }
  
  digitalWrite(CAMERA_FLASH_PIN, LOW); // done setup
}

void loop() {
  for (int i = 0; i < TRACKS; i++) {
    checkTrackTrigger(i);
  }
}
