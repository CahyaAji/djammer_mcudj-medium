#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <IWatchdog.h>
#include <ArduinoOTA.h>
#include <Adafruit_NeoPixel.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ============================================
// DEBUG CONFIGURATION
// Set to false for production/release builds
// ============================================
#define DEBUG_ENABLED true

// Debug print macros - automatically disabled when DEBUG_ENABLED is false
#if DEBUG_ENABLED
  #define DEBUG_PRINT(x)    Serial.print(x)
  #define DEBUG_PRINTLN(x)  Serial.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

// W5500 Hardware Pins
#define W5500_SCK   PB3
#define W5500_MISO  PB4
#define W5500_MOSI  PB5
#define W5500_CS    PA15
#define W5500_RST   PB8

// JAMM Pin
#define JAMM_900_PIN   PA3
#define JAMM_1500_PIN  PA5
#define JAMM_2400_PIN  PA2
#define JAMM_5200_PIN  PA1
#define JAMM_5800_PIN  PA0

// FAN Pin
#define FAN_PIN    PA4
// #define SSR_SPARE  PA6

// STATE Pin
#define LED_BOARD PC13
// #define LED1_PIN  PB14
// #define LED2_PIN  PB15
#define LED_RGB_PIN PA7
#define LED_RGB_NUM_PIXELS 7
// #define BUZZER_PIN  PA8

#define TEMP_SENSOR_PIN PB1
// #define TEMP_SENSOR_1_PIN PB1

// Jammer Output State Sensor Pins
#define JAMM_900_SENS_PIN   PB9
#define JAMM_1500_SENS_PIN  PC14
#define JAMM_2400_SENS_PIN  PB7
#define JAMM_5200_SENS_PIN  PB6
#define JAMM_5800_SENS_PIN  PC13
// #define SPARE_SENS_PIN      PC15


struct TimedPulse {
  uint8_t pin;
  unsigned long endTime;
  bool active;
};

TimedPulse ledPulse = { LED_BOARD, 0, false };

void checkPulse(TimedPulse &p) {
  // Overflow-safe comparison: checks if current time has passed endTime
  if (p.active && (millis() - p.endTime) < 0x80000000UL) {
    digitalWrite(p.pin, LOW);
    p.active = false;
  }
}

// notifRGB (no 6 is middle of the ring)
// Parameter 1 = Number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = Pixel type flags, add together as needed (NEO_GRB is standard for WS2812)
Adafruit_NeoPixel notifRGB(LED_RGB_NUM_PIXELS, LED_RGB_PIN, NEO_GRB + NEO_KHZ800);

// Temperature sensor setup OneWire
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);

