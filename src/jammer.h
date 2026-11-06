#ifndef JAMMER_H
#define JAMMER_H

#include <Arduino.h>
#include "config.h"

// Jammer output state (updated from sensor pins)
extern bool isJammer900On;
extern bool isJammer1500On;
extern bool isJammer2400On;
extern bool isJammer5200On;
extern bool isJammer5800On;

// Jammer sensor reading throttling
extern unsigned long lastJammerSensorReadTime;

// Turn off every jammer output.
void turnOffAllJammers();

// Read jammer sensor pins on a throttled interval and update state variables
void updateJammerSensors();

#endif // JAMMER_H
