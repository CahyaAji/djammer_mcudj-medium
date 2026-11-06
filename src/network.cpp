#include "network.h"
#include <IWatchdog.h>
#include "jammer.h"
#include "led.h"

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x41, 0x00};
IPAddress ip(192, 168, 17, 150);
IPAddress gateway(192, 168, 10, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress myDns(192, 168, 10, 1);

EthernetServer server(HTTP_PORT);

unsigned long lastNetworkCheckTime = 0;
uint8_t networkFailureCount = 0;
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
  uint8_t linkStatus = Ethernet.linkStatus();

  if (linkStatus == LinkOFF)
  {
    networkFailureCount++;
    DEBUG_PRINT("[NET] Link DOWN detected. Failure count: ");
    DEBUG_PRINTLN(networkFailureCount);

    for (int i = 0; i < LED_RGB_NUM_PIXELS; i++)
    {
      notifRGB.setPixelColor(i, notifRGB.Color(200, 0, 0));
    }
    notifRGB.show();

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

    for (int i = 0; i < LED_RGB_NUM_PIXELS; i++)
    {
      notifRGB.setPixelColor(i, notifRGB.Color(0, 0, 200));
    }
    notifRGB.show();
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