// Network configuration
// Mark change IP for different devices
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x51 };
IPAddress ip(192, 168, 10, 11);
IPAddress gateway(192, 168, 10, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress myDns(192, 168, 10, 1);

EthernetServer server(80);

// Jammer state variables
bool isJammer900On = false;
bool isJammer1500On = false;
bool isJammer2400On = false;
bool isJammer5200On = false;
bool isJammer5800On = false;

// Jammer sensor reading throttling
unsigned long lastJammerSensorReadTime = 0;
const unsigned long JAMMER_SENSOR_READ_INTERVAL = 2000UL; // in milliseconds

// Network health monitoring
unsigned long lastNetworkCheckTime = 0;
const unsigned long NETWORK_CHECK_INTERVAL = 30000UL; // Check every 30 seconds
uint8_t networkFailureCount = 0;
const uint8_t MAX_NETWORK_FAILURES = 3; // Reinit after 3 consecutive failures

float cachedTempC0 = -127.0f;
float cachedTempC1 = -127.0f;
unsigned long lastTempReadTime = 0;
const unsigned long TEMP_READ_INTERVAL = 10000UL; // 10 seconds
bool tempConversionRequested = false;
unsigned long tempConversionStartTime = 0;

// Animation state variables
bool animationRunning = false;
bool manualOverride = false;
unsigned long lastAnimationUpdate = 0;
const unsigned long animationInterval = 20UL; // 50 FPS (20ms per frame)
// notifRGB
uint8_t notifRotationOffset = 0;    // Current rotating dot position (0 to LED_RGB_NUM_PIXELS-1)
uint8_t rotationFrameSkip = 0;      // Frame counter to slow down rotation


// notifRGB
void updateNotifAnimation() {
    unsigned long now = millis();
    // Overflow-safe interval check
    if ((unsigned long)(now - lastAnimationUpdate) < animationInterval) return;
    lastAnimationUpdate = now;
    if (!animationRunning) return;

    // Advance the rotating dot position every ~15 frames (300ms) for slower rotation
    // Full rotation of 7 pixels takes ~2.1 seconds
    rotationFrameSkip++;
    if (rotationFrameSkip >= 15) {
        rotationFrameSkip = 0;
        notifRotationOffset = (notifRotationOffset + 1) % LED_RGB_NUM_PIXELS;
    }

    // Triangle-wave breathing for dark purple dimming
    // Phase cycles 0-511, creating a smooth pulse
    // Using modulo to prevent overflow issues in phase calculation
    uint16_t phase = ((now / 10) % 512);
    uint8_t breathe;
    if (phase > 255) {
        breathe = 511 - phase;  // falling edge
    } else {
        breathe = phase;        // rising edge
    }
    // Scale to dim purple range: ~3 to ~28
    breathe = (breathe * 25 / 255) + 3;

    // Blue dot is offset by ~half the ring for visual balance
    uint8_t bluePos = (notifRotationOffset + 3) % LED_RGB_NUM_PIXELS;

    for (int i = 0; i < LED_RGB_NUM_PIXELS; i++) {
        if (i == notifRotationOffset) {
            // Red dot at full brightness
            notifRGB.setPixelColor(i, notifRGB.Color(255, 0, 0));
        } else if (i == bluePos) {
            // Blue dot at full brightness
            notifRGB.setPixelColor(i, notifRGB.Color(0, 0, 255));
        } else {
            // Dark purple background with slow breathing
            notifRGB.setPixelColor(i, notifRGB.Color(breathe / 2, 0, breathe));
        }
    }
    notifRGB.show();
}

// Network initialization and recovery function
void initializeNetwork() {
  DEBUG_PRINTLN("[NET] Initializing network...");
  
  // Hard Reset W5500
  pinMode(W5500_RST, OUTPUT);
  digitalWrite(W5500_RST, LOW);
  delay(100);
  digitalWrite(W5500_RST, HIGH);
  delay(300);
  
  SPI.setMOSI(W5500_MOSI);
  SPI.setMISO(W5500_MISO);
  SPI.setSCLK(W5500_SCK);
  SPI.begin();

  Ethernet.init(W5500_CS);
  Ethernet.begin(mac, ip, myDns, gateway, subnet);
  
  // Give it time to stabilize
  delay(500);
  
  server.begin();

  DEBUG_PRINT("[NET] Server listening on: http://");
  DEBUG_PRINTLN(Ethernet.localIP());
}

// Check network health and recover if needed
void checkNetworkHealth() {
  // Check link status using W5500 hardware
  uint8_t linkStatus = Ethernet.linkStatus();
  
  if (linkStatus == LinkOFF) {
    networkFailureCount++;
    DEBUG_PRINT("[NET] Link DOWN detected. Failure count: ");
    DEBUG_PRINTLN(networkFailureCount);
    
    if (networkFailureCount >= MAX_NETWORK_FAILURES) {
      DEBUG_PRINTLN("[NET] Max failures reached. Reinitializing network...");
      initializeNetwork();
      networkFailureCount = 0;
    }
  } else if (linkStatus == LinkON) {
    // Link is good, reset failure counter
    if (networkFailureCount > 0) {
      DEBUG_PRINTLN("[NET] Link restored.");
      networkFailureCount = 0;
    }
  }
}

void sendJSONResponse(EthernetClient& client, int statusCode, const String& jsonMessage) {
  client.print("HTTP/1.1 ");
  client.print(statusCode);
  client.println(" OK\r\nContent-Type: application/json\r\nConnection: close\r\n");
  client.println(jsonMessage);
}

// --- Route: GET /led (Read current state) ---
void handleGetLedRoute(EthernetClient& client) {
  bool currentState = digitalRead(LED_BOARD);
  // Note: On many STM32 boards like Black Pill, PC13 is active-LOW. 
  // If your LED turns on when pin is HIGH, keep this as-is.
  String response = "{\"status\":\"success\",\"led_on\":" + String(currentState ? "true" : "false") + "}";
  sendJSONResponse(client, 200, response);
}

// --- Route: POST /led1/pulse (Turn on LED1 for 3 seconds) ---
void handlePostLed1PulseRoute(EthernetClient& client) {
  digitalWrite(ledPulse.pin, HIGH);
  // Overflow-safe: endTime will wrap correctly with unsigned arithmetic
  ledPulse.endTime = millis() + 3000UL;
  ledPulse.active = true;
  sendJSONResponse(client, 200, "{\"success\": true,\"message\":\"LED1 pulse started for 3 seconds\"}");
}

void handleGetTempRoute(EthernetClient& client) {
  JsonDocument doc; 

  doc["status"] = "success";
  doc["temp_0_c"] = cachedTempC0;
  doc["temp_1_c"] = cachedTempC1;

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  sendJSONResponse(client, 200, jsonResponse);
}

void handleGetJammStateRoute(EthernetClient& client) {
  JsonDocument doc; 

  doc["status"] = "success";
  doc["jamm_900"] = isJammer900On;
  doc["jamm_1500"] = isJammer1500On;
  doc["jamm_2400"] = isJammer2400On;
  doc["jamm_5200"] = isJammer5200On;
  doc["jamm_5800"] = isJammer5800On;

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  sendJSONResponse(client, 200, jsonResponse);
}

void handleNotifRGBControl(EthernetClient& client) {
  JsonDocument doc; 
  DeserializationError error = deserializeJson(doc, client);

  if (error) {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Invalid JSON or body missing\"}");
    return;
  }

  if (!doc["is_on"].is<bool>() || !doc["r"].is<int>() || !doc["g"].is<int>() || !doc["b"].is<int>() || !doc["numb"].is<byte>()) {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Missing or invalid keys (is_on, r, g, b, numb required)\"}");
    return;
  }

    DEBUG_PRINTLN("[HTTP] Received /notifRGB request");

    bool isOn = doc["is_on"];
    int r = doc["r"];
    int g = doc["g"];
    int b = doc["b"];
    byte numb = doc["numb"];

    // Boundary check for RGB ranges
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 || numb >= LED_RGB_NUM_PIXELS) {
        sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"RGB values must be 0-255 and pixel index must be valid\"}");
        return;
    }

    if(isOn == false) {
      notifRGB.clear();
    } else {
      // for(int i=0; i<LED_RGB_NUM_PIXELS; i++) {
      //   notifRGB.setPixelColor(i, notifRGB.Color(r, g, b));
      // }
      notifRGB.setPixelColor(numb, notifRGB.Color(r, g, b));
    }

    notifRGB.show();

    JsonDocument responseDoc;
    responseDoc["success"] = true;
    responseDoc["is_on"] = isOn ? "ON" : "OFF";
    responseDoc["color"] = String(r) + "," + String(g) + "," + String(b);
    responseDoc["pixel"] = numb;

    String jsonResponse;
    serializeJson(responseDoc, jsonResponse);
    sendJSONResponse(client, 200, jsonResponse);

}

