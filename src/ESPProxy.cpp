#include "ESPProxy.h"
#include "config.h"

////////////////////////////
// Context Implementation //
////////////////////////////
//
// A Context is a single cloud-to-device connection pair.
// It manages the two sockets and data forwarding between them.
//
Context::Context(WiFiClient* cloudSocket, ESPProxy* proxy, int connectionId) {
  // We receive 
  //  a socket already connected to the cloud server
  //  a reference to the parent ESPProxy instance
  //  a connection identifier for debugging (global number incremented by ESPProxy)
  this->cloudSocket = cloudSocket;
  this->proxy = proxy;
  this->connectionId = connectionId;

  // initialize members
  this->deviceSocket = nullptr;
  
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
      this->proxy->removeConnection(this);
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
      // Device disconnected - close entire connection (both device and cloud)
      Serial.println("[DEVICE] Connection closed - removing entire connection");
      this->proxy->removeConnection(this);
      return;
    }
    
    if (this->deviceSocket->available() > 0) {
      // We have incoming data from device
      uint8_t buffer[512];
      int len = this->deviceSocket->read(buffer, sizeof(buffer));

      if (len > 0) {
        // Blink LED when forwarding device data to cloud
        this->blinkLED();
        this->proxy->logData(DEVICE_TO_CLOUD, len, buffer, this->connectionId);
        
        if (this->cloudSocket && this->cloudSocket->connected()) {
          this->cloudSocket->write(buffer, len);
          // Track statistics
          this->proxy->addBytesTransferred(len);
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
  
  if (!this->deviceSocket || !this->deviceConnected) {
    // No device connection yet - check what kind of message this is
    
    if (this->isHeartbeatRequest(strBuffer, len)) {
      this->proxy->logMessage(FROM_CLOUD, this->connectionId, "Heartbeat request, responding...");

      // answer the heartbeat request
      this->cloudSocket->write("[72,3]");
      return;
    }
    
    if (this->isConnectionResponse(strBuffer, len)) {
      // response from the server to our connection request
      this->proxy->logMessage(FROM_CLOUD, this->connectionId, "Connection response: ", strBuffer);
      return;
    }
    
    // Real data - a new client wants to connect
    //  -> we need to connect to the device 
    //     and forward this data + all next data
    this->proxy->logMessage(FROM_CLOUD, this->connectionId, "New client connection detected");
    
    // Track statistics - incoming client connection
    this->proxy->incrementClientConnections();
    
    // Connect to the device and forward this initial data
    this->makeDeviceConnection(buffer, len);
    
    // This connection is now busy with a device, so create a new free connection if needed
    if (this->deviceConnected && !this->proxy->hasFreeConnection()) {
      Serial.println("[PROXY] Connection now has device attached - creating new free connection...");
      this->proxy->makeNewCloudConnection();
    }

  } else if (this->deviceSocket && this->deviceConnected) {
    // Forward data to device
    this->proxy->logData(CLOUD_TO_DEVICE, len, buffer, this->connectionId);
    this->deviceSocket->write(buffer, len);
    // Track statistics
    this->proxy->addBytesTransferred(len);
  }
}

void Context::makeDeviceConnection(uint8_t* data, size_t len) {
  const ProxyConfig& config = this->proxy->getConfig();

  this->proxy->logDirection(TO_DEVICE);
  Serial.print("Connecting to device at ");
  Serial.print(config.masterAddress);
  Serial.print(":");
  Serial.println(config.masterPort);
  
  if (!this->deviceSocket) {
    this->deviceSocket = new WiFiClient();
  }
  
  // Parse IP address
  IPAddress deviceIP;
  if (deviceIP.fromString(config.masterAddress)) {
    if (this->deviceSocket->connect(deviceIP, config.masterPort)) {
      this->proxy->logMessage(TO_DEVICE, 0, "Connected to device");
      this->deviceConnected = true;
      
      if (config.debug) {
        // Send initial data
        this->proxy->logDirection(TO_DEVICE);
        Serial.print("Sending initial ");
        Serial.print(len);
        Serial.print(" bytes: ");
        for (int i = 0; i < len; i++) {
          if (data[i] != 0) Serial.write(data[i]);
        }
      }
      this->deviceSocket->write(data, len);
      // Track statistics
      this->proxy->addBytesTransferred(len);

    } else {
      this->proxy->logMessage(TO_DEVICE, 0, "Failed to connect to device");
      delete this->deviceSocket;
      this->deviceSocket = nullptr;
      this->deviceConnected = false;
    }
  } else {
    this->proxy->logMessage(TO_DEVICE, 0, "Invalid device IP address");
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

/////////////////////////////
// ESPProxy Implementation //
/////////////////////////////

ESPProxy::ESPProxy() {
  this->debug = false;
  this->connectionCount = 0;
  this->nextConnectionId = 0;
  this->lastConnectionCheck = 0;
  this->totalBytesTransferred = 0;
  this->totalClientConnections = 0;
  
  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    this->connections[i] = nullptr;
  }
}

ESPProxy::~ESPProxy() {
  this->cleanStart(false);
}

bool ESPProxy::begin(const ProxyConfig& cfg) {
  this->config = cfg;
  this->debug = cfg.debug;
  
  this->logInfo("ESP Proxy Starting");

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
    this->logError("Maximum connections reached, cannot create new connection");
    return;
  }
  
  Serial.print("[INFO] Attempt ");
  Serial.print(retryCount);
  Serial.print(" to make cloud connection to ");
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
    Serial.print("[PROXY -> CLOUD] Connected to cloud at ");
    Serial.print(this->config.cloudServer);
    Serial.print(":");
    Serial.println(this->config.cloudPort);
    
    // Send unique ID
    cloudSocket->print("[");
    cloudSocket->print(this->config.uniqueId);
    cloudSocket->print("]");
    
    this->logMessage(TO_CLOUD, 0, "Sent unique ID: ", this->config.uniqueId);
    
    // Create context and add to pool with unique ID
    this->nextConnectionId++;
    Context* ctx = new Context(cloudSocket, this, this->nextConnectionId);
    
    // Find empty slot
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
      if (this->connections[i] == nullptr) {
        this->connections[i] = ctx;
        this->connectionCount++;
        this->logMessage(TO_CLOUD, this->nextConnectionId, "New free connection");
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
  
  // Remove inactive connections
  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (this->connections[i] && !this->connections[i]->isActive()) {
      this->logDebug("Removing inactive connection...");
      delete this->connections[i];
      this->connections[i] = nullptr;
      this->connectionCount--;
    }
  }
  
  // Check if we have at least one free connection
  if (this->hasFreeConnection()) {
    this->logDebug("Found free connection - OK");
  } else {
    this->logError("No free connections available - creating new connection...");
    // Try to create a new connection instead of restarting
    this->makeNewCloudConnection();
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

int ESPProxy::getFreeConnectionCount() const {
  int count = 0;
  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (this->connections[i] && this->connections[i]->isFree()) {
      count++;
    }
  }
  return count;
}

int ESPProxy::getActiveConnectionCount() const {
  int count = 0;
  for (int i = 0; i < MAX_CONNECTIONS; i++) {
    if (this->connections[i] && !this->connections[i]->isFree()) {
      count++;
    }
  }
  return count;
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
  this->nextConnectionId = 0;
  this->lastConnectionCheck = millis();


  if (restart) {
    this->logInfo("Restarting proxy...");
    delay(100);
    ESP.restart();
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
        this->logError("No more connections - restarting");
        this->cleanStart(true);
      }
      return;
    }
  }
}

///////////////////////
// Logging Functions //
///////////////////////

void ESPProxy::logDebug(const char* msg) {
  if (this->debug) {
    Serial.print("[DEBUG] ");
    Serial.println(msg);
  }
}

void ESPProxy::logInfo(const char* msg) {
  Serial.print("[INFO] ");
  Serial.println(msg);
}


void ESPProxy::logError(const char* msg) {
  Serial.print("[ERROR] **** ");
  Serial.print(msg);
  Serial.println(" ****");
}

void ESPProxy::logDirection(ConnectionDirection direction) {
  switch (direction) {
    case DEVICE_TO_CLOUD:
      Serial.print("[DEVICE -> CLOUD] ");
      break;
    case CLOUD_TO_DEVICE:
      Serial.print("[CLOUD -> DEVICE] ");
      break;
    case FROM_CLOUD:
      Serial.print("[CLOUD -> PROXY] ");
      break;
    case TO_DEVICE:
      Serial.print("[PROXY -> DEVICE] ");
      break;
    case TO_CLOUD:
      Serial.print("[PROXY -> CLOUD] ");
      break;
    default:
      Serial.print("[UNKNOWN] ");
      break;
  }
}

void ESPProxy::logData(ConnectionDirection direction, int len, const uint8_t* buffer, int connectionId) {
  if (!this->debug) return;

  this->logDirection(direction);
  Serial.print("conn #");
  Serial.print(connectionId);
  Serial.print(": Forwarding ");
  Serial.print(len);
  Serial.print(" bytes: ");
  for (int i = 0; i < len; i++) {
    if (buffer[i] != 0) Serial.write(buffer[i]);
  }
}

void ESPProxy::logMessage(ConnectionDirection direction, int connectionId, 
                          const char* message, const char* extraStr) {

  this->logDirection(direction);
  if (connectionId) {
    Serial.print("conn #");
    Serial.print(connectionId);
    Serial.print(": ");
  }
  
  Serial.print(message);
  
  if (extraStr) {
    Serial.println(extraStr);
  } else {
    Serial.println();
  }
}