#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================
// DEBUG CONFIGURATION
// Set to false for production/release builds
// ============================================
#define DEBUG_ENABLED false

// Debug print macros - automatically disabled when DEBUG_ENABLED is false
#if DEBUG_ENABLED
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

// ============================================
// HARDWARE PIN DEFINITIONS
// ============================================

// W5500 Hardware Pins
#define W5500_SCK PB3
#define W5500_MISO PB4
#define W5500_MOSI PB5
#define W5500_CS PA15
#define W5500_RST PB8

// JAMM (Jammer) Output Pins
#define JAMM_900_PIN PA3
#define JAMM_1500_PIN PA5
#define JAMM_2400_PIN PA2
#define JAMM_5200_PIN PA1
#define JAMM_5800_PIN PA0

// FAN Pin
#define FAN_PIN PA4

// RGB LED Pin
// RGB values, RED = disconnected, BLUE = connected/Standby, YELLOW = Jamming
#define LED_RGB_PIN PA7
#define LED_RGB_NUM_PIXELS 7

// Temperature Sensor Pin
#define TEMP_SENSOR_PIN PB1

// Jammer Output State Sensor Pins (read LOW = ON)
#define JAMM_900_SENS_PIN PB9
#define JAMM_1500_SENS_PIN PC14
#define JAMM_2400_SENS_PIN PB7
#define JAMM_5200_SENS_PIN PB6
#define JAMM_5800_SENS_PIN PC13

// ============================================
// NETWORK CONFIGURATION
// ============================================
#define HTTP_PORT 8018

// ============================================
// TIMING CONSTANTS
// ============================================
#define JAMMER_SENSOR_READ_INTERVAL 2000UL // ms
#define JAMMER_LINK_DOWN_SHUTOFF_DELAY 20000UL // ms
#define NETWORK_CHECK_INTERVAL 5000UL      // Check every 5 seconds
#define MAX_NETWORK_FAILURES 24            // Reinit after 24 x 5s = 2 minutes
#define TEMP_READ_INTERVAL 5000UL         // 5 seconds
#define STATUS_LED_INTERVAL 500UL          // LED state refresh
#define ANIMATION_INTERVAL 20UL            // 50 FPS (20ms per frame)

#endif // CONFIG_H
