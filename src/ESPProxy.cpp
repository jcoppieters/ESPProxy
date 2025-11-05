#include "ESPProxy.h"
#include "config.h"

// Global reference for connection removal callback
ESPProxy* g_proxyInstance = nullptr;

///////////////
// Context Implementation
///////////////

Context::Context(WiFiClient* cs, const char* m, uint16_t mp, 
                 const char* s, uint16_t sp, const char* uid, int id) {
  this->cloudSocket = cs;
  this->deviceSocket = nullptr;
  this->connectionId = id;
  
  strncpy(this->master, m, sizeof(this->master) - 1);
  this->master[sizeof(this->master) - 1] = '\0';
  this->masterPort = mp;
  
  strncpy(this->server, s, sizeof(this->server) - 1);
  this->server[sizeof(this->server) - 1] = '\0';
  this->serverPort = sp;
  
  strncpy(this->uniqueId, uid, sizeof(this->uniqueId) - 1);
  this->uniqueId[sizeof(this->uniqueId) - 1] = '\0';
  
  this->cloudConnected = (this->cloudSocket && this->cloudSocket->connected());
  this->deviceConnected = false;
  
  this->ledOnTime = 0;
  this->ledState = false;
}

Context::~Context() {
  this->cleanupSockets();
}

void Context::cleanupSockets() {
  if (this->deviceSocket) {
    if (this->deviceSocket->connected()) {
      this->deviceSocket->stop();
    }
    delete this->deviceSocket;
    this->deviceSocket = nullptr;
    this->deviceConnected = false;
  }
  
  if (this->cloudSocket) {
    if (this->cloudSocket->connected()) {
      this->cloudSocket->stop();
    }
    delete this->cloudSocket;
    this->cloudSocket = nullptr;
    this->cloudConnected = false;
  }
}

void Context::loop() {
  // Update LED state
  this->updateLED();
  
  // Check cloud socket status
  if (this->cloudSocket) {
    if (!this->cloudSocket->connected()) {
      Serial.println("[CLOUD] Connection closed");
      this->cleanupSockets();
      return;
    }
    
    // Handle incoming data from cloud
    if (this->cloudSocket->available() > 0) {
      this->handleDataFromCloud();
    }
  }
  
  // Handle data from device to cloud
  if (this->deviceSocket && this->deviceConnected) {
    if (!this->deviceSocket->connected()) {
      Serial.println("[DEVICE] Connection closed");
      if (this->deviceSocket) {
        this->deviceSocket->stop();
        delete this->deviceSocket;
        this->deviceSocket = nullptr;
        this->deviceConnected = false;
      }
      // Don't cleanup cloud socket, let connection checker handle it
      return;
    }
    
    if (this->deviceSocket->available() > 0) {
      uint8_t buffer[512];
      int len = this->deviceSocket->read(buffer, sizeof(buffer));
      if (len > 0) {
        this->blinkLED();  // Blink LED when forwarding device data to cloud
#if ENABLE_DATA_LOGGING
        Serial.print("[DEVICE] -> [CLOUD] (conn #");
        Serial.print(this->connectionId);
        Serial.print(") Forwarding ");
        Serial.print(len);
        Serial.print(" bytes: ");
        for (int i = 0; i < len; i++) {
          if (buffer[i] != 0) Serial.write(buffer[i]);
        }
#endif
        
        if (this->cloudSocket && this->cloudSocket->connected()) {
          this->cloudSocket->write(buffer, len);
        }
      }
    }
  }
}

void Context::handleDataFromCloud() {
  uint8_t buffer[512];
  int len = this->cloudSocket->read(buffer, sizeof(buffer));
  
  if (len <= 0) return;
  
  this->blinkLED();  // Blink LED when receiving data from cloud
  
  // Null-terminate for string operations (be careful with binary data)
  char strBuffer[513];
  memcpy(strBuffer, buffer, len);
  strBuffer[len] = '\0';
  
  // Only log non-data messages (heartbeats, connection responses)
  // Data forwarding will be logged separately
  
  if (!this->deviceSocket || !this->deviceConnected) {
    // No device connection yet - check what kind of message this is
    
    if (this->isHeartbeatRequest(strBuffer, len)) {
      Serial.print("[CLOUD] -> [PROXY] (conn #");
      Serial.print(this->connectionId);
      Serial.println(") Heartbeat request, responding...");
      this->cloudSocket->write("[72,3]");
      return;
    }
    
    if (this->isConnectionResponse(strBuffer, len)) {
      Serial.print("[CLOUD] -> [PROXY] (conn #");
      Serial.print(this->connectionId);
      Serial.print(") Connection response: ");
      Serial.println(strBuffer);
      return;
    }
    
    // Real data - a new client wants to connect
    Serial.print("[PROXY] -> [CLOUD] (conn #");
    Serial.print(this->connectionId);
    Serial.println(") New client connection detected");
    
    // Connect to the device and forward this initial data
    this->makeDeviceConnection(buffer, len);
    
    // This connection is now busy with a device, so we always need a new free connection
    if (g_proxyInstance && this->deviceConnected) {
      Serial.println("[PROXY] Connection now has device attached - creating new free connection...");
      g_proxyInstance->makeNewCloudConnection();
    }
  } else if (this->deviceSocket && this->deviceConnected) {
    // Forward data to device
    #if ENABLE_DATA_LOGGING
      Serial.print("[CLOUD] -> [DEVICE] (conn #");
      Serial.print(this->connectionId);
      Serial.print(") Forwarding ");
      Serial.print(len);
      Serial.print(" bytes: ");
      for (int i = 0; i < len; i++) {
        if (buffer[i] != 0) Serial.write(buffer[i]);
      }
    #endif
    this->deviceSocket->write(buffer, len);
  }
}

