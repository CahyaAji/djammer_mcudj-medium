#include "handlers.h"

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <IWatchdog.h>

#include "config.h"
#include "fan.h"
#include "jammer.h"
#include "led.h"
#include "network.h"
#include "temperature.h"

void sendJSONResponse(EthernetClient &client, int statusCode, const String &jsonMessage)
{
  const char *reason;
  switch (statusCode)
  {
  case 200: reason = "OK"; break;
  case 400: reason = "Bad Request"; break;
  case 404: reason = "Not Found"; break;
  case 408: reason = "Request Timeout"; break;
  case 409: reason = "Conflict"; break;
  case 500: reason = "Internal Server Error"; break;
  default: reason = "Unknown"; break;
  }

  client.print("HTTP/1.1 ");
  client.print(statusCode);
  client.print(" ");
  client.print(reason);
  client.println("\r\nContent-Type: application/json\r\nConnection: close\r\n");
  client.println(jsonMessage);
}

void handleGetVersionRoute(EthernetClient &client)
{
  JsonDocument doc;
  doc["status"] = "success";
  doc["version"] = FIRMWARE_VERSION;
  doc["board"] = BOARD_NAME;

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  sendJSONResponse(client, 200, jsonResponse);
}

void handleGetTempRoute(EthernetClient &client)
{
  JsonDocument doc;

  doc["status"] = "success";
  doc["temp_0_c"] = cachedTempC0;
  doc["temp_1_c"] = cachedTempC1;

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  sendJSONResponse(client, 200, jsonResponse);
}

void handleGetJammStateRoute(EthernetClient &client)
{
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

void handleNotifRGBControl(EthernetClient &client)
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, client);

  if (error)
  {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Invalid JSON or body missing\"}");
    return;
  }

  if (!doc["is_on"].is<bool>() || !doc["r"].is<int>() || !doc["g"].is<int>() || !doc["b"].is<int>() || !doc["numb"].is<byte>())
  {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Missing or invalid keys (is_on, r, g, b, numb required)\"}");
    return;
  }

  bool isOn = doc["is_on"];
  int r = doc["r"];
  int g = doc["g"];
  int b = doc["b"];
  byte numb = doc["numb"];

  // Boundary check for RGB ranges
  if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 || numb >= LED_RGB_NUM_PIXELS)
  {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"RGB values must be 0-255 and pixel index must be valid\"}");
    return;
  }

  if (isOn == false)
  {
    notifRGB.clear();
  }
  else
  {
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

void handleAnimateRGBControl(EthernetClient &client)
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, client);

  if (error)
  {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Invalid JSON or body missing\"}");
    return;
  }

  if (!doc["is_on"].is<bool>())
  {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Missing or invalid key: is_on (boolean required)\"}");
    return;
  }

  bool isOn = doc["is_on"];

  if (isOn == false)
  {
    animationRunning = false;
    notifRGB.clear();
    notifRGB.show();
  }
  else
  {
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

void handleGetFanRoute(EthernetClient &client)
{
  JsonDocument doc;
  doc["mode"] = getFanMode() == FanMode::Auto ? "auto" : "manual";
  doc["is_on"] = isFanOn();

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  sendJSONResponse(client, 200, jsonResponse);
}

void handlePostFanModeRoute(EthernetClient &client)
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, client);
  if (error || !doc["mode"].is<const char *>())
  {
    sendJSONResponse(client, 400, "{\"success\":false,\"message\":\"Mode must be auto or manual\"}");
    return;
  }

  if (doc["mode"] == "auto")
    setFanMode(FanMode::Auto);
  else if (doc["mode"] == "manual")
    setFanMode(FanMode::Manual);
  else
  {
    sendJSONResponse(client, 400, "{\"success\":false,\"message\":\"Mode must be auto or manual\"}");
    return;
  }

  sendJSONResponse(client, 200, "{\"success\":true,\"message\":\"Fan mode updated\"}");
}

void handlePostFanRoute(EthernetClient &client)
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, client);

  if (error)
  {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Invalid JSON or body missing\"}");
    return;
  }

  if (!doc["is_on"].is<bool>())
  {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Missing or invalid key: is_on (boolean required)\"}");
    return;
  }

  bool isOn = doc["is_on"];
  if (!setManualFan(isOn))
  {
    sendJSONResponse(client, 409, "{\"success\":false,\"message\":\"Fan control requires manual mode\"}");
    return;
  }
  sendJSONResponse(client, 200, "{\"success\": true,\"message\":\"Fan state updated\"}");
}

