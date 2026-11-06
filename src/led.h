#ifndef LED_H
#define LED_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

// notifRGB (index 6 is the middle of the ring)
extern Adafruit_NeoPixel notifRGB;

// Animation state variables
extern bool animationRunning;
extern bool manualOverride;
extern unsigned long lastAnimationUpdate;

// Rotating dot state
extern uint8_t notifRotationOffset; // Current rotating dot position (0 to LED_RGB_NUM_PIXELS-1)
extern uint8_t rotationFrameSkip;   // Frame counter to slow down rotation

// Update the rotating-dot animation (throttled internally to 20ms)
void updateNotifAnimation();

#endif // LED_H