// notifRGB
void handleAnimateRGBControl(EthernetClient& client) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, client);

  if (error) {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Invalid JSON or body missing\"}");
    return;
  }

  if (!doc["is_on"].is<bool>()) {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Missing or invalid key: is_on (boolean required)\"}");
    return;
  }

    DEBUG_PRINTLN("[HTTP] Received /animateRGB request");

    bool isOn = doc["is_on"];

    if(isOn == false) {
      animationRunning = false;
      notifRGB.clear();
      notifRGB.show();
    } else {
      animationRunning = true;
    }

    JsonDocument responseDoc;
    responseDoc["success"] = true;
    responseDoc["is_on"] = isOn ? "ON" : "OFF";
    responseDoc["message"] = isOn ? "Animation started" : "Animation stopped";

    String jsonResponse;
    serializeJson(responseDoc, jsonResponse);
    sendJSONResponse(client, 200, jsonResponse);
}

void handlePostFanRoute(EthernetClient& client) {
  JsonDocument doc; 
  DeserializationError error = deserializeJson(doc, client);

  if (error) {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Invalid JSON or body missing\"}");
    return;
  }

  // Validate that required keys
  if (!doc["is_on"].is<bool>()) {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Missing or invalid key: fan (boolean required)\"}");
    return;
  }

  DEBUG_PRINTLN("[HTTP] Received Fan Control Command");

  bool isOn = doc["is_on"];
  digitalWrite(FAN_PIN, isOn);
  sendJSONResponse(client, 200, "{\"success\": true,\"message\":\"Fan state updated\"}");

}

