#include "jammer.h"

bool isJammer900On = false;
bool isJammer1500On = false;
bool isJammer2400On = false;
bool isJammer5200On = false;
bool isJammer5800On = false;

unsigned long lastJammerSensorReadTime = 0;

void turnOffAllJammers()
{
  digitalWrite(JAMM_900_PIN, LOW);
  digitalWrite(JAMM_1500_PIN, LOW);
  digitalWrite(JAMM_2400_PIN, LOW);
  digitalWrite(JAMM_5200_PIN, LOW);
  digitalWrite(JAMM_5800_PIN, LOW);
}

void updateJammerSensors()
{
  unsigned long currentTime = millis();

  if ((unsigned long)(currentTime - lastJammerSensorReadTime) >= JAMMER_SENSOR_READ_INTERVAL)
  {
    isJammer900On = (digitalRead(JAMM_900_SENS_PIN) == LOW);
    isJammer1500On = (digitalRead(JAMM_1500_SENS_PIN) == LOW);
    isJammer2400On = (digitalRead(JAMM_2400_SENS_PIN) == LOW);
    isJammer5200On = (digitalRead(JAMM_5200_SENS_PIN) == LOW);
    isJammer5800On = (digitalRead(JAMM_5800_SENS_PIN) == LOW);
    lastJammerSensorReadTime = currentTime;
  }
}
