#include "led.h"
#include <Ethernet.h>
#include "jammer.h"
#include "network.h"

// Parameter 1 = Number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = Pixel type flags (NEO_GRB is standard for WS2812)
Adafruit_NeoPixel notifRGB(LED_RGB_NUM_PIXELS, LED_RGB_PIN, NEO_GRB + NEO_KHZ800);

bool animationRunning = false;
bool manualOverride = false;
unsigned long lastAnimationUpdate = 0;

uint8_t notifRotationOffset = 0;
uint8_t rotationFrameSkip = 0;

void updateNotifAnimation()
{
  unsigned long now = millis();

  // Overflow-safe interval check
  if ((unsigned long)(now - lastAnimationUpdate) < ANIMATION_INTERVAL)
    return;
  lastAnimationUpdate = now;

  if (!animationRunning)
    return;

  // Advance the rotating dot position every ~15 frames (300ms) for slower rotation.
  // Full rotation of 6 ring pixels takes ~1.8 seconds.
  rotationFrameSkip++;
  if (rotationFrameSkip >= 15)
  {
    rotationFrameSkip = 0;
    // Rotate only on the ring (indices 0-5), skip center (index 6).
    // Decrement for clockwise rotation.
    notifRotationOffset = (notifRotationOffset - 1 + (LED_RGB_NUM_PIXELS - 1)) % (LED_RGB_NUM_PIXELS - 1);
  }

  for (int i = 0; i < LED_RGB_NUM_PIXELS; i++)
  {
    if (i == 6)
    {
      // Center LED: steady dim yellow
      notifRGB.setPixelColor(i, notifRGB.Color(5, 5, 0));
    }
    else if (i == notifRotationOffset)
    {
      // Rotating blue dot on the ring
      notifRGB.setPixelColor(i, notifRGB.Color(0, 0, 255));
    }
    else
    {
      // Ring background: off
      notifRGB.setPixelColor(i, notifRGB.Color(5, 5, 0));
    }
  }

  notifRGB.show();
}

void updateStatusLed()
{
  static unsigned long lastUpdate = 0;
  static uint32_t lastColor = 0;
  static bool hasColor = false;

  unsigned long now = millis();
  if ((unsigned long)(now - lastUpdate) < STATUS_LED_INTERVAL)
    return;
  lastUpdate = now;

  uint32_t color;
  if (networkFault || Ethernet.linkStatus() == LinkOFF)
    color = notifRGB.Color(200, 0, 0);
  else if (isJammer900On || isJammer1500On || isJammer2400On || isJammer5200On || isJammer5800On)
    color = notifRGB.Color(200, 150, 0);
  else
    color = notifRGB.Color(0, 0, 200);

  if (hasColor && color == lastColor)
    return;

  for (int i = 0; i < LED_RGB_NUM_PIXELS; i++)
    notifRGB.setPixelColor(i, color);
  notifRGB.show();
  lastColor = color;
  hasColor = true;
}
