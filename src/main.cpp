#include <Arduino.h>
#include <IWatchdog.h>

#include "config.h"
#include "fan.h"
#include "jammer.h"
#include "led.h"
#include "network.h"
#include "temperature.h"
#include "handlers.h"

void setup()
{
  IWatchdog.begin(20000000);

  initializeFan();

  tempSensor.begin(); // Initialize the DallasTemperature library
  // updateTemperature() already waits 750 ms itself; a blocking request would stall the HTTP server
  tempSensor.setWaitForConversion(false);

  // Jammer output pins
  pinMode(JAMM_900_PIN, OUTPUT);
  pinMode(JAMM_1500_PIN, OUTPUT);
  pinMode(JAMM_2400_PIN, OUTPUT);
  pinMode(JAMM_5200_PIN, OUTPUT);
  pinMode(JAMM_5800_PIN, OUTPUT);

  // Jammer output state sensor pins
  pinMode(JAMM_900_SENS_PIN, INPUT);
  pinMode(JAMM_1500_SENS_PIN, INPUT);
  pinMode(JAMM_2400_SENS_PIN, INPUT);
  pinMode(JAMM_5200_SENS_PIN, INPUT);
  pinMode(JAMM_5800_SENS_PIN, INPUT);

  // Ensure all outputs start in a known (off) state
  digitalWrite(JAMM_900_PIN, LOW);
  digitalWrite(JAMM_1500_PIN, LOW);
  digitalWrite(JAMM_2400_PIN, LOW);
  digitalWrite(JAMM_5200_PIN, LOW);
  digitalWrite(JAMM_5800_PIN, LOW);

  // Initialize RGB indicator strip
  notifRGB.begin();
  for (int i = 0; i < LED_RGB_NUM_PIXELS; i++)
  {
    notifRGB.setPixelColor(i, notifRGB.Color(200, 0, 0)); // Set all pixels to 'off'
  }
  notifRGB.show();

  // Initialize network with recovery capability
  initializeNetwork();
}

void loop()
{
  IWatchdog.reload();

  // Throttled jammer sensor reading
  updateJammerSensors();
  updateFan();

  // RGB animation disabled: LED shows real network state only
  // updateNotifAnimation();
  updateStatusLed();

  // Network health check (throttled internally)
  unsigned long currentTime = millis();
  if ((unsigned long)(currentTime - lastNetworkCheckTime) >= NETWORK_CHECK_INTERVAL)
  {
    checkNetworkHealth();
    lastNetworkCheckTime = currentTime;
  }

  // Non-blocking temperature conversion
  updateTemperature();
  updateFan();

  // Serve an incoming HTTP request, if any
  handleHttpRequest();

  // Enforce the jammer shutdown grace period after Ethernet link loss.
  enforceJammerLinkSafety();
  updateFan();
}