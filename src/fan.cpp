#include "fan.h"

#include "config.h"
#include "jammer.h"
#include "temperature.h"

static FanMode fanMode = FanMode::Auto;
static bool fanOn = false;

static void setFanOutput(bool on)
{
  fanOn = on;
  digitalWrite(FAN_PIN, on ? HIGH : LOW);
}

void initializeFan()
{
  fanMode = FanMode::Auto;
  setFanOutput(true);
  pinMode(FAN_PIN, OUTPUT);
}

void updateFan()
{
  if (fanMode != FanMode::Auto)
    return;

  bool jammerOn = digitalRead(JAMM_900_PIN) == HIGH ||
                  digitalRead(JAMM_1500_PIN) == HIGH ||
                  digitalRead(JAMM_2400_PIN) == HIGH ||
                  digitalRead(JAMM_5200_PIN) == HIGH ||
                  digitalRead(JAMM_5800_PIN) == HIGH ||
                  isJammer900On || isJammer1500On || isJammer2400On ||
                  isJammer5200On || isJammer5800On;

  bool valid0 = cachedTempC0 >= -55.0f && cachedTempC0 <= 125.0f;
  bool valid1 = cachedTempC1 >= -55.0f && cachedTempC1 <= 125.0f;

  if (jammerOn || !valid0 || !valid1 ||
      cachedTempC0 > FAN_AUTO_ON_TEMP_C || cachedTempC1 > FAN_AUTO_ON_TEMP_C)
  {
    setFanOutput(true);
  }
  else if (cachedTempC0 < FAN_AUTO_OFF_TEMP_C && cachedTempC1 < FAN_AUTO_OFF_TEMP_C)
  {
    setFanOutput(false);
  }
}

void setFanMode(FanMode mode)
{
  fanMode = mode;
  updateFan();
}

FanMode getFanMode()
{
  return fanMode;
}

bool isFanOn()
{
  return fanOn;
}

bool setManualFan(bool on)
{
  if (fanMode != FanMode::Manual)
    return false;

  setFanOutput(on);
  return true;
}
