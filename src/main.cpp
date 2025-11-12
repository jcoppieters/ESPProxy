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
      Serial.println("[ETH] ETH-Disconnected");
      eth_connected = false;
      break;

    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("[ETH] ETH-Stopped");
      eth_connected = false;
      break;

    default:
      break;
  }
}

// Proxy instance
ESPProxy proxy;

// Web configuration instance
WebConfig* webConfig = nullptr;

void setup() {
  Serial.begin(115200);
  delay(500); // Small delay for serial to stabilize
  
  // Reduce unnecessary error logging
  esp_log_level_set("WebServer", ESP_LOG_WARN);    // Only show warnings and errors
  
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
  Serial.println("[ETH] Initializing Ethernet hardware...");
  ETH.begin(0, 12, 23, 18, ETH_PHY_LAN8720, ETH_CLOCK_GPIO17_OUT);
  
  // Give PHY time to stabilize after power-up
  delay(100);
  
  // Load configuration (from NVRAM or defaults from config.h)
  ProxyConfig config;
  webConfig = new WebConfig(&proxy);
  webConfig->loadConfig(config);
  
  // Configure static IP if not using DHCP
  if (!config.useDHCP && strlen(config.staticIP) > 0) {
    Serial.println("[ETH] Configuring static IP...");
    IPAddress ip;
    ip.fromString(config.staticIP);
    IPAddress gateway;
    gateway.fromString(config.gateway);
    IPAddress subnet;
    subnet.fromString(config.subnet);
    IPAddress dns;
    dns.fromString(config.dns);
    
    if (!ETH.config(ip, gateway, subnet, dns)) {
      Serial.println("[ETH] Failed to configure static IP!");
    }
  } else {
    Serial.println("[ETH] Using DHCP for IP configuration...");
  }
  
  // Wait for Ethernet connection (GOT_IP event) if not already connected
  if (!eth_connected) {
    Serial.println("[ETH] Waiting for IP address...");
    unsigned long startTime = millis();
    while (!eth_connected && (millis() - startTime < 15000)) {  // Increased timeout to 15 seconds
      delay(500);
      Serial.print(".");
    }
    Serial.println();
  }
  
  if (!eth_connected) {
    Serial.println("[ETH] Failed to connect to Ethernet!");
    return;
  }
  
  // Config was already loaded earlier for network configuration
  
  // Start proxy
  if (proxy.begin(config)) {
    proxy.logInfo("ESP Proxy started successfully!");
  } else {
    proxy.logError("ESP Proxy Failed to start!");
  }

    // Start web configuration interface
  if (webConfig->begin()) {
    proxy.logInfo("=== Web configuration interface ready!");
    Serial.print("[INFO] === Access at: http://");
      Serial.print(webConfig->getMDNSHostname());
      Serial.print(".local");
      Serial.print(" - http://");
      Serial.println(ETH.localIP());
  } else {
    proxy.logError("Failed to start web configuration interface!");
  }

  Serial.print("[INFO] === Published '"); 
    Serial.print(config.uniqueId);
    Serial.print("' to: ");
    Serial.print(config.cloudServer);
    Serial.print(":");
    Serial.println(config.cloudPort);
  Serial.print("[INFO] === Proxy is running on "); 
    Serial.print(config.masterAddress); 
    Serial.print(":"); 
    Serial.print(config.masterPort); 
}

void loop() {
  // Run the proxy main loop
  proxy.loop();
  
  // Handle web server requests
  if (webConfig) {
    webConfig->loop();
  }
  
  // Small delay to prevent watchdog issues
  delay(10);
}
