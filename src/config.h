#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Firmware version reported by GET /version; update before building a release.
#define FIRMWARE_VERSION "1.1.0"

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
#define JAMM_900_PIN  PA2  //PA3
#define JAMM_1500_PIN PA7  //PA5
#define JAMM_2400_PIN PA3  //PA2
#define JAMM_5200_PIN PA1
#define JAMM_5800_PIN PA0

// FAN Pin
#define FAN_PIN PA6  //PA4
#define FAN_AUTO_ON_TEMP_C 40.0f
#define FAN_AUTO_OFF_TEMP_C 30.0f

// RGB LED Pin
// RGB values, RED = disconnected, BLUE = connected/Standby, YELLOW = Jamming
#define LED_RGB_PIN PB0  //PA7
#define LED_RGB_NUM_PIXELS 7

// Temperature Sensor Pin
#define TEMP_SENSOR_PIN PB1

// Jammer Output State Sensor Pins (read LOW = ON)
#define JAMM_900_SENS_PIN PB9
#define JAMM_1500_SENS_PIN PB7  //PC14
#define JAMM_2400_SENS_PIN PB6  //PB7
#define JAMM_5200_SENS_PIN PC13  //PB6
#define JAMM_5800_SENS_PIN PC14  //PC13

// ============================================
// NETWORK CONFIGURATION
// ============================================
#define HTTP_PORT 8018
#define MAC_ADDRESS {0xDE, 0xAD, 0xBE, 0xEF, 0x41, 0x00} // Change this to a unique MAC address for your device
#define STATIC_IP 192, 168, 17, 150
#define GATEWAY_IP 192, 168, 17, 1
#define SUBNET_MASK 255, 255, 255, 0
#define DNS_IP 192, 168, 10, 1

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
