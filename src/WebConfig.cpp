#include "WebConfig.h"
#include "config.h"
#include "ConfigPage.h"

WebConfig::WebConfig(ESPProxy* proxy) {
  this->proxy = proxy;
  this->preferences.begin("duotecno", false);
  // Note: loadConfig() is called separately in main.cpp with ProxyConfig parameter
}

WebConfig::~WebConfig() {
  if (this->server) {
    this->server->stop();
    delete this->server;
  }
  this->preferences.end();
}

bool WebConfig::begin() {
  // Preferences already initialized in constructor
  
  // Load or use default mDNS hostname
  this->currentMDNS = this->preferences.getString("mdnsHostname", MDNS_HOSTNAME);
  
  // Create web server
  this->server = new WebServer(WEB_SERVER_PORT);
  
  // Set up routes
  this->server->on("/", [this]() { this->handleRoot(); });
  this->server->on("/status", HTTP_GET, [this]() { this->handleStatus(); });
  this->server->on("/save", HTTP_POST, [this]() { this->handleSave(); });
  this->server->on("/restart", HTTP_POST, [this]() { this->handleRestart(); });
  this->server->onNotFound([this]() { this->handleNotFound(); });
  
  // Start server
  this->server->begin();
  Serial.print("[WEB] http server started on port ");
  Serial.println(WEB_SERVER_PORT);
  
  // Start mDNS
  if (MDNS.begin(this->currentMDNS.c_str())) {
    Serial.print("[WEB] mDNS responder started: http://");
    Serial.print(this->currentMDNS);
    Serial.println(".local");
    
    // Add service
    MDNS.addService("http", "tcp", WEB_SERVER_PORT);
  } else {
    Serial.println("[WEB] Error starting mDNS");
    return false;
  }
  
  return true;
}

void WebConfig::loop() {
  if (this->server) {
    this->server->handleClient();
  }
}

bool WebConfig::isFirstBoot() {
  return !this->preferences.isKey("configured");
}

String WebConfig::getMDNSHostname() {
  return this->currentMDNS;
}

bool WebConfig::loadConfig(ProxyConfig& config) {
  if (this->isFirstBoot()) {
    Serial.println("[CONFIG] First boot - loading compile-time defaults");
  } else {
    Serial.println("[CONFIG] Configuration from NVRAM (with compile-time fallbacks)");
  }
  
  // Load all parameters
  this->loadStringParameter("cloudServer", config.cloudServer, sizeof(config.cloudServer), CLOUD_SERVER);
  Serial.print("[CONFIG] === cloudServer: ");
  Serial.println(config.cloudServer);
  
  this->loadUShortParameter("cloudPort", config.cloudPort, CLOUD_PORT);
  Serial.print("[CONFIG] === cloudPort: ");
  Serial.println(config.cloudPort);
  
  this->loadStringParameter("masterAddr", config.masterAddress, sizeof(config.masterAddress), MASTER_ADDRESS);
  Serial.print("[CONFIG] === masterAddress: ");
  Serial.println(config.masterAddress);
  
  this->loadUShortParameter("masterPort", config.masterPort, MASTER_PORT);
  Serial.print("[CONFIG] === masterPort: ");
  Serial.println(config.masterPort);
  
  this->loadStringParameter("uniqueId", config.uniqueId, sizeof(config.uniqueId), UNIQUE_ID);
  Serial.print("[CONFIG] === uniqueId: ");
  Serial.println(config.uniqueId);
  
  this->loadBoolParameter("debug", config.debug, DEBUG_MODE);
  Serial.print("[CONFIG] === debug: ");
  Serial.println(config.debug ? "true" : "false");
  
  // Load network configuration from NVRAM with config.h defaults
  this->loadBoolParameter("useDHCP", config.useDHCP, USE_DHCP);
  Serial.print("[CONFIG] === useDHCP: ");
  Serial.println(config.useDHCP ? "true" : "false");
  
  this->loadStringParameter("staticIP", config.staticIP, sizeof(config.staticIP), LOCAL_IP);
  Serial.print("[CONFIG] === staticIP: ");
  Serial.println(config.staticIP);
  
  this->loadStringParameter("gateway", config.gateway, sizeof(config.gateway), GATEWAY_IP);
  Serial.print("[CONFIG] === gateway: ");
  Serial.println(config.gateway);
  
  this->loadStringParameter("subnet", config.subnet, sizeof(config.subnet), SUBNET_MASK);
  Serial.print("[CONFIG] === subnet: ");
  Serial.println(config.subnet);
  
  this->loadStringParameter("dns", config.dns, sizeof(config.dns), DNS_SERVER);
  Serial.print("[CONFIG] === dns: ");
  Serial.println(config.dns);
  
  return true;
}