void Context::makeDeviceConnection(uint8_t* data, size_t len) {
  Serial.print("[PROXY] -> [DEVICE] Connecting to device at ");
  Serial.print(this->master);
  Serial.print(":");
  Serial.println(this->masterPort);
  
  if (!this->deviceSocket) {
    this->deviceSocket = new WiFiClient();
  }
  
  // Parse IP address
  IPAddress deviceIP;
  if (deviceIP.fromString(this->master)) {
    if (this->deviceSocket->connect(deviceIP, this->masterPort)) {
      Serial.println("[PROXY] -> [DEVICE] Connected to device");
      this->deviceConnected = true;
      
      #if ENABLE_DATA_LOGGING
        // Send initial data
        Serial.print("[PROXY] -> [DEVICE] Sending initial ");
        Serial.print(len);
        Serial.print(" bytes: ");
        for (int i = 0; i < len; i++) {
          if (data[i] != 0) Serial.write(data[i]);
        }
      #endif
      this->deviceSocket->write(data, len);

    } else {
      Serial.println("[PROXY] -> [DEVICE] Failed to connect to device");
      delete this->deviceSocket;
      this->deviceSocket = nullptr;
      this->deviceConnected = false;
    }
  } else {
    Serial.println("[PROXY] -> [DEVICE] Invalid device IP address");
    delete this->deviceSocket;
    this->deviceSocket = nullptr;
    this->deviceConnected = false;
  }
}

bool Context::isHeartbeatRequest(const char* data, size_t len) {
  // Check for [215,3]
  if (len >= 7) {
    return (strncmp(data, "[215,3]", 7) == 0);
  }
  return false;
}

bool Context::isConnectionResponse(const char* data, size_t len) {
  if (len >= 3) {
    return (strncmp(data, "[OK", 3) == 0) || (strncmp(data, "[ERROR", 6) == 0);
  }
  return false;
}

void Context::blinkLED() {
  #if ENABLE_LED
    digitalWrite(LED_PIN, HIGH);
    this->ledState = true;
    this->ledOnTime = millis();
    Serial.println("[LED] RED ON");
  #endif
}

void Context::updateLED() {
  #if ENABLE_LED
    if (this->ledState && (millis() - this->ledOnTime >= LED_BLINK_DURATION)) {
      digitalWrite(LED_PIN, LOW);
      this->ledState = false;
      Serial.println("[LED] RED OFF");
    }
  #endif
}

///////////////
// ESPProxy Implementation
///////////////

ESPProxy::ESPProxy() {
  this->debug = false;
  this->connectionCount = 0;
  this->lastConnectionCheck = 0;
  
  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    this->connections[i] = nullptr;
  }
  
  g_proxyInstance = this;
}

ESPProxy::~ESPProxy() {
  this->cleanStart(false);
  g_proxyInstance = nullptr;
}

bool ESPProxy::begin(const ProxyConfig& cfg) {
  this->config = cfg;
  this->debug = cfg.debug;
  
  this->logInfo("=== ESP Proxy Starting ===");
  Serial.print("Cloud Server: ");
  Serial.print(this->config.cloudServer);
  Serial.print(":");
  Serial.println(this->config.cloudPort);
  Serial.print("Master Device: ");
  Serial.print(this->config.masterAddress);
  Serial.print(":");
  Serial.println(this->config.masterPort);
  Serial.print("Unique ID: ");
  Serial.println(this->config.uniqueId);
  
  if (strlen(this->config.uniqueId) == 0) {
    this->logError("No unique ID configured - cannot start proxy");
    return false;
  }
  
  // Create initial free connection
  this->makeNewCloudConnection();
  
  this->lastConnectionCheck = millis();
  
  return true;
}

void ESPProxy::loop() {
  // Process all existing connections
  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (this->connections[i] && this->connections[i]->isActive()) {
      this->connections[i]->loop();
    }
  }
  
  // Check if we need a new free connection
  unsigned long now = millis();
  if (now - this->lastConnectionCheck >= CONNECTION_CHECK_INTERVAL) {
    this->lastConnectionCheck = now;
    this->checkConnections();
  }
  
  // ESP32 ETH maintains connection automatically - no need for maintain()
}