void handlePostJammerRoute(EthernetClient& client) {
  JsonDocument doc; 
  DeserializationError error = deserializeJson(doc, client);

  if (error) {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Invalid JSON or body missing\"}");
    return;
  }

  // Validate that required keys
  if (!doc["jamm_freq"].is<int>() || !doc["is_on"].is<bool>()) {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Missing a key\"}");
    return;
  }

  DEBUG_PRINTLN("[HTTP] Received Jammer Command");

  // Process the jammer command
  int jammerFreq = doc["jamm_freq"];
  bool isOn = doc["is_on"];

  switch (jammerFreq) {
    case 900:
        digitalWrite(JAMM_900_PIN, isOn);
        break;
    case 1500:
        digitalWrite(JAMM_1500_PIN, isOn);
        break;
    case 2400:
        digitalWrite(JAMM_2400_PIN, isOn);
        break;
    case 5200:
        digitalWrite(JAMM_5200_PIN, isOn);
        break;
    case 5800:
        digitalWrite(JAMM_5800_PIN, isOn);
        break;
    default:
        sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Invalid 'jamm' frequency\"}");
        return;
    }

    DEBUG_PRINT("[HTTP] Jammer frequency ");
    DEBUG_PRINT(jammerFreq);
    DEBUG_PRINT(" is now ");
    DEBUG_PRINTLN(isOn ? "ON" : "OFF");
    String response = "{\"success\": true,\"message\":\"Jammer frequency " + String(jammerFreq) + " is now " + (isOn ? "ON" : "OFF") + "\"}";
    sendJSONResponse(client, 200, response);
}

// --- Route: POST /ota ---
void handleOtaRoute(EthernetClient& client, long expectedSize) {
  if (expectedSize <= 0 || expectedSize > 256000) {
    sendJSONResponse(client, 400, "{\"status\":\"error\",\"message\":\"Invalid size constraints\"}");
    return;
  }

  DEBUG_PRINT("[OTA] Initializing landing partition allocation for: ");
  DEBUG_PRINT(expectedSize);
  DEBUG_PRINTLN(" bytes.");

  if (InternalStorage.open(expectedSize)) {
    uint8_t buffer[512];
    long totalReceived = 0;

    while (client.connected() || client.available()) {
      IWatchdog.reload();
      if (client.available()) {
        int len = client.read(buffer, sizeof(buffer));
        if (len > 0) {
          for (int j = 0; j < len; j++) {
            InternalStorage.write(buffer[j]);
            totalReceived++;
          }
        }
      }
      if (totalReceived >= expectedSize) {
        break;
      }
    }
    InternalStorage.close();

    DEBUG_PRINT("[OTA] Extraction finished. Received count: ");
    DEBUG_PRINTLN(totalReceived);

    if (totalReceived == expectedSize) {
      DEBUG_PRINTLN("[OTA] Verification Success! Performing system remap...");
      sendJSONResponse(client, 200, "{\"status\":\"success\",\"message\":\"Flashing complete. Rebooting.\"}");
      client.stop();
      delay(500);
      InternalStorage.apply();
    } else {
      DEBUG_PRINTLN("[OTA] Critical Fail: Packet sizes do not match header metadata.");
    }
  }
  
  client.stop();
  NVIC_SystemReset();
}

