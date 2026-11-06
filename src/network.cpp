#include "network.h"
#include "utility/w5100.h"
#include <IWatchdog.h>
#include "jammer.h"
#include "led.h"

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x41, 0x00};
IPAddress ip(192, 168, 17, 150);
IPAddress gateway(192, 168, 17, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress myDns(192, 168, 10, 1);

EthernetServer server(HTTP_PORT);

unsigned long lastNetworkCheckTime = 0;
uint8_t networkFailureCount = 0;
bool networkFault = false;
static uint8_t noListenCount = 0;
static unsigned long jammerLinkDownStartTime = 0;
static bool jammerLinkDownTimerActive = false;

void initializeNetwork()
{
  DEBUG_PRINTLN("[NET] Initializing network...");

  // Hard Reset W5500
  pinMode(W5500_RST, OUTPUT);
  digitalWrite(W5500_RST, LOW);
  delay(100);
  digitalWrite(W5500_RST, HIGH);
  delay(300);

  // Reload watchdog during long initialization
  IWatchdog.reload();

  SPI.setMOSI(W5500_MOSI);
  SPI.setMISO(W5500_MISO);
  SPI.setSCLK(W5500_SCK);
  SPI.begin();

  Ethernet.init(W5500_CS);
  Ethernet.begin(mac, ip, myDns, gateway, subnet);

  // Give it time to stabilize
  delay(500);
  IWatchdog.reload();

  server.begin();

  DEBUG_PRINT("[NET] Server listening on: http://");
  DEBUG_PRINTLN(Ethernet.localIP());
}

void checkNetworkHealth()
{
  // Check link status using W5500 hardware
  // W5500 lost its config (brownout/reset/SPI glitch): the link can stay ON while the server is dead.
  // localIP() reads the chip's live IP register (hardwareStatus() is only cached from init).
  if (Ethernet.localIP() != ip)
  {
    DEBUG_PRINTLN("[NET] W5500 lost its configuration. Reinitializing network...");
    networkFault = true;
    initializeNetwork();
    networkFailureCount = 0;
    noListenCount = 0;
    return;
  }

  // Server socket stuck: no socket is listening even though server.available() keeps trying to re-open one
  // (e.g. all 8 sockets leaked in half-closed states). A single miss can be a request in progress.
  bool listening = false;
  for (uint8_t i = 0; i < MAX_SOCK_NUM; i++)
  {
    if (W5100.readSnSR(i) == SnSR::LISTEN)
    {
      listening = true;
      break;
    }
  }

  if (listening)
  {
    noListenCount = 0;
  }
  else if (++noListenCount >= 2)
  {
    DEBUG_PRINTLN("[NET] No listening socket. Reinitializing network...");
    networkFault = true;
    initializeNetwork();
    networkFailureCount = 0;
    noListenCount = 0;
    return;
  }

  networkFault = false;

  uint8_t linkStatus = Ethernet.linkStatus();

  if (linkStatus == LinkOFF)
  {
    networkFailureCount++;
    DEBUG_PRINT("[NET] Link DOWN detected. Failure count: ");
    DEBUG_PRINTLN(networkFailureCount);

    if (networkFailureCount >= MAX_NETWORK_FAILURES)
    {
      DEBUG_PRINTLN("[NET] Max failures reached. Reinitializing network...");
      initializeNetwork();
      networkFailureCount = 0;
    }
  }

  else if (linkStatus == LinkON)
  {
    // Link is good, reset failure counter
    if (networkFailureCount > 0)
    {
      DEBUG_PRINTLN("[NET] Link restored.");
      networkFailureCount = 0;
    }
  }
}

void enforceJammerLinkSafety()
{
  uint8_t linkStatus = Ethernet.linkStatus();

  if (linkStatus == LinkOFF)
  {
    if (!jammerLinkDownTimerActive)
    {
      jammerLinkDownStartTime = millis();
      jammerLinkDownTimerActive = true;
    }
    else if ((unsigned long)(millis() - jammerLinkDownStartTime) >= JAMMER_LINK_DOWN_SHUTOFF_DELAY)
    {
      turnOffAllJammers();
    }
  }
  else if (linkStatus == LinkON)
  {
    jammerLinkDownTimerActive = false;
  }
}
