#include <cassert>
#include <limits>

#include "config.h"
#include "fan.h"
#include "jammer.h"

static int pins[64] = {};
float cachedTempC0 = -127.0f;
float cachedTempC1 = -127.0f;
bool isJammer900On = false;
bool isJammer1500On = false;
bool isJammer2400On = false;
bool isJammer5200On = false;
bool isJammer5800On = false;

void digitalWrite(uint32_t pin, int value) { pins[pin] = value; }
int digitalRead(uint32_t pin) { return pins[pin]; }
void pinMode(uint32_t, int) {}

int main()
{
  initializeFan();
  assert(getFanMode() == FanMode::Auto && isFanOn() && pins[FAN_PIN] == HIGH);
  assert(!setManualFan(false));
  updateFan();
  assert(isFanOn());

  cachedTempC0 = 29.0f;
  cachedTempC1 = 29.0f;
  updateFan();
  assert(!isFanOn());

  cachedTempC0 = 40.0f;
  updateFan();
  assert(!isFanOn());
  cachedTempC0 = 40.1f;
  updateFan();
  assert(isFanOn());
  cachedTempC0 = 30.0f;
  updateFan();
  assert(isFanOn());
  cachedTempC0 = 29.0f;
  cachedTempC1 = 30.0f;
  updateFan();
  assert(isFanOn());
  cachedTempC1 = 29.0f;
  updateFan();
  assert(!isFanOn());

  cachedTempC1 = 41.0f;
  updateFan();
  assert(isFanOn());
  cachedTempC1 = 29.0f;
  updateFan();
  assert(!isFanOn());
  cachedTempC0 = -127.0f;
  updateFan();
  assert(isFanOn());
  cachedTempC0 = 29.0f;
  cachedTempC1 = std::numeric_limits<float>::quiet_NaN();
  updateFan();
  assert(isFanOn());
  cachedTempC1 = 29.0f;
  updateFan();
  assert(!isFanOn());

  const uint32_t jammerPins[] = {
    JAMM_900_PIN, JAMM_1500_PIN, JAMM_2400_PIN, JAMM_5200_PIN, JAMM_5800_PIN
  };
  bool *jammerSensors[] = {
    &isJammer900On, &isJammer1500On, &isJammer2400On, &isJammer5200On, &isJammer5800On
  };
  for (unsigned int i = 0; i < 5; ++i)
  {
    pins[jammerPins[i]] = HIGH;
    updateFan();
    assert(isFanOn());
    pins[jammerPins[i]] = LOW;
    updateFan();
    assert(!isFanOn());

    *jammerSensors[i] = true;
    updateFan();
    assert(isFanOn());
    *jammerSensors[i] = false;
    updateFan();
    assert(!isFanOn());
  }

  setFanMode(FanMode::Manual);
  assert(setManualFan(true));
  updateFan();
  assert(isFanOn());
  assert(setManualFan(false));
  pins[JAMM_5800_PIN] = HIGH;
  cachedTempC0 = -127.0f;
  updateFan();
  assert(!isFanOn());
  setFanMode(FanMode::Auto);
  assert(isFanOn());
  assert(!setManualFan(false));
  setFanMode(FanMode::Manual);
  assert(isFanOn());
  return 0;
}
