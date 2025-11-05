/*
 * ESP32 Proxy for Duotecno Cloud
 * 
 * This sketch implements a TCP proxy that:
 * 1. Connects to a cloud server and registers with a unique ID
 * 2. Maintains a pool of free connections ready for clients
 * 3. Forwards data between cloud clients and local device
 * 4. Automatically creates new free connections when needed
 * 
 * Hardware: ESP32 with Ethernet (WT32-ETH01, Olimex ESP32-POE, etc.)
 * 
 * Author: Translated from TypeScript proxy.ts
 * Date: November 2025
 */

#include <ETH.h>
#include <WiFiClient.h>
#include "ESPProxy.h"
#include "config.h"  // Contains your configuration

// ESP32 ETH event flag
static bool eth_connected = false;

// ETH event handler
void onEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      // Set hostname
      ETH.setHostname("duotecno-esp32-proxy");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("ETH Got IP: ");
      Serial.println(ETH.localIP());
      Serial.print("ETH MAC: ");
      Serial.println(ETH.macAddress());
      if (ETH.fullDuplex()) {
        Serial.print("FULL_DUPLEX, ");
      }
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
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

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Wait for serial or timeout after 3s
  
#if ENABLE_LED
  // Initialize LED (only if enabled)
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
#endif
  
  Serial.println("\n\n=================================");
  Serial.println("ESP32 Duotecno Proxy");
  Serial.println("=================================\n");
  
  // Register ETH event handler
  WiFi.onEvent(onEvent);
  
  // Initialize Ethernet for Olimex ESP32-POE
  Serial.println("Initializing Ethernet...");
  
  // Olimex ESP32-POE uses LAN8720 PHY
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
  Serial.println("Waiting for Ethernet connection...");
  unsigned long startTime = millis();
  while (!eth_connected && (millis() - startTime < 10000)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  if (!eth_connected) {
    Serial.println("Failed to connect to Ethernet!");
    return;
  }
  
  // Print network configuration
  Serial.print("IP Address: ");
  Serial.println(ETH.localIP());
  Serial.print("Subnet Mask: ");
  Serial.println(ETH.subnetMask());
  Serial.print("Gateway: ");
  Serial.println(ETH.gatewayIP());
  Serial.print("DNS Server: ");
  Serial.println(ETH.dnsIP());
  Serial.println();
  
  // Configure proxy
  ProxyConfig config;
  strncpy(config.cloudServer, CLOUD_SERVER, sizeof(config.cloudServer) - 1);
  config.cloudPort = CLOUD_PORT;
  strncpy(config.masterAddress, MASTER_ADDRESS, sizeof(config.masterAddress) - 1);
  config.masterPort = MASTER_PORT;
  strncpy(config.uniqueId, UNIQUE_ID, sizeof(config.uniqueId) - 1);
  config.debug = DEBUG_MODE;
  
  // Start proxy
  if (proxy.begin(config)) {
    Serial.println("Proxy started successfully!");
  } else {
    Serial.println("Failed to start proxy!");
  }
  
  Serial.println("\n=================================");
  Serial.print("Publishing '"); 
    Serial.print(UNIQUE_ID);
    Serial.print("' to: ");
    Serial.print(CLOUD_SERVER);
    Serial.print(":");
    Serial.println(CLOUD_PORT);
  Serial.print("Proxy for "); 
    Serial.print(MASTER_ADDRESS); 
    Serial.print(":"); 
    Serial.print(MASTER_PORT); 
    Serial.println(" is running...");
  Serial.println("=================================\n");
}

void loop() {
  // Run the proxy main loop
  proxy.loop();
  
  // Small delay to prevent watchdog issues
  delay(10);
}
