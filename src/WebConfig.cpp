#include "WebConfig.h"
#include "config.h"
#include "ConfigPage.h"

WebConfig::WebConfig(ESPProxy* proxy) {
  this->proxy = proxy;
  this->server = nullptr;
  
  // Initialize Preferences early so loadConfig() can use it
  this->preferences.begin("duotecno", false); // false = read/write mode
  Serial.println("[CONFIG] Preferences initialized");
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
  Serial.print("Web server started on port ");
  Serial.println(WEB_SERVER_PORT);
  
  // Start mDNS
  if (MDNS.begin(this->currentMDNS.c_str())) {
    Serial.print("mDNS responder started: http://");
    Serial.print(this->currentMDNS);
    Serial.println(".local");
    
    // Add service
    MDNS.addService("http", "tcp", WEB_SERVER_PORT);
  } else {
    Serial.println("Error starting mDNS");
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

bool WebConfig::loadConfig(ProxyConfig& config, String& mdnsHostname) {
  if (this->isFirstBoot()) {
    Serial.println("[CONFIG] First boot - using default config");
    return false;
  }
  
  Serial.println("[CONFIG] Loading configuration from NVRAM...");
  Serial.println("[CONFIG] ======== NVRAM Contents ========");
  
  // Load all parameters
  this->loadStringParameter("cloudServer", config.cloudServer, sizeof(config.cloudServer), CLOUD_SERVER);
  Serial.print("[CONFIG]   cloudServer: ");
  Serial.println(config.cloudServer);
  
  this->loadIntParameter("cloudPort", (int&)config.cloudPort, CLOUD_PORT);
  Serial.print("[CONFIG]   cloudPort: ");
  Serial.println(config.cloudPort);
  
  this->loadStringParameter("masterAddr", config.masterAddress, sizeof(config.masterAddress), MASTER_ADDRESS);
  Serial.print("[CONFIG]   masterAddress: ");
  Serial.println(config.masterAddress);
  
  this->loadIntParameter("masterPort", (int&)config.masterPort, MASTER_PORT);
  Serial.print("[CONFIG]   masterPort: ");
  Serial.println(config.masterPort);
  
  this->loadStringParameter("uniqueId", config.uniqueId, sizeof(config.uniqueId), UNIQUE_ID);
  Serial.print("[CONFIG]   uniqueId: ");
  Serial.println(config.uniqueId);
  
  this->loadBoolParameter("debug", config.debug, DEBUG_MODE);
  Serial.print("[CONFIG]   debug: ");
  Serial.println(config.debug ? "true" : "false");
  
  mdnsHostname = this->preferences.getString("mdnsHostname", MDNS_HOSTNAME);
  Serial.print("[CONFIG]   mdnsHostname: ");
  Serial.println(mdnsHostname);
  
  Serial.println("[CONFIG] ============================");
  Serial.println("[CONFIG] Configuration loaded successfully");
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
  this->preferences.putBool("configured", true);
  
  Serial.println("[CONFIG] Configuration saved successfully");
  return true;
}

void WebConfig::loadStringParameter(const char* key, char* value, size_t maxLen, const char* defaultValue) {
  String stored = this->preferences.getString(key, defaultValue);
  strncpy(value, stored.c_str(), maxLen - 1);
  value[maxLen - 1] = '\0';
}

void WebConfig::loadIntParameter(const char* key, int& value, int defaultValue) {
  value = this->preferences.getInt(key, defaultValue);
}

void WebConfig::loadBoolParameter(const char* key, bool& value, bool defaultValue) {
  value = this->preferences.getBool(key, defaultValue);
}

void WebConfig::handleRoot() {
  String html = this->generateHTML();
  this->server->send(200, "text/html", html);
}

void WebConfig::handleStatus() {
  String json = this->generateStatusJSON();
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
  this->server->send(404, "text/plain", "These are not the droids you're looking for.");
}

String WebConfig::generateStatusJSON() {
  String json = "{";
  json += "\"connectionCount\":" + String(this->proxy ? this->proxy->getActiveConnectionCount() : 0) + ",";
  json += "\"freeConnections\":" + String(this->proxy ? this->proxy->getFreeConnectionCount() : 0) + ",";
  json += "\"maxConnections\":" + String(MAX_CONNECTIONS) + ",";
  json += "\"bytesTransferred\":" + String(this->proxy ? this->proxy->getTotalBytesTransferred() : 0) + ",";
  json += "\"clientConnections\":" + String(this->proxy ? this->proxy->getTotalClientConnections() : 0) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"ip\":\"" + ETH.localIP().toString() + "\",";
  json += "\"connections\":[";
  
  // TODO: Add connection details when we expose them from ESPProxy
  
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
    String tempMDNS;
    this->loadConfig(currentConfig, tempMDNS);
  }
  
  // Use the template function to generate HTML
  return generateConfigPage(
    &currentConfig,
    this->currentMDNS.c_str()
  );
}
