#include "WebConfig.h"
#include "config.h"

WebConfig::WebConfig(ESPProxy* proxy) {
  this->proxyInstance = proxy;
  this->server = nullptr;
}

WebConfig::~WebConfig() {
  if (this->server) {
    this->server->stop();
    delete this->server;
  }
  this->preferences.end();
}

bool WebConfig::begin() {
  // Initialize Preferences
  this->preferences.begin("duotecno", false); // false = read/write mode
  
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
  
  // Load all parameters
  this->loadStringParameter("cloudServer", config.cloudServer, sizeof(config.cloudServer), CLOUD_SERVER);
  this->loadIntParameter("cloudPort", (int&)config.cloudPort, CLOUD_PORT);
  this->loadStringParameter("masterAddr", config.masterAddress, sizeof(config.masterAddress), MASTER_ADDRESS);
  this->loadIntParameter("masterPort", (int&)config.masterPort, MASTER_PORT);
  this->loadStringParameter("uniqueId", config.uniqueId, sizeof(config.uniqueId), UNIQUE_ID);
  this->loadBoolParameter("debug", config.debug, DEBUG_MODE);
  mdnsHostname = this->preferences.getString("mdnsHostname", MDNS_HOSTNAME);
  
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
    // Send HTML response with restart button
    String response = R"html(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Configuration Saved</title>
  <style>
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .container {
      background: white;
      border-radius: 12px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      padding: 40px;
      max-width: 500px;
      text-align: center;
    }
    h1 { color: #28a745; margin-bottom: 20px; }
    p { color: #333; margin-bottom: 30px; line-height: 1.6; }
    .btn {
      padding: 15px 40px;
      border: none;
      border-radius: 6px;
      font-size: 16px;
      font-weight: 500;
      cursor: pointer;
      transition: all 0.2s;
      margin: 10px;
    }
    .btn-primary {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
    }
    .btn-primary:hover {
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
    }
    .btn-secondary {
      background: #6c757d;
      color: white;
    }
    .btn-secondary:hover {
      background: #5a6268;
    }
    #status { margin-top: 20px; color: #667eea; font-weight: 500; }
  </style>
</head>
<body>
  <div class="container">
    <h1>✅ Configuration Saved!</h1>
    <p>Your settings have been saved successfully to non-volatile memory.</p>
    <p><strong>A restart is required for changes to take effect.</strong></p>
    <button class="btn btn-primary" onclick="restartESP()">🔄 Restart ESP32 Now</button>
    <button class="btn btn-secondary" onclick="goBack()">← Back to Settings</button>
    <div id="status"></div>
  </div>
  <script>
    function restartESP() {
      document.getElementById('status').textContent = 'Restarting ESP32... Please wait 10 seconds.';
      fetch('/restart', { method: 'POST' })
        .then(() => {
          document.getElementById('status').textContent = 'ESP32 is restarting... Redirecting in 10 seconds.';
          setTimeout(() => {
            window.location.href = '/';
          }, 10000);
        })
        .catch(err => {
          document.getElementById('status').textContent = 'ESP32 is restarting... (Connection lost - this is normal)';
          setTimeout(() => {
            window.location.href = '/';
          }, 10000);
        });
    }
    function goBack() {
      window.location.href = '/';
    }
  </script>
</body>
</html>
)html";
    this->server->send(200, "text/html", response);
  } else {
    this->server->send(500, "text/plain", "Failed to save configuration");
  }
}

void WebConfig::handleRestart() {
  Serial.println("[WEB] Restart requested via web interface");
  this->server->send(200, "text/plain", "Restarting ESP32...");
  delay(500);  // Give time for response to be sent
  ESP.restart();
}

void WebConfig::handleNotFound() {
  this->server->send(404, "text/plain", "These are not the droids you're looking for.");
}