bool WebConfig::saveConfig(const ProxyConfig& config, const String& mdnsHostname) {
  Serial.println("[CONFIG] Saving configuration to NVRAM...");
  
  this->preferences.putString("cloudServer", config.cloudServer);
  this->preferences.putUShort("cloudPort", config.cloudPort);
  this->preferences.putString("masterAddr", config.masterAddress);
  this->preferences.putUShort("masterPort", config.masterPort);
  this->preferences.putString("uniqueId", config.uniqueId);
  this->preferences.putBool("debug", config.debug);
  this->preferences.putString("mdnsHostname", mdnsHostname);
  
  // Save network configuration
  this->preferences.putBool("useDHCP", config.useDHCP);
  this->preferences.putString("staticIP", config.staticIP);
  this->preferences.putString("gateway", config.gateway);
  this->preferences.putString("subnet", config.subnet);
  this->preferences.putString("dns", config.dns);
  
  this->preferences.putBool("configured", true);
  
  Serial.println("[CONFIG] Configuration saved successfully");
  return true;
}

void WebConfig::loadStringParameter(const char* key, char* value, size_t maxLen, const char* defaultValue) {
  if (this->preferences.isKey(key)) {
    String stored = this->preferences.getString(key, defaultValue);
    strncpy(value, stored.c_str(), maxLen - 1);
    value[maxLen - 1] = '\0';
  } else {
    // Use default
    strncpy(value, defaultValue, maxLen - 1);
    value[maxLen - 1] = '\0';
  }
}

void WebConfig::loadUShortParameter(const char* key, uint16_t& value, uint16_t defaultValue) {
  if (this->preferences.isKey(key)) {
   value = this->preferences.getUShort(key, defaultValue);
  } else {
    value = defaultValue;
  }
}

void WebConfig::loadBoolParameter(const char* key, bool& value, bool defaultValue) {
  if (this->preferences.isKey(key)) {
   value = this->preferences.getBool(key, defaultValue);
  } else {
    value = defaultValue;
  }
}

void WebConfig::handleRoot() {
  String html = this->generateHTML();
  if (this->proxy && this->proxy->getConfig().debug) {
    Serial.println("[WEB] Serving configuration page");
  }
 this->server->send(200, "text/html", html);
}

void WebConfig::handleStatus() {
  String json = this->generateStatusJSON();
  if (this->proxy && this->proxy->getConfig().debug) {
    Serial.println("[WEB] Serving status JSON");
  }
  this->server->send(200, "application/json", json);
}

void WebConfig::handleSave() {
  Serial.println("[WEB] Received configuration update");
  
  // Get current config
  ProxyConfig newConfig;
  
  // Parse form data
  if (this->server->hasArg("cloudServer")) {
    strncpy(newConfig.cloudServer, this->server->arg("cloudServer").c_str(), sizeof(newConfig.cloudServer) - 1);
  }
  if (this->server->hasArg("cloudPort")) {
    newConfig.cloudPort = this->server->arg("cloudPort").toInt();
  }
  if (this->server->hasArg("masterAddress")) {
    strncpy(newConfig.masterAddress, this->server->arg("masterAddress").c_str(), sizeof(newConfig.masterAddress) - 1);
  }
  if (this->server->hasArg("masterPort")) {
    newConfig.masterPort = this->server->arg("masterPort").toInt();
  }
  if (this->server->hasArg("uniqueId")) {
    strncpy(newConfig.uniqueId, this->server->arg("uniqueId").c_str(), sizeof(newConfig.uniqueId) - 1);
  }
  // Checkbox: present in POST = checked (true), absent = unchecked (false)
  newConfig.debug = this->server->hasArg("debug");
  
  // Parse network configuration
  newConfig.useDHCP = this->server->hasArg("useDHCP");
  if (this->server->hasArg("staticIP")) {
    strncpy(newConfig.staticIP, this->server->arg("staticIP").c_str(), sizeof(newConfig.staticIP) - 1);
  }
  if (this->server->hasArg("gateway")) {
    strncpy(newConfig.gateway, this->server->arg("gateway").c_str(), sizeof(newConfig.gateway) - 1);
  }
  if (this->server->hasArg("subnet")) {
    strncpy(newConfig.subnet, this->server->arg("subnet").c_str(), sizeof(newConfig.subnet) - 1);
  }
  if (this->server->hasArg("dns")) {
    strncpy(newConfig.dns, this->server->arg("dns").c_str(), sizeof(newConfig.dns) - 1);
  }
    
  // Get mDNS hostname
  String newMDNS = this->currentMDNS;
  if (this->server->hasArg("mdnsHostname")) {
    newMDNS = this->server->arg("mdnsHostname");
  }
  
  // Save to NVRAM
  if (this->saveConfig(newConfig, newMDNS)) {
    // Update running proxy instance immediately (without restart)
    if (this->proxy) {
      this->proxy->setDebug(newConfig.debug);
      Serial.print("[WEB] Updated running proxy debug flag to: ");
      Serial.println(newConfig.debug ? "true" : "false");
    }
    
    // Send HTML response with restart button
    this->server->send(200, "text/html", generateSavePage());
  } else {
    this->server->send(500, "text/plain", "Failed to save configuration");
  }
}

