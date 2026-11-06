#ifndef HANDLERS_H
#define HANDLERS_H

#include <Arduino.h>
#include <Ethernet.h>

// Send a JSON HTTP response with the given status code
void sendJSONResponse(EthernetClient &client, int statusCode, const String &jsonMessage);

// Parse one incoming HTTP request and route it to the right handler
void handleHttpRequest();

// --- Route handlers ---
void handleGetVersionRoute(EthernetClient &client);
void handleGetTempRoute(EthernetClient &client);
void handleGetJammStateRoute(EthernetClient &client);
void handleNotifRGBControl(EthernetClient &client);
void handleAnimateRGBControl(EthernetClient &client);
void handlePostFanRoute(EthernetClient &client);
void handlePostJammersRoute(EthernetClient &client);
void handlePostJammerRoute(EthernetClient &client);
void handleOtaRoute(EthernetClient &client, long expectedSize);

#endif // HANDLERS_H
