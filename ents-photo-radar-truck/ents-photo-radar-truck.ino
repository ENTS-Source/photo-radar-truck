#define TRACKS 1
#define DISTANCE_MM 10
#define TOO_FAST_SPEED 4 // mm/s
#define CAMERA_FLASH_PIN 10

double triggerLevels[TRACKS][2];
long triggered[TRACKS];

int pins[][2] = {{A0, A1}, {A2, A3}, {A4, A5}, {A6, A7}}; // Pairs of pins, matching number of tracks

void setup() {
  Serial.begin(9600);

  for (int i = 0; i < TRACKS; i++) {
    pinMode(pins[i][0], INPUT); // A0
    pinMode(pins[i][1], INPUT); // A1
  }

  pinMode(13, OUTPUT); // LED, onboard
  pinMode(CAMERA_FLASH_PIN, OUTPUT);
  digitalWrite(13, HIGH);

  int maxRounds = 5;
  int roundResults[TRACKS][maxRounds][2];
  for (int r = 0; r < maxRounds; r++){
    for (int i = 0; i < TRACKS; i++) {
       roundResults[i][r][0] = analogRead(pins[i][0]);
       roundResults[i][r][1] = analogRead(pins[i][1]);
    }
    delay(100);
  }

  for (int i = 0; i < TRACKS; i++) {
    int mi1 = 1024;
    int ma1 = 0;
    int mi2 = 1024;
    int ma2 = 0;
    for (int r = 0; r < maxRounds; r++) {
      int val = roundResults[i][r][0];
      Serial.println("Track " + String(i) + " round " + String(r) + " has value " + String(val));
      if (val > ma1) {
        ma1 = val;
      }
      if (val < mi1) {
        mi1 = val;
      }
      
      val = roundResults[i][r][1];
      if (val > ma2) {
        ma2 = val;
      }
      if (val < mi2) {
        mi2 = val;
      }
    }

    triggerLevels[i][0] = mi1 - ((mi1 + ma1) / 4.0);
    triggerLevels[i][1] = mi2 - ((mi2 + ma2) / 4.0);
    Serial.println("Track " + String(i) + " has min (" + String(mi1) + ", " + String(mi2) +") and max (" + String(ma1) + ", " + String(ma2) +") and triggers at (" + String(triggerLevels[i][0]) + ", " + String(triggerLevels[i][1]) + ")");
  }

  digitalWrite(13, LOW);
}

void loop() {
  // Check for triggers
  for (int i = 0; i < TRACKS; i++) {
    int val = analogRead(pins[i][0]);
    int triggerVal = triggerLevels[i][0];
    int currentTime = triggered[i];
    if (currentTime == 0 && val <= triggerVal) {
      Serial.println("Track " + String(i) + " triggered on sensor 0");
      triggered[i] = millis();
    }
  }

  // Check for signal finish
  for (int i = 0; i < TRACKS; i++) {
    int val = analogRead(pins[i][1]);
    int triggerVal = triggerLevels[i][1];
    int triggeredTime = triggered[i];
    if (millis() - triggeredTime > 30000 && triggeredTime != 0) {
      Serial.println("Track " + String(i) + " timed out");
      triggered[i] = 0;
      triggeredTime = 0;
    }
    if (triggeredTime != 0 && val <= triggerVal) {
      Serial.println("Track " + String(i) + " triggered on sensor 1");
      double delta = (millis() - triggeredTime) * 1.0;
      if (delta == 0) {
        delta = 1.0;
      }
      
      double carSpeed = (DISTANCE_MM) / (delta / 1000);
      Serial.println("Track " + String(i) + " moving at " + String(carSpeed) + " mm/s");
      triggered[i] = 0;

      if (carSpeed > TOO_FAST_SPEED) {
        Serial.println("Speeding detected on track " + String(i));
        digitalWrite(CAMERA_FLASH_PIN, HIGH);
        delay(100);
        digitalWrite(CAMERA_FLASH_PIN, LOW);
        delay(100);
        digitalWrite(CAMERA_FLASH_PIN, HIGH);
        delay(100);
        digitalWrite(CAMERA_FLASH_PIN, LOW);
      }
    }
  }
}
