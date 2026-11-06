#include "temperature.h"

OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);

float cachedTempC0 = -127.0f;
float cachedTempC1 = -127.0f;

unsigned long lastTempReadTime = 0;
bool tempConversionRequested = false;
unsigned long tempConversionStartTime = 0;

void updateTemperature()
{
  unsigned long currentTime = millis();

  // Overflow-safe interval check: request a new conversion every 10 seconds
  if (!tempConversionRequested && ((unsigned long)(currentTime - lastTempReadTime) >= TEMP_READ_INTERVAL))
  {
    tempSensor.requestTemperatures();
    tempConversionRequested = true;
    tempConversionStartTime = currentTime;
  }

  // Overflow-safe check for conversion completion (750ms delay)
  if (tempConversionRequested && ((unsigned long)(currentTime - tempConversionStartTime) >= 750UL))
  {
    cachedTempC0 = tempSensor.getTempCByIndex(0);
    cachedTempC1 = tempSensor.getTempCByIndex(1);
    lastTempReadTime = currentTime;
    tempConversionRequested = false;
  }
}