void handlePostJammersRoute(EthernetClient &client)
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, client);

  if (error)
  {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Invalid JSON or body missing\"}");
    return;
  }

  // Validate that required keys
  if (!doc["jamm_900"].is<bool>() || !doc["jamm_1500"].is<bool>() || !doc["jamm_2400"].is<bool>() || !doc["jamm_5200"].is<bool>() || !doc["jamm_5800"].is<bool>())
  {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Missing or invalid keys: jamm_900, jamm_1500, jamm_2400, jamm_5200, jamm_5800 (boolean required)\"}");
    return;
  }

  // Process the jammer commands
  bool is900on = doc["jamm_900"];
  bool is1500on = doc["jamm_1500"];
  bool is2400on = doc["jamm_2400"];
  bool is5200on = doc["jamm_5200"];
  bool is5800on = doc["jamm_5800"];

  digitalWrite(JAMM_900_PIN, is900on);
  digitalWrite(JAMM_1500_PIN, is1500on);
  digitalWrite(JAMM_2400_PIN, is2400on);
  digitalWrite(JAMM_5200_PIN, is5200on);
  digitalWrite(JAMM_5800_PIN, is5800on);
  updateFan();

  sendJSONResponse(client, 200, "{\"success\": true,\"message\":\"Jammers state updated\"}");
}

void handlePostJammerRoute(EthernetClient &client)
{
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, client);

  if (error)
  {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Invalid JSON or body missing\"}");
    return;
  }

  // Validate that required keys
  if (!doc["jamm_freq"].is<int>() || !doc["is_on"].is<bool>())
  {
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Missing a key\"}");
    return;
  }

  // Process the jammer command
  int jammerFreq = doc["jamm_freq"];
  bool isOn = doc["is_on"];
  uint32_t jammerPin;

  switch (jammerFreq)
  {
  case 900:
    jammerPin = JAMM_900_PIN;
    break;
  case 1500:
    jammerPin = JAMM_1500_PIN;
    break;
  case 2400:
    jammerPin = JAMM_2400_PIN;
    break;
  case 5200:
    jammerPin = JAMM_5200_PIN;
    break;
  case 5800:
    jammerPin = JAMM_5800_PIN;
    break;
  default:
    sendJSONResponse(client, 400, "{\"success\": false,\"message\":\"Invalid 'jamm' frequency\"}");
    return;
  }

  digitalWrite(jammerPin, isOn);
  updateFan();

  String response = "{\"success\": true,\"message\":\"Jammer frequency " + String(jammerFreq) + " is now " + (isOn ? "ON" : "OFF") + "\"}";
  sendJSONResponse(client, 200, response);
}

// --- Route: POST /ota ---
void handleOtaRoute(EthernetClient &client, long expectedSize)
{
  if (expectedSize <= 0 || expectedSize > 256000)
  {
    sendJSONResponse(client, 400, "{\"status\":\"error\",\"message\":\"Invalid size constraints\"}");
    return;
  }

  if (InternalStorage.open(expectedSize))
  {
    uint8_t buffer[512];
    long totalReceived = 0;
    unsigned long otaTimeout = millis();
    const unsigned long OTA_TIMEOUT = 60000UL; // 60 second timeout for OTA

    while (client.connected() || client.available())
    {
      IWatchdog.reload();

      // Timeout protection for OTA process
      if ((unsigned long)(millis() - otaTimeout) > OTA_TIMEOUT)
      {
        InternalStorage.close();
        sendJSONResponse(client, 408, "{\"status\":\"error\",\"message\":\"OTA timeout\"}");
        client.stop();
        return;
      }

      if (client.available())
      {
        int len = client.read(buffer, sizeof(buffer));
        if (len > 0)
        {
          for (int j = 0; j < len; j++)
          {
            InternalStorage.write(buffer[j]);
            totalReceived++;
            if (totalReceived > expectedSize)
            {
              InternalStorage.close();
              sendJSONResponse(client, 400, "{\"status\":\"error\",\"message\":\"Size mismatch\"}");
              client.stop();
              return;
            }
          }
          otaTimeout = millis(); // Reset timeout on data received
        }
      }
      if (totalReceived >= expectedSize)
      {
        break;
      }
    }
    InternalStorage.close();

    if (totalReceived == expectedSize)
    {
      sendJSONResponse(client, 200, "{\"status\":\"success\",\"message\":\"Flashing complete. Rebooting.\"}");
      client.stop();
      delay(500);
      InternalStorage.apply();
      // Should not reach here, but just in case
      NVIC_SystemReset();
    }
    else
    {
      sendJSONResponse(client, 400, "{\"status\":\"error\",\"message\":\"Size mismatch\"}");
    }
  }
  else
  {
    sendJSONResponse(client, 500, "{\"status\":\"error\",\"message\":\"Storage init failed\"}");
  }

  client.stop();
}

