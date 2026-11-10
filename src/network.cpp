#include "network.h"
#include "utility/w5100.h"
#include <IWatchdog.h>
#include "jammer.h"
#include "led.h"

byte mac[] = MAC_ADDRESS;
IPAddress ip(STATIC_IP);
IPAddress gateway(GATEWAY_IP);
IPAddress subnet(SUBNET_MASK);
IPAddress myDns(DNS_IP);

EthernetServer server(HTTP_PORT);

unsigned long lastNetworkCheckTime = 0;
uint8_t networkFailureCount = 0;
bool networkFault = false;
static uint8_t noListenCount = 0;
static unsigned long jammerLinkDownStartTime = 0;
static bool jammerLinkDownTimerActive = false;

void initializeNetwork()
{
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
}

void checkNetworkHealth()
{
  // Check link status using W5500 hardware
  // W5500 lost its config (brownout/reset/SPI glitch): the link can stay ON while the server is dead.
  // localIP() reads the chip's live IP register (hardwareStatus() is only cached from init).
  if (Ethernet.localIP() != ip)
  {
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
    if (networkFailureCount >= MAX_NETWORK_FAILURES)
    {
      initializeNetwork();
      networkFailureCount = 0;
    }
  }

  else if (linkStatus == LinkON)
  {
    // Link is good, reset failure counter
    if (networkFailureCount > 0)
    {
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
