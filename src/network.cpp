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

// =====================================================
// W5500 TIERED RECOVERY HELPERS
// =====================================================

// VERSIONR (0x0039) always reads 0x04 on a W5500. A wrong value means the
// chip is not responding correctly on the SPI bus — skip straight to a hard
// reset because lighter recoveries need a working SPI.
static bool isW5500Responsive()
{
  return (W5100.read(0x0039) == 0x04);
}

// Reset only the W5500's internal PHY (PHYCFGR at 0x002E, bit 7).
// This forces a link-drop + full re-auto-negotiation — the programmatic
// equivalent of physically unplugging and replugging the Ethernet cable.
static void resetW5500Phy()
{
  uint8_t phycfgr = W5100.read(0x002E);
  W5100.write(0x002E, phycfgr | 0x80); // Bit 7 auto-clears when done

  // Wait up to ~3.5 s for 100BASE-TX auto-negotiation to complete.
  unsigned long start = millis();
  while ((unsigned long)(millis() - start) < 3500UL)
  {
    IWatchdog.reload();
    if (Ethernet.linkStatus() == LinkON)
      break;
    delay(100);
  }
}

// Soft reset of the W5500 digital core (MR at 0x0000, bit 7).
// Clears all registers/sockets but does NOT reset the PHY. The Ethernet
// library and the HTTP server must be reinitialized afterwards.
static void softResetW5500()
{
  W5100.write(0x0000, 0x80); // Bit 7 auto-clears when done

  unsigned long start = millis();
  while ((unsigned long)(millis() - start) < 100UL)
  {
    if ((W5100.read(0x0000) & 0x80) == 0)
      break;
    delay(1);
  }

  Ethernet.init(W5500_CS);
  Ethernet.begin(mac, ip, myDns, gateway, subnet);
  delay(500);
  IWatchdog.reload();
  server.begin();
}

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
  // --- Tier 0: SPI responsiveness ---
  // If the W5500 cannot answer on the SPI bus (VERSIONR corrupt), skip
  // straight to a hard reset — lighter recoveries need a working SPI.
  if (!isW5500Responsive())
  {
    networkFault = true;
    initializeNetwork();
    networkFailureCount = 0;
    noListenCount = 0;
    return;
  }

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

    // Tiered recovery, escalating with each consecutive link-down check.
    switch (networkFailureCount)
    {
    case 2:
      // Tier 1: PHY reset only — quick attempt. The jammer may still be
      // transmitting, so the PHY might get re-jammed immediately.
      resetW5500Phy();
      server.begin();
      break;

    case 4:
      // Tier 1b: turn the jammers off first so the PHY can renegotiate
      // with clean spectrum, then PHY reset.
      turnOffAllJammers();
      resetW5500Phy();
      server.begin();
      break;

    case 7:
      // Tier 2: soft reset of the W5500 digital core (sockets/registers).
      softResetW5500();
      break;

    default:
      // Tier 3: full hard reset via the RST pin as the last resort.
      if (networkFailureCount >= MAX_NETWORK_FAILURES)
      {
        initializeNetwork();
        networkFailureCount = 0;
      }
      break;
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