// Parse one incoming request and dispatch to the matching route handler
void handleHttpRequest()
{
  EthernetClient client = server.available();
  if (!client)
    return;

  String currentLine = "";
  String method = "";
  String route = "";
  long contentLength = 0;
  unsigned long timeoutStart = millis();
  bool headersParsed = false;

  while (client.connected())
  {
    IWatchdog.reload();

    // Overflow-safe timeout check (3 second timeout)
    if ((unsigned long)(millis() - timeoutStart) > 3000UL)
    {
      break;
    }

    if (client.available())
    {
      char c = client.read();

      if (c == '\n')
      {
        if (currentLine.length() == 0)
        {
          headersParsed = true;
          break;
        }

        // --- EXTRACT METHOD AND ROUTE ---
        // Example first line: "POST /led HTTP/1.1"
        if (currentLine.startsWith("POST ") || currentLine.startsWith("GET "))
        {
          int firstSpace = currentLine.indexOf(' ');
          int secondSpace = currentLine.indexOf(' ', firstSpace + 1);

          if (firstSpace > 0 && secondSpace > firstSpace)
          {
            method = currentLine.substring(0, firstSpace);
            route = currentLine.substring(firstSpace + 1, secondSpace);
          }
        }

        String lowercaseLine = currentLine;
        lowercaseLine.toLowerCase();

        if (lowercaseLine.startsWith("content-length:"))
        {
          int colonIdx = currentLine.indexOf(':');
          if (colonIdx > 0)
          {
            String sizeStr = currentLine.substring(colonIdx + 1);
            sizeStr.trim();
            contentLength = atol(sizeStr.c_str());
          }
        }

        currentLine = "";
      }
      else if (c != '\r')
      {
        // Prevent unbounded String growth - limit header line length
        if (currentLine.length() < 512)
        {
          currentLine += c;
        }
        else
        {
          sendJSONResponse(client, 400, "{\"status\":\"error\",\"message\":\"Header too long\"}");
          client.stop();
          return;
        }
      }
    }
  }

  // Validate that we got valid method and route
  if (!headersParsed || method.length() == 0 || route.length() == 0)
  {
    client.stop();
    return;
  }

  // --- STRATEGIC METHOD-BASED ROUTER BLOCK ---
  if (method == "GET" && route == "/temp")
  {
    handleGetTempRoute(client);
  }
  else if (method == "GET" && route == "/version")
  {
    handleGetVersionRoute(client);
  }
  else if (method == "POST" && route == "/fan")
  {
    handlePostFanRoute(client);
  }
  else if (method == "GET" && route == "/fan")
  {
    handleGetFanRoute(client);
  }
  else if (method == "POST" && route == "/fan/mode")
  {
    handlePostFanModeRoute(client);
  }
  else if (method == "POST" && route == "/ota")
  {
    handleOtaRoute(client, contentLength);
  }
  else if (method == "POST" && route == "/jammer")
  {
    handlePostJammerRoute(client);
  }
  else if (method == "POST" && route == "/jammers")
  {
    handlePostJammersRoute(client);
  }
  else if (method == "POST" && route == "/notifRGB")
  {
    handleNotifRGBControl(client);
  }
  // Animation disabled
  // else if (method == "POST" && route == "/animateRGB")
  // {
  //   handleAnimateRGBControl(client);
  // }
  else if (method == "GET" && route == "/jamm_state")
  {
    // Force a fresh sensor read before responding
    updateJammerSensors();
    handleGetJammStateRoute(client);
  }
  else
  {
    sendJSONResponse(client, 404, "{\"status\":\"error\",\"message\":\"Endpoint or Method unsupported\"}");
  }

  client.stop();
}
