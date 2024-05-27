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

static bool eth_active = false;

void EthService::loop(){
    unsigned long currentMillis = millis();
}

boolean EthService::isActive(){
    return(eth_active);
}

boolean EthService::isConnected(){
  if(BLASTER_ENABLE_ETH == true){
    return(ETH.linkUp());
  }
  return(false);
}

String EthService::getMAC(){
  if(BLASTER_ENABLE_ETH == true){
    return ETH.macAddress();
  }
  return("");
}

IPAddress EthService::getIP(){
  if(BLASTER_ENABLE_ETH == true){
    return ETH.localIP();
  }
  return(IPAddress());
}
IPAddress EthService::getDNS(){
  if(BLASTER_ENABLE_ETH == true){
    return ETH.dnsIP();
  }
  return(IPAddress());
}
IPAddress EthService::getGateway(){
  if(BLASTER_ENABLE_ETH == true){
    return ETH.gatewayIP();
  }
  return(IPAddress());
}
String EthService::getConnectionSpeed(){
  if(!isConnected()){
    return "Ethernet not connected";
  }
  uint speed = ETH.linkSpeed();
  bool fdx = ETH.fullDuplex();
  String ret = String(speed);
  ret = ret + " Mbit ";
  if(fdx){
    ret = ret + "FDX";
  } else {
    ret = ret + "HDX";
  }
  return(ret);
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
      eth_active = true;
      // We got a working Ethernet connection. Therefore, we will stop wifi...
      WifiService::getInstance().disconnect();
      // ... and restart mDNS
      MDNSService::getInstance().restartService();
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      ESP_LOGI(TAG, "ETH Disconnected");
      eth_active = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      ESP_LOGI(TAG, "ETH Stopped");
      eth_active = false;
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