void WebConfig::handleRestart() {
  Serial.println("[WEB] Restart requested via web interface");
  this->server->send(200, "text/plain", "Restarting ESP32...");
  this->proxy->cleanStart(true);
}

void WebConfig::handleNotFound() {
  // Log the request for debugging
  String uri = this->server->uri();
  String method = (this->server->method() == HTTP_GET) ? "GET" : "POST";
  
  if (this->proxy && this->proxy->getConfig().debug) {
    Serial.print("[WEB] 404 Not Found: ");
    Serial.print(method);
    Serial.print(" ");
    Serial.println(uri);
  }

  this->server->send(404, "text/plain", "404 These are not the droids you're looking for: " + method + " " + uri);
}

String WebConfig::generateStatusJSON() {
  int activeCount = this->proxy ? this->proxy->getActiveConnectionCount() : 0;
  int freeCount = this->proxy ? this->proxy->getFreeConnectionCount() : 0;
  
  String json = "{";
  json += "\"connectionCount\":" + String(activeCount) + ",";
  json += "\"freeConnections\":" + String(freeCount) + ",";
  json += "\"maxConnections\":" + String(MAX_CONNECTIONS) + ",";
  json += "\"bytesTransferred\":" + String(this->proxy ? this->proxy->getTotalBytesTransferred() : 0) + ",";
  json += "\"clientConnections\":" + String(this->proxy ? this->proxy->getTotalClientConnections() : 0) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"ip\":\"" + ETH.localIP().toString() + "\",";
  json += "\"connections\":[";
  
  // Generate connection details JSON inline
  if (this->proxy) {
    bool first = true;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
      Context* conn = this->proxy->getConnection(i);
      if (conn) {
        if (!first) json += ",";
        first = false;
        
        json += "{";
        json += "\"slot\":" + String(i) + ",";
        json += "\"id\":" + String(conn->getConnectionId()) + ",";
        json += "\"cloudSocket\":" + String(conn->hasCloudSocket() ? "true" : "false") + ",";
        json += "\"deviceSocket\":" + String(conn->hasDeviceSocket() ? "true" : "false") + ",";
        json += "\"cloudConnected\":" + String(conn->isCloudConnected() ? "true" : "false") + ",";
        json += "\"deviceConnected\":" + String(conn->isDeviceConnected() ? "true" : "false") + ",";
        json += "\"status\":\"" + String(conn->isFree() ? "FREE" : "ACTIVE") + "\"";
        json += "}";
      }
    }
  }
  
  json += "]";
  json += "}";
  return json;
}

String WebConfig::generateHTML() {
  // Get CURRENT running configuration from the proxy
  ProxyConfig currentConfig;
  if (this->proxy) {
    currentConfig = this->proxy->getConfig();
  } else {
    // Fallback: load from NVRAM (shouldn't happen normally)
    this->loadConfig(currentConfig);
  }
  
  // Ensure static IP fields have values (from config.h if not set in NVRAM)
  // This way they're always prefilled even when using DHCP
  if (strlen(currentConfig.staticIP) == 0) {
    strncpy(currentConfig.staticIP, LOCAL_IP, sizeof(currentConfig.staticIP) - 1);
  }
  if (strlen(currentConfig.gateway) == 0) {
    strncpy(currentConfig.gateway, GATEWAY_IP, sizeof(currentConfig.gateway) - 1);
  }
  if (strlen(currentConfig.subnet) == 0) {
    strncpy(currentConfig.subnet, SUBNET_MASK, sizeof(currentConfig.subnet) - 1);
  }
  if (strlen(currentConfig.dns) == 0) {
    strncpy(currentConfig.dns, DNS_SERVER, sizeof(currentConfig.dns) - 1);
  }
  
  // Use the template function to generate HTML
  return generateConfigPage(
    &currentConfig,
    this->currentMDNS.c_str()
  );
}