String WebConfig::generateStatusJSON() {
  String json = "{";
  json += "\"connectionCount\":" + String(this->proxyInstance ? this->proxyInstance->getConnectionCount() : 0) + ",";
  json += "\"freeConnections\":" + String(this->proxyInstance ? this->proxyInstance->getFreeConnectionCount() : 0) + ",";
  json += "\"maxConnections\":" + String(MAX_CONNECTIONS) + ",";
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
  if (this->proxyInstance) {
    currentConfig = this->proxyInstance->getConfig();
  } else {
    // Fallback: load from NVRAM (shouldn't happen normally)
    String tempMDNS;
    this->loadConfig(currentConfig, tempMDNS);
  }
  
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Duotecno Cloud Proxy Configuration</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
    }
    .container {
      max-width: 800px;
      margin: 0 auto;
      background: white;
      border-radius: 12px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      overflow: hidden;
    }
    .header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 30px;
      text-align: center;
    }
    .header h1 { font-size: 28px; margin-bottom: 10px; }
    .header p { opacity: 0.9; font-size: 14px; }
    .status {
      padding: 20px 30px;
      background: #f8f9fa;
      border-bottom: 1px solid #e9ecef;
    }
    .status-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
      gap: 15px;
    }
    .status-item {
      background: white;
      padding: 15px;
      border-radius: 8px;
      border-left: 4px solid #667eea;
    }
    .status-item label {
      display: block;
      font-size: 12px;
      color: #6c757d;
      margin-bottom: 5px;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    .status-item .value {
      font-size: 24px;
      font-weight: bold;
      color: #333;
    }
    .status-item .value.good { color: #28a745; }
    .status-item .value.warning { color: #ffc107; }
    .content {
      padding: 30px;
    }
    .section {
      margin-bottom: 30px;
    }
    .section h2 {
      font-size: 20px;
      margin-bottom: 15px;
      color: #333;
      border-bottom: 2px solid #667eea;
      padding-bottom: 10px;
    }
    .form-group {
      margin-bottom: 20px;
    }
    .form-group label {
      display: block;
      margin-bottom: 8px;
      color: #495057;
      font-weight: 500;
      font-size: 14px;
    }
    .form-group input[type="text"],
    .form-group input[type="number"] {
      width: 100%;
      padding: 12px;
      border: 2px solid #e9ecef;
      border-radius: 6px;
      font-size: 14px;
      transition: border-color 0.2s;
    }
    .form-group input:focus {
      outline: none;
      border-color: #667eea;
    }
    .checkbox-group {
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .checkbox-group input[type="checkbox"] {
      width: 20px;
      height: 20px;
      cursor: pointer;
      margin: 0;
      flex-shrink: 0;
    }
    .checkbox-group label {
      margin-bottom: 0;
      display: inline;
      cursor: pointer;
    }
    .warning-box {
      background: #fff3cd;
      border-left: 4px solid #ffc107;
      padding: 15px;
      border-radius: 6px;
      margin: 20px 0;
      color: #856404;
    }
    .warning-box strong { display: block; margin-bottom: 5px; }
    .button-group {
      display: flex;
      gap: 10px;
      margin-top: 30px;
    }
    .btn {
      padding: 12px 30px;
      border: none;
      border-radius: 6px;
      font-size: 16px;
      font-weight: 500;
      cursor: pointer;
      transition: all 0.2s;
    }
    .btn-primary {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      flex: 1;
    }
    .btn-primary:hover {
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
    }
    .btn-secondary {
      background: #6c757d;
      color: white;
    }
    .btn-secondary:hover {
      background: #5a6268;
    }
    .footer {
      text-align: center;
      padding: 20px;
      background: #f8f9fa;
      color: #6c757d;
      font-size: 12px;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>🌐 Duotecno Cloud Proxy</h1>
      <p>ESP32 Network Configuration Interface</p>
    </div>
    
    <div class="status">
      <div class="status-grid">
        <div class="status-item">
          <label>Active Connections</label>
          <div class="value good" id="connCount">-</div>
        </div>
        <div class="status-item">
          <label>Free Connections</label>
          <div class="value" id="freeCount">-</div>
        </div>
        <div class="status-item">
          <label>Max Connections</label>
          <div class="value">)rawliteral" + String(MAX_CONNECTIONS) + R"rawliteral(</div>
        </div>
        <div class="status-item">
          <label>Uptime</label>
          <div class="value" id="uptime">-</div>
        </div>
        <div class="status-item">
          <label>IP Address</label>
          <div class="value" style="font-size: 16px" id="ipAddr">)rawliteral" + ETH.localIP().toString() + R"rawliteral(</div>
        </div>
      </div>
    </div>
    
    <form method="POST" action="/save" class="content">
      <div class="section">
        <h2>☁️ Cloud Server Settings</h2>
        <div class="form-group">
          <label for="cloudServer">Cloud Server Address</label>
          <input type="text" id="cloudServer" name="cloudServer" value=")rawliteral" + String(currentConfig.cloudServer) + R"rawliteral(" required>
        </div>
        <div class="form-group">
          <label for="cloudPort">Cloud Server Port</label>
          <input type="number" id="cloudPort" name="cloudPort" value=")rawliteral" + String(currentConfig.cloudPort) + R"rawliteral(" required min="1" max="65535">
        </div>
      </div>
      
      <div class="section">
        <h2>🏠 Local Master Device Settings</h2>
        <div class="form-group">
          <label for="masterAddress">Master Device IP Address</label>
          <input type="text" id="masterAddress" name="masterAddress" value=")rawliteral" + String(currentConfig.masterAddress) + R"rawliteral(" required pattern="^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$">
        </div>
        <div class="form-group">
          <label for="masterPort">Master Device Port</label>
          <input type="number" id="masterPort" name="masterPort" value=")rawliteral" + String(currentConfig.masterPort) + R"rawliteral(" required min="1" max="65535">
        </div>
      </div>
      
      <div class="section">
        <h2>🔑 Identification</h2>
        <div class="form-group">
          <label for="uniqueId">Unique ID (your DDNS address)</label>
          <input type="text" id="uniqueId" name="uniqueId" value=")rawliteral" + String(currentConfig.uniqueId) + R"rawliteral(" required>
        </div>
        <div class="form-group">
          <label for="mdnsHostname">mDNS Hostname (without .local)</label>
          <input type="text" id="mdnsHostname" name="mdnsHostname" value=")rawliteral" + this->currentMDNS + R"rawliteral(" required pattern="[a-z0-9\-]+">
          <small style="color: #6c757d; font-size: 12px; display: block; margin-top: 5px;">
            Access device at http://<span style="color: #667eea; font-weight: bold;">)rawliteral" + this->currentMDNS + R"rawliteral(</span>.local
          </small>
        </div>
      </div>
      
      <div class="section">
        <h2>⚙️ Advanced Settings</h2>
        <div class="form-group checkbox-group">
          <input type="checkbox" id="debug" name="debug" value="true" )rawliteral" + String(currentConfig.debug ? "checked" : "") + R"rawliteral(>
          <label for="debug">Enable Debug Logging</label>
        </div>
        <div class="form-group">
          <label>Maximum Connections: )rawliteral" + String(MAX_CONNECTIONS) + R"rawliteral( (compile-time setting)</label>
        </div>
        <div class="form-group">
          <label>Connection Check Interval: )rawliteral" + String(CONNECTION_CHECK_INTERVAL / 1000) + R"rawliteral(s (compile-time setting)</label>
        </div>
      </div>
      
      <div class="section">
        <h2>ℹ️ Current Configuration Source</h2>
        <div class="warning-box" style="background: #d1ecf1; border-left-color: #0c5460; color: #0c5460;">
          <strong>Configuration Status</strong>
          These values are currently running on the device. After saving new values, you must restart the ESP32 for changes to take effect.
        </div>
      </div>
      
      <div class="section">
        <h2>🌐 Network Settings</h2>
        <div class="warning-box">
          <strong>⚠️ Not Yet Implemented</strong>
          Network settings (DHCP, Static IP, DNS, Gateway, Subnet Mask) cannot be changed via web interface yet. 
          Please edit config.h and recompile to change these settings.
        </div>
        <div class="form-group">
          <label>Current Mode: )rawliteral" + String(
#ifdef USE_DHCP
    "DHCP"
#else
    "Static IP"
#endif
  ) + R"rawliteral(</label>
        </div>
      </div>
      
      <div class="button-group">
        <button type="submit" class="btn btn-primary">💾 Save Configuration</button>
        <button type="button" class="btn btn-secondary" onclick="location.reload()">Reload</button>
      </div>
    </form>
    
    <div class="footer">
      ESP32 Duotecno Cloud Proxy ❤️ for reliable connections - Johan for Duotecno 2025
    </div>
  </div>
  
  <script>
    function updateStatus() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          document.getElementById('connCount').textContent = data.connectionCount;
          document.getElementById('freeCount').textContent = data.freeConnections;
          document.getElementById('uptime').textContent = formatUptime(data.uptime);
        })
        .catch(err => console.error('Status update failed:', err));
    }
    
    function formatUptime(seconds) {
      const days = Math.floor(seconds / 86400);
      const hours = Math.floor((seconds % 86400) / 3600);
      const mins = Math.floor((seconds % 3600) / 60);
      if (days > 0) return days + 'd ' + hours + 'h';
      if (hours > 0) return hours + 'h ' + mins + 'm';
      return mins + 'm';
    }
    
    // Update status every 5 seconds
    updateStatus();
    setInterval(updateStatus, 5000);
  </script>
</body>
</html>
)rawliteral";
  
  return html;
}
