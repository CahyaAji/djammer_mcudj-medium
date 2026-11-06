#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"

// Temperature sensor setup (OneWire)
extern OneWire oneWire;
extern DallasTemperature tempSensor;

// Cached temperature readings
extern float cachedTempC0;
extern float cachedTempC1;

// Temperature conversion state
extern unsigned long lastTempReadTime;
extern bool tempConversionRequested;
extern unsigned long tempConversionStartTime;

// Kick off and complete non-blocking temperature reads
void updateTemperature();

#endif // TEMPERATURE_H
