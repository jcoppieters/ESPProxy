/*
 * ESP32 Proxy for Duotecno Cloud
 * 
 * This program implements a TCP proxy that:
 * 1. Connects to a cloud server and registers with a unique ID
 * 2. Maintains a pool of free connections ready for clients
 * 3. Forwards data between cloud clients and local device
 * 4. Automatically creates new free connections when needed
 * 
 * Hardware: ESP32 with Ethernet (WT32-ETH01, Olimex ESP32-POE, etc.)
 * 
 * Author: Johan Coppieters for Duotecno
 * Translated from TypeScript proxy.ts 
 * Date: November 2025

 */

 #ifndef ESPPROXY_H
#define ESPPROXY_H

#include <Arduino.h>
#include <ETH.h>
#include <WiFiClient.h>
#include "config.h"

// LED Configuration
// LED disabled - no LED connected to any GPIO pins
// Set ENABLE_LED to true to use it for packet indication
#define ENABLE_LED false          // Set to false - no safe GPIO available
#define LED_PIN 12                // Not used when ENABLE_LED is false
#define LED_BLINK_DURATION 200    // ms to keep LED on when packet received

// Configuration structure
struct ProxyConfig {
  char cloudServer[64];   // Cloud server address
  uint16_t cloudPort;     // Cloud server port
  char masterAddress[16]; // Local master IP address
  uint16_t masterPort;    // Local master port
  char uniqueId[64];      // Unique ID for the device
  bool debug;             // Debug mode
};

// Forward declaration
class ESPProxy;

// Connection direction for logging
enum ConnectionDirection {
  FROM_CLOUD,       // [CLOUD -> PROXY]
  TO_DEVICE,        // [PROXY -> DEVICE]
  TO_CLOUD,         // [PROXY -> CLOUD]
  DEVICE_TO_CLOUD,  // [DEVICE -> CLOUD]
  CLOUD_TO_DEVICE   // [CLOUD -> DEVICE]
};

// Connection context - manages one cloud-to-device connection pair
class Context {
public:
  Context(WiFiClient* cloudSocket, ESPProxy* proxy, int connectionId);
  ~Context();
  
  void loop();  // Must be called regularly to handle data transfer
  bool isActive() const { return cloudSocket != nullptr; }
  bool isFree() const { return cloudSocket != nullptr && deviceSocket == nullptr && cloudConnected; }
  
  // Getters for connection details
  int getConnectionId() const { return connectionId; }
  bool hasCloudSocket() const { return cloudSocket != nullptr; }
  bool hasDeviceSocket() const { return deviceSocket != nullptr; }
  bool isCloudConnected() const { return cloudConnected; }
  bool isDeviceConnected() const { return deviceConnected; }
  
  void cleanupSockets();
  void handleDataFromCloud();
  
private:
  ESPProxy* proxy;  // Reference to parent ESPProxy instance
  
  WiFiClient* cloudSocket;
  WiFiClient* deviceSocket;
  
  int connectionId;  // Unique ID for debugging
  
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
//////////////////////
// Main proxy class //
//////////////////////
//
// Actually our application (next to the config web server)
//
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
  int getActiveConnectionCount() const;  // Returns count of connections with device attached
  int getMaxConnections() const { return MAX_CONNECTIONS; }
  const ProxyConfig& getConfig() const { return config; }
  String getConnectionDetailsJSON() const;  // Returns JSON array with connection details
  
  // Statistics getters
  unsigned long getTotalBytesTransferred() const { return totalBytesTransferred; }
  unsigned long getTotalClientConnections() const { return totalClientConnections; }
  
  // Statistics updaters (called by Context)
  void addBytesTransferred(size_t bytes) { totalBytesTransferred += bytes; }
  void incrementClientConnections() { totalClientConnections++; }
  void removeConnection(Context* ctx);  // Called by Context when connection closes

  // Logging functions
  void logDebug(const char* msg);
  void logInfo(const char* msg);
  void logError(const char* msg);  
  void logData(ConnectionDirection direction, int len, const uint8_t* buffer, int connectionId);
  
  // Generic logging helper: logs [SOURCE] -> [DEST] messages
  // connectionId: if > 0, includes "(conn #X)" in message
  // extraStr: if not nullptr, appends this string to the message
  void logDirection(ConnectionDirection direction);
  void logMessage(ConnectionDirection direction, int connectionId, 
                  const char* message, const char* extraStr = nullptr);

  // Restart the ESP, but try to clean up first
  void cleanStart(bool restart = false);

private:
  ProxyConfig config;
  bool debug;
  
  Context* connections[MAX_CONNECTIONS];
  int connectionCount;  // Number of active connections in array
  int nextConnectionId; // Counter for generating unique connection IDs
  
  unsigned long lastConnectionCheck;
  
  // Statistics
  unsigned long totalBytesTransferred;
  unsigned long totalClientConnections;
  
  void checkConnections();
};

#endif // ESPPROXY_H
