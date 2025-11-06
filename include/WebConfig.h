#ifndef WEBCONFIG_H
#define WEBCONFIG_H

#include <Arduino.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "ESPProxy.h"

class WebConfig {
public:
  WebConfig(ESPProxy* proxy);
  ~WebConfig();
  
  bool begin();
  void loop();  // Must be called regularly to handle HTTP requests
  
  // Load configuration from NVRAM
  bool loadConfig(ProxyConfig& config, String& mdnsHostname);
  
  // Save configuration to NVRAM
  bool saveConfig(const ProxyConfig& config, const String& mdnsHostname);
  
  // Check if this is first boot (no saved config)
  bool isFirstBoot();
  
  // Get current mDNS hostname
  String getMDNSHostname();
  
private:
  ESPProxy* proxyInstance;
  WebServer* server;
  Preferences preferences;
  String currentMDNS;
  
  // HTTP handlers
  void handleRoot();
  void handleStatus();
  void handleSave();
  void handleRestart();
  void handleNotFound();
  
  // Helper functions
  String generateHTML();
  String generateStatusJSON();
  
  // Configuration management
  void loadIntParameter(const char* key, int& value, int defaultValue);
  void loadStringParameter(const char* key, char* value, size_t maxLen, const char* defaultValue);
  void loadBoolParameter(const char* key, bool& value, bool defaultValue);
};

#endif // WEBCONFIG_H