void setup() {
  Serial.begin(115200);
  IWatchdog.begin(20000000); 

  tempSensor.begin(); // Initialize the DallasTemperature library
  DEBUG_PRINT("[TEMP] Sensors found: ");
  DEBUG_PRINTLN(tempSensor.getDeviceCount());


  pinMode(LED_BOARD, OUTPUT);

  pinMode(JAMM_900_PIN, OUTPUT);
  pinMode(JAMM_1500_PIN, OUTPUT);
  pinMode(JAMM_2400_PIN, OUTPUT);
  pinMode(JAMM_5200_PIN, OUTPUT);
  pinMode(JAMM_5800_PIN, OUTPUT);

  pinMode(FAN_PIN, OUTPUT);

  pinMode(LED_BOARD, OUTPUT);
  // pinMode(LED_PWM_PIN, OUTPUT);
  // pinMode(BUZZER_PIN, OUTPUT);

  pinMode(JAMM_900_SENS_PIN, INPUT);
  pinMode(JAMM_1500_SENS_PIN, INPUT);
  pinMode(JAMM_2400_SENS_PIN, INPUT);
  pinMode(JAMM_5200_SENS_PIN, INPUT);
  pinMode(JAMM_5800_SENS_PIN, INPUT);

  digitalWrite(JAMM_1500_PIN, LOW);
  digitalWrite(JAMM_900_PIN, LOW);
  digitalWrite(JAMM_2400_PIN, LOW);
  digitalWrite(JAMM_5200_PIN, LOW);
  digitalWrite(JAMM_5800_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  // digitalWrite(LED_BOARD, LOW);
  digitalWrite(LED_BOARD, HIGH);

  notifRGB.begin();
  notifRGB.show(); // Initialize all pixels to 'off'

  // Initialize network with recovery capability
  initializeNetwork();
}

void loop() {
  IWatchdog.reload();

  // Check LED pulse timing (runs every loop for precision)
  checkPulse(ledPulse);

  // Get current time once for all timing checks
  unsigned long currentTime = millis();

  // Throttled jammer sensor reading (every 1 second)
  if ((unsigned long)(currentTime - lastJammerSensorReadTime) >= JAMMER_SENSOR_READ_INTERVAL) {
    isJammer900On = (digitalRead(JAMM_900_SENS_PIN) == LOW);
    isJammer1500On = (digitalRead(JAMM_1500_SENS_PIN) == LOW);
    isJammer2400On = (digitalRead(JAMM_2400_SENS_PIN) == LOW);
    isJammer5200On = (digitalRead(JAMM_5200_SENS_PIN) == LOW);
    isJammer5800On = (digitalRead(JAMM_5800_SENS_PIN) == LOW);
    lastJammerSensorReadTime = currentTime;
  }

  // notifRGB animation update (throttled internally to 20ms)
  updateNotifAnimation();

  // Network health check (every 30 seconds)
  if ((unsigned long)(currentTime - lastNetworkCheckTime) >= NETWORK_CHECK_INTERVAL) {
    checkNetworkHealth();
    lastNetworkCheckTime = currentTime;
  }

  // Overflow-safe interval check for temperature reading (every 10 seconds)
  if (!tempConversionRequested && ((unsigned long)(currentTime - lastTempReadTime) >= TEMP_READ_INTERVAL)) {
    tempSensor.requestTemperatures();
    tempConversionRequested = true;
    tempConversionStartTime = currentTime;
  }

  // Overflow-safe check for temperature conversion completion (750ms delay)
  if (tempConversionRequested && ((unsigned long)(currentTime - tempConversionStartTime) >= 750UL)) {
    cachedTempC0 = tempSensor.getTempCByIndex(0);
    cachedTempC1 = tempSensor.getTempCByIndex(1);
    lastTempReadTime = currentTime;
    tempConversionRequested = false;
  }

  EthernetClient client = server.available();
  if (client) {
    String currentLine = "";
    String method = "";  // Added variable to catch GET/POST
    String route = "";
    long contentLength = 0;
    unsigned long timeoutStart = millis();

    while (client.connected()) {
      IWatchdog.reload();
      // Overflow-safe timeout check (3 second timeout)
      if ((unsigned long)(millis() - timeoutStart) > 3000UL) {
        break;
      }

      if (client.available()) {
        char c = client.read();
        
        if (c == '\n') {
          if (currentLine.length() == 0) {
            break; 
          }

          // --- EXTRACT METHOD AND ROUTE ---
          // Example first line: "POST /led HTTP/1.1"
          if (currentLine.startsWith("POST ") || currentLine.startsWith("GET ")) {
            int firstSpace = currentLine.indexOf(' ');
            int secondSpace = currentLine.indexOf(' ', firstSpace + 1);
            
            method = currentLine.substring(0, firstSpace);
            route = currentLine.substring(firstSpace + 1, secondSpace);

            DEBUG_PRINT("[HTTP] Intercepted Request: ");
            DEBUG_PRINT(method);
            DEBUG_PRINT(" -> ");
            DEBUG_PRINTLN(route);
          }

          String lowercaseLine = currentLine;
          lowercaseLine.toLowerCase();

          if (lowercaseLine.startsWith("content-length:")) {
            int colonIdx = currentLine.indexOf(':');
            if (colonIdx > 0) {
              String sizeStr = currentLine.substring(colonIdx + 1);
              sizeStr.trim();
              contentLength = atol(sizeStr.c_str()); 
            }
          }

          currentLine = "";
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }

    // --- STRATEGIC METHOD-BASED ROUTER BLOCK ---
    if (method == "GET" && route == "/led") {
      handleGetLedRoute(client);
    } 
    else if (method == "POST" && route == "/led1/pulse") {
      handlePostLed1PulseRoute(client);
    }
    else if (method == "POST" && route == "/jammer") {
      handlePostJammerRoute(client);
    }
    else if (method == "POST" && route == "/fan") {
      handlePostFanRoute(client);
    }
    else if (method == "POST" && route == "/ota") {
      handleOtaRoute(client, contentLength);
    } 
    else if (method == "POST" && route == "/notifRGB") {
      handleNotifRGBControl(client);

    }
    // notifRGB
    else if (method == "POST" && route == "/animateRGB") {
      handleAnimateRGBControl(client);

    }
    // Jammer state route
    else if (method == "GET" && route == "/jamm_state") {
      isJammer900On = (digitalRead(JAMM_900_SENS_PIN) == LOW);
      isJammer1500On = (digitalRead(JAMM_1500_SENS_PIN) == LOW);
      isJammer2400On = (digitalRead(JAMM_2400_SENS_PIN) == LOW);
      isJammer5200On = (digitalRead(JAMM_5200_SENS_PIN) == LOW);
      isJammer5800On = (digitalRead(JAMM_5800_SENS_PIN) == LOW);
      lastJammerSensorReadTime = currentTime;

      handleGetJammStateRoute(client);
    }

    // Temp route
    else if (method == "GET" && route == "/temp") {
      handleGetTempRoute(client);
    }

    else {
      sendJSONResponse(client, 404, "{\"status\":\"error\",\"message\":\"Endpoint or Method unsupported\"}");
    }

    client.stop();
  }
}