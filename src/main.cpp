/*
 * Start ESP32 Proxy and configurator for Duotecno Cloud
 * Author: Johan Coppieters for Duotecno
 * Date: November 2025
 */

#include <ETH.h>
#include <WiFiClient.h>
#include "ESPProxy.h"
#include "WebConfig.h"
#include "config.h"  // Contains your configuration

// ESP32 ETH event flag
static bool eth_connected = false;

// ETH event handler
void onEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("[ETH] Started setup");
      // Set hostname
      ETH.setHostname(MDNS_HOSTNAME);
      break;

    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("[ETH] Connected");
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("[ETH] Got IP: ");
      Serial.println(ETH.localIP());

      Serial.print("[ETH] Subnet Mask: ");
      Serial.println(ETH.subnetMask());

      Serial.print("[ETH] MAC: ");
      Serial.println(ETH.macAddress());

      Serial.print("[ETH] ");
      if (ETH.fullDuplex()) Serial.print("FULL_DUPLEX, ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");

      Serial.print("[ETH] Gateway: ");
      Serial.println(ETH.gatewayIP());

      Serial.print("[ETH] DNS Server: ");
      Serial.println(ETH.dnsIP());

      eth_connected = true;
      break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;

    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;

    default:
      break;
  }
}

// Proxy instance
ESPProxy proxy;

#if ENABLE_WEB_CONFIG
// Web configuration instance
WebConfig* webConfig = nullptr;
#endif

void setup() {
  Serial.begin(115200);
  delay(500); // Small delay for serial to stabilize
  
#if ENABLE_LED
  // Initialize LED (only if enabled)
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
#endif

  Serial.println("\n============================");
  Serial.println("=== ESP32 Duotecno Proxy ===");
  Serial.print("=== Version: "); Serial.print(VERSION); Serial.println("    ===");
  Serial.println("============================\n");
  
  // Register ETH event handler
  WiFi.onEvent(onEvent);
    
  // Initialize Ethernet for Olimex ESP32-POE uses LAN8720 PHY
  // PHY_ADDR = 0, PHY_POWER = 12, PHY_MDC = 23, PHY_MDIO = 18, PHY_TYPE = ETH_PHY_LAN8720
  ETH.begin(0, 12, 23, 18, ETH_PHY_LAN8720, ETH_CLOCK_GPIO17_OUT);
  
#ifndef USE_DHCP
  // Configure static IP if not using DHCP
  IPAddress ip;
  ip.fromString(LOCAL_IP);
  IPAddress gateway;
  gateway.fromString(GATEWAY_IP);
  IPAddress subnet;
  subnet.fromString(SUBNET_MASK);
  IPAddress dns;
  dns.fromString(DNS_SERVER);
  
  ETH.config(ip, gateway, subnet, dns);
#endif
  
  // Wait for Ethernet connection
  if (!eth_connected) Serial.println("[ETH] Waiting for connection...");
  unsigned long startTime = millis();
  while (!eth_connected && (millis() - startTime < 10000)) {
    delay(500);
  }
  
  if (!eth_connected) {
    Serial.println("[ETH] Failed to connect to Ethernet!");
    return;
  }
  
  // Configure proxy
  ProxyConfig config;
  
#if ENABLE_WEB_CONFIG
  // Initialize web config (for loading/saving)
  webConfig = new WebConfig(&proxy);
  
  // Try to load config from NVRAM, fallback to config.h defaults
  String mdnsHostname;
  if (!webConfig->loadConfig(config, mdnsHostname)) {
    Serial.println("[CONFIG] Using default configuration from config.h");
    strncpy(config.cloudServer, CLOUD_SERVER, sizeof(config.cloudServer) - 1);
    config.cloudPort = CLOUD_PORT;
    strncpy(config.masterAddress, MASTER_ADDRESS, sizeof(config.masterAddress) - 1);
    config.masterPort = MASTER_PORT;
    strncpy(config.uniqueId, UNIQUE_ID, sizeof(config.uniqueId) - 1);
    config.debug = DEBUG_MODE;
    mdnsHostname = MDNS_HOSTNAME;
  }
#else
  // Load config from config.h
  strncpy(config.cloudServer, CLOUD_SERVER, sizeof(config.cloudServer) - 1);
  config.cloudPort = CLOUD_PORT;
  strncpy(config.masterAddress, MASTER_ADDRESS, sizeof(config.masterAddress) - 1);
  config.masterPort = MASTER_PORT;
  strncpy(config.uniqueId, UNIQUE_ID, sizeof(config.uniqueId) - 1);
  config.debug = DEBUG_MODE;
#endif
  
  // Start proxy
  if (proxy.begin(config)) {
    Serial.println("[INFO] ESP Proxy started successfully!");
  } else {
    Serial.println("[ERROR] ESP Proxy Failed to start!");
  }
  
#if ENABLE_WEB_CONFIG
  // Start web configuration interface
  if (webConfig->begin()) {
    Serial.println("[INFO] =======");
    Serial.println("[INFO] === Web configuration interface ready!");
      Serial.print("[INFO] === Access at: http://");
      Serial.print(webConfig->getMDNSHostname());
      Serial.print(".local");
      Serial.print(" - http://");
    Serial.println(ETH.localIP());
  } else {
    Serial.println("Failed to start web configuration interface!");
  }
#endif
  
  Serial.println("[INFO] =======");
  Serial.print("[INFO] === Publishing '"); 
    Serial.print(config.uniqueId);
    Serial.print("' to: ");
    Serial.print(config.cloudServer);
    Serial.print(":");
    Serial.println(config.cloudPort);
  Serial.print("[INFO] === Proxy is running on "); 
    Serial.print(config.masterAddress); 
    Serial.print(":"); 
    Serial.print(config.masterPort); 
    Serial.println("...");
  Serial.println("[INFO] =======");
}

void loop() {
  // Run the proxy main loop
  proxy.loop();
  
#if ENABLE_WEB_CONFIG
  // Handle web server requests
  if (webConfig) {
    webConfig->loop();
  }
#endif
  
  // Small delay to prevent watchdog issues
  delay(10);
}
