#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include "config.h"

// Network configuration
// Mark change IP for different devices
extern byte mac[];
extern IPAddress ip;
extern IPAddress gateway;
extern IPAddress subnet;
extern IPAddress myDns;

extern EthernetServer server;

// Network health monitoring
extern unsigned long lastNetworkCheckTime;
extern uint8_t networkFailureCount;
extern bool networkFault; // true while W5500/server is detected as broken (LED turns red)

// Initialize W5500 and start the HTTP server (with hard reset)
void initializeNetwork();

// Check link status and recover if needed (all-red LED on failure)
void checkNetworkHealth();

// Turn off all jammer outputs if the Ethernet link stays down past its grace period.
void enforceJammerLinkSafety();

#endif // NETWORK_H
