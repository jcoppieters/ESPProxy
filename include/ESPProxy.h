#ifndef ESPPROXY_H
#define ESPPROXY_H

#include <Arduino.h>
#include <ETH.h>
#include <WiFiClient.h>
#include "config.h"

// LED Configuration
// LED disabled - GPIO pins conflict with Ethernet
// Set ENABLE_LED to true to use it for packet indication
#define ENABLE_LED false   // Set to false - no safe GPIO available
#define LED_PIN 12        // Not used when ENABLE_LED is false
#define LED_BLINK_DURATION 200  // ms to keep LED on when packet received

// Configuration structure
struct ProxyConfig {
  char cloudServer[64];   // Cloud server address
  uint16_t cloudPort;     // Cloud server port
  char masterAddress[16]; // Local master IP address
  uint16_t masterPort;    // Local master port
  char uniqueId[64];      // Unique ID for the device
  bool debug;             // Debug mode
};

// Connection context - manages one cloud-to-device connection pair
class Context {
public:
  Context(WiFiClient* cloudSocket, const char* master, uint16_t masterPort, 
          const char* server, uint16_t serverPort, const char* uniqueId, int id, bool* debugPtr);
  ~Context();
  
  void loop();  // Must be called regularly to handle data transfer
  bool isActive() const { return cloudSocket != nullptr; }
  bool isFree() const { return cloudSocket != nullptr && deviceSocket == nullptr && cloudConnected; }
  
  void cleanupSockets();
  void handleDataFromCloud();
  
private:
  WiFiClient* cloudSocket;
  WiFiClient* deviceSocket;
  
  int connectionId;  // Unique ID for debugging
  
  char master[16];
  uint16_t masterPort;
  
  char server[64];
  uint16_t serverPort;
  
  char uniqueId[64];
  
  bool* debug;  // Pointer to debug flag from ESPProxy
  
  bool cloudConnected;
  bool deviceConnected;
  
  unsigned long ledOnTime;  // Time when LED was turned on
  bool ledState;            // Current LED state
  
  void setupCloudSocket();
  void makeDeviceConnection(uint8_t* data, size_t len);
  void setUpDeviceSocket();
  
  bool isHeartbeatRequest(const char* data, size_t len);
  bool isConnectionResponse(const char* data, size_t len);
  
  void blinkLED();     // Turn on LED briefly
  void updateLED();    // Update LED state (turn off after blink duration)
};

// Main proxy class
class ESPProxy {
public:
  ESPProxy();
  ~ESPProxy();
  
  bool begin(const ProxyConfig& config);
  void loop();  // Must be called regularly in Arduino loop()
  
  void setDebug(bool enabled) { debug = enabled; }
  void makeNewCloudConnection(int retryCount = 1);  // Public: can be called to create new connection
  bool hasFreeConnection();  // Public: check if we have a free connection available
  
  // Status getters for web interface
  int getConnectionCount() const { return connectionCount; }
  int getFreeConnectionCount() const;
  int getMaxConnections() const { return MAX_CONNECTIONS; }
  const ProxyConfig& getConfig() const { return config; }
  
private:
  ProxyConfig config;
  bool debug;
  
  Context* connections[MAX_CONNECTIONS];
  int connectionCount;
  
  unsigned long lastConnectionCheck;
  
  void checkConnections();
  void cleanStart(bool restart = false);
  void removeConnection(Context* ctx);
  
  // Logging functions
  void logDebug(const char* msg);
  void logInfo(const char* msg);
  void logWarning(const char* msg);
  void logError(const char* msg);
};

#endif // ESPPROXY_H
