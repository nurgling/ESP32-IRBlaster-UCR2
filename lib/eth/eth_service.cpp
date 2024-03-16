// Copyright by Alex Koessler


// Provides ETH Service that connects the Dock to a wired ethernet network.

#include <Arduino.h>
#include "eth_service.h"
#include <libconfig.h>

#include <wifi_service.h>
#include <mdns_service.h>

#include <esp_log.h>
#define WROOM 1
#define WROVER 2

#if BLASTER_ENABLE_ETH == true

#if (OLIMEX_ESP == WROOM)
#undef ETH_CLK_MODE
#undef ETH_PHY_POWER
#undef ETH_PHY_TYPE

#define ETH_PHY_TYPE        ETH_PHY_LAN8720
//#define ETH_PHY_ADDR         0
//#define ETH_PHY_MDC         23
//#define ETH_PHY_MDIO        18
#define ETH_PHY_POWER       12
#define ETH_CLK_MODE        ETH_CLOCK_GPIO17_OUT

#elif (OLIMEX_ESP == WROVER)
#undef ETH_CLK_MODE
#undef ETH_PHY_POWER
#undef ETH_PHY_TYPE

#define ETH_PHY_TYPE        ETH_PHY_LAN8720
//#define ETH_PHY_ADDR         0
//#define ETH_PHY_MDC         23
//#define ETH_PHY_MDIO        18
#define ETH_PHY_POWER       12
#define ETH_CLK_MODE        ETH_CLOCK_GPIO0_OUT

#warning WARNING: UNTESTED CONFIGURATION FOR WROFER MODULE
#error REMOVE THIS LINE IF YOU KNOW WHAT YOU ARE DOING

#else

#error UNSUPPORTED OLIMEX ESP VERSION

#endif


#endif

#include <ETH.h>

static const char * TAG = "eth";

const unsigned long thirtySecs = 30 * 1000UL;

void EthService::loop(){
    unsigned long currentMillis = millis();
}


static bool eth_connected = false;

boolean EthService::isActive()
{
    return(eth_connected);
}



// WARNING: EthEvent is called from a separate FreeRTOS task (thread)!
void EthEvent(WiFiEvent_t event)
{
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ESP_LOGI(TAG, "ETH Started");
      // The hostname must be set after the interface is started, but needs
      // to be set before DHCP, so set it from the event handler thread.
      ESP_LOGI(TAG, "Setting ETH Hostname: %s", Config::getInstance().getHostName().c_str());
      ETH.setHostname(Config::getInstance().getHostName().c_str());
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      ESP_LOGI(TAG, "ETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      ESP_LOGI(TAG, "ETH got IP: %s", ETH.localIP().toString().c_str());
      eth_connected = true;
      // We got a working Ethernet connection. Therefore, we will stop wifi...
      WifiService::getInstance().disconnect();
      // ... and restart mDNS
      MDNSService::getInstance().restartService();
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      ESP_LOGI(TAG, "ETH Disconnected");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      ESP_LOGI(TAG, "ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}


void EthService::connect(){

#if BLASTER_ENABLE_ETH == true
    Config &config = Config::getInstance();
    WiFi.onEvent(EthEvent);
    ETH.begin();
#endif

}