void ESPProxy::makeNewCloudConnection(int retryCount) {
  if (this->connectionCount >= MAX_CONNECTIONS) {
    this->logWarning("Maximum connections reached, cannot create new connection");
    return;
  }
  
  this->logInfo("Creating new cloud connection...");
  Serial.print("Attempt ");
  Serial.print(retryCount);
  Serial.print(" to connect to ");
  Serial.print(this->config.cloudServer);
  Serial.print(":");
  Serial.println(this->config.cloudPort);
  
  WiFiClient* cloudSocket = new WiFiClient();
  
  // Resolve hostname or parse IP
  IPAddress serverIP;
  if (!serverIP.fromString(this->config.cloudServer)) {
    // Try DNS resolution - ESP32 uses WiFi class for DNS
    if (WiFi.hostByName(this->config.cloudServer, serverIP) != 1) {
      this->logError("Failed to resolve cloud server hostname");
      delete cloudSocket;
      
      if (retryCount < 3) {
        delay(retryCount * retryCount * 1000); // Exponential backoff
        this->makeNewCloudConnection(retryCount + 1);
      }
      return;
    }
  }
  
  // Connect to cloud server
  if (cloudSocket->connect(serverIP, this->config.cloudPort)) {
    Serial.print("[PROXY] -> [CLOUD] Connected to cloud at ");
    Serial.print(this->config.cloudServer);
    Serial.print(":");
    Serial.println(this->config.cloudPort);
    
    // Send unique ID
    cloudSocket->print("[");
    cloudSocket->print(this->config.uniqueId);
    cloudSocket->print("]");
    
    Serial.print("[PROXY] -> [CLOUD] Sent unique ID: ");
    Serial.println(this->config.uniqueId);
    
    // Create context and add to pool
    Context* ctx = new Context(cloudSocket, this->config.masterAddress, this->config.masterPort,
                               this->config.cloudServer, this->config.cloudPort, this->config.uniqueId, this->connectionCount + 1);
    
    // Find empty slot
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
      if (this->connections[i] == nullptr) {
        this->connections[i] = ctx;
        this->connectionCount++;
        Serial.print("[PROXY] -> [CLOUD] New free connection #");
        Serial.println(this->connectionCount);
        break;
      }
    }
    
  } else {
    this->logError("Failed to connect to cloud server");
    delete cloudSocket;
    
    if (retryCount < 3) {
      delay(retryCount * retryCount * 1000); // Exponential backoff
      this->makeNewCloudConnection(retryCount + 1);
    }
  }
}

void ESPProxy::checkConnections() {
  this->logDebug("Checking for free connections...");
  
  // Remove inactive connections
  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (this->connections[i] && !this->connections[i]->isActive()) {
      delete this->connections[i];
      this->connections[i] = nullptr;
      this->connectionCount--;
    }
  }
  
  // Check if we have at least one free connection
  if (this->hasFreeConnection()) {
    this->logDebug("Found free connection - OK");
  } else {
    this->logError("No free connections available - restarting");
    this->cleanStart(true);
  }
}

bool ESPProxy::hasFreeConnection() {
  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (this->connections[i] && this->connections[i]->isFree()) {
      return true;
    }
  }
  return false;
}

void ESPProxy::cleanStart(bool restart) {
  this->logInfo("Cleaning up connections...");
  
  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (this->connections[i]) {
      this->connections[i]->cleanupSockets();
      delete this->connections[i];
      this->connections[i] = nullptr;
    }
  }
  
  this->connectionCount = 0;
  
  if (restart) {
    this->logInfo("Restarting proxy...");
    delay(100);
    
    if (strlen(this->config.uniqueId) > 0) {
      this->makeNewCloudConnection();
      this->lastConnectionCheck = millis();
    } else {
      this->logError("No unique ID - cannot restart");
    }
  }
}

void ESPProxy::removeConnection(Context* ctx) {
  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (this->connections[i] == ctx) {
      this->connections[i]->cleanupSockets();
      delete this->connections[i];
      this->connections[i] = nullptr;
      this->connectionCount--;
      
      if (this->connectionCount == 0) {
        this->logWarning("No more connections - restarting");
        this->cleanStart(true);
      }
      return;
    }
  }
}

///////////////
// Logging Functions
///////////////

void ESPProxy::logDebug(const char* msg) {
  if (this->debug) {
    Serial.print("DEBUG: ");
    Serial.println(msg);
  }
}

void ESPProxy::logInfo(const char* msg) {
  Serial.print("INFO: ");
  Serial.println(msg);
}

void ESPProxy::logWarning(const char* msg) {
  Serial.print("WARNING: ");
  Serial.println(msg);
}

void ESPProxy::logError(const char* msg) {
  Serial.print("ERROR: **** ");
  Serial.print(msg);
  Serial.println(" ****");
}
