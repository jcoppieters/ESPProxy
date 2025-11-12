#ifndef CONFIGPAGE_H
#define CONFIGPAGE_H

#include <Arduino.h>
#include <ETH.h>
#include "ESPProxy.h"
#include "config.h"
#include "logo.h"

// Common CSS styles shared by all pages
static const char COMMON_STYLES[] PROGMEM = R"css(
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Arial, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; padding: 20px; display: flex; align-items: center; justify-content: center;
    }
    .container { background: white; border-radius: 12px; box-shadow: 0 20px 60px rgba(0,0,0,0.3);
    }
    .btn { padding: 15px 40px; border: none; border-radius: 6px; font-size: 16px; font-weight: 500; cursor: pointer; transition: all 0.2s; margin: 10px; text-decoration: none; display: inline-block;
    }
    .btn-primary { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white;
    }
    .btn-primary:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
    }
    .btn-secondary { background: #6c757d; color: white;
    }
    .btn-secondary:hover { background: #5a6268;
    }
)css";

// Generate the save confirmation page
inline String generateSavePage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Configuration Saved</title>
  <style>)rawliteral";
  
  html += COMMON_STYLES;
  
  html += R"rawliteral(
    .container { max-width: 500px; padding: 40px; text-align: center }
    h1 { color: #28a745; margin-bottom: 20px }
    p { color: #333; margin-bottom: 30px; line-height: 1.6 }
    #status { margin-top: 20px; color: #667eea; font-weight: 500 }
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
)rawliteral";
  
  return html;
}

// Generate the HTML configuration page
inline String generateConfigPage(
    const ProxyConfig* config,
    const char* mdnsHostname
) {
  // Get values from compile-time settings and runtime environment
  #ifdef USE_DHCP
    bool useDHCP = true;
  #else
    bool useDHCP = false;
  #endif
  
  String ipAddress = ETH.localIP().toString();
  int maxConnections = MAX_CONNECTIONS;
  int connectionCheckIntervalSeconds = CONNECTION_CHECK_INTERVAL / 1000;
  
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Duotecno Cloud Proxy Configuration</title>
  <style>)rawliteral";
  
  html += COMMON_STYLES;
  
  html += R"rawliteral(
    body { padding: 20px; align-items: flex-start }
    .container { max-width: 1000px; width: 100%; margin: 0 auto; overflow: hidden }
    .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 30px; text-align: center }
    .header h1 { font-size: 28px; margin-bottom: 10px; }
    .header p { opacity: 0.9; font-size: 14px; }
    .status { padding: 20px 30px; background: #f8f9fa; border-bottom: 1px solid #e9ecef }
    .status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px }
    .status-item { background: white; padding: 15px; border-radius: 8px; border-left: 4px solid #667eea }
    .status-item label { display: block; font-size: 12px; color: #6c757d; margin-bottom: 5px; 
                         text-transform: uppercase; letter-spacing: 0.5px }
    .status-item .value { font-size: 16px; font-weight: bold; color: #333 }
    .status-item .value.good { color: #28a745 }
    .status-item .value.warning { color: #ffc107 }
    .content { padding: 30px }
    .section { margin-bottom: 30px }
    .section h2 { font-size: 20px; margin-bottom: 15px; color: #333; border-bottom: 2px solid #667eea; padding-bottom: 10px }
    .form-group { margin-bottom: 20px }
    .form-group label { display: block; margin-bottom: 8px; color: #495057; font-weight: 500; font-size: 14px }
    .form-group input[type="text"],
    .form-group input[type="number"] { width: 100%; padding: 12px; border: 2px solid #e9ecef; 
                                       border-radius: 6px; font-size: 14px; transition: border-color 0.2s }
    .form-group input:focus { outline: none; border-color: #667eea }
    .checkbox-group { display: flex; align-items: center; gap: 10px }
    .checkbox-group input[type="checkbox"] { width: 20px; height: 20px; cursor: pointer; margin: 0; flex-shrink: 0 }
    .checkbox-group label { margin-bottom: 0; display: inline; cursor: pointer }
    .warning-box { background: #fff3cd; border-left: 4px solid #ffc107; padding: 15px; 
                   border-radius: 6px; margin: 20px 0; color: #856404 }
    .warning-box strong { display: block; margin-bottom: 5px }
    .button-group { display: flex; gap: 10px; margin-top: 30px } 
    .btn-primary {flex: 1}
    .btn-secondary:hover { transform: translateY(-2px) }
    .btn-secondary:hover { background: #5a6268 }
    .footer { text-align: center; padding: 20px; background: #f8f9fa; color: #6c757d; font-size: 12px }
    .header-content { display: flex; align-items: center; gap: 20px }
    .header-text { flex: 1 }
    .header-text h1 { font-size: 28px; margin-bottom: 10px; text-align: left }
    .header-text p { opacity: 0.9; font-size: 14px; text-align: left }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <div class="header-content">)rawliteral";
  
  html += DUOTECNO_LOGO_SVG;
  
  html += R"rawliteral(
        <div class="header-text">
          <h1>Duotecno Cloud Proxy</h1>
          <p>ESP32 Configuration Interface</p>
        </div>
      </div>
    </div>
    
    <div class="status">
      <div class="status-grid">
        <div class="status-item">
          <label>Active</label>
          <div class="value good" id="connCount">-</div>
        </div>
        <div class="status-item">
          <label>Free</label>
          <div class="value" id="freeCount">-</div>
        </div>
        <div class="status-item">
          <label>Max</label>
          <div class="value">)rawliteral" + String(maxConnections) + R"rawliteral(</div>
        </div>
        <div class="status-item" onclick="toggleConnectionDetails()" style="cursor: pointer;" title="Click to show/hide connection details">
          <label>Total</label>
          <div class="value" id="connections">-</div>
        </div>
        <div class="status-item">
          <label>Transferred</label>
          <div class="value" id="bytesTransferred">-</div>
        </div>
        <div class="status-item">
          <label>Handled</label>
          <div class="value" id="clientConnections">-</div>
        </div>
        <div class="status-item">
          <label>Uptime</label>
          <div class="value" id="uptime">-</div>
        </div>
        <div class="status-item">
          <label>IP Address</label>
          <div class="value" id="ipAddr">)rawliteral" + ipAddress + R"rawliteral(</div>
        </div>
      </div>
    </div>
    
    <div id="connectionDetails" class="content" style="display: none; padding-bottom: 0; margin-top: 20px">
      <div class="section">
        <h2>🔌 Connection Details</h2>
        <div id="connectionList" style="font-family: monospace; font-size: 14px">
          <!-- Connection details will be populated here -->
        </div>
      </div>
    </div>
    
    <form method="POST" action="/save" class="content">
      <div class="section">
        <h2>☁️ Cloud Server Settings</h2>
        <div class="form-group">
          <label for="cloudServer">Cloud Server Address</label>
          <input type="text" id="cloudServer" name="cloudServer" value=")rawliteral" + String(config->cloudServer) + R"rawliteral(" required>
        </div>
        <div class="form-group">
          <label for="cloudPort">Cloud Server Port</label>
          <input type="number" id="cloudPort" name="cloudPort" value=")rawliteral" + String(config->cloudPort) + R"rawliteral(" required min="1" max="65535">
        </div>
      </div>
      
      <div class="section">
        <h2>🏠 Local Master Device Settings</h2>
        <div class="form-group">
          <label for="masterAddress">Master Device IP Address</label>
          <input type="text" id="masterAddress" name="masterAddress" value=")rawliteral" + String(config->masterAddress) + R"rawliteral(" required pattern="^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$">
        </div>
        <div class="form-group">
          <label for="masterPort">Master Device Port</label>
          <input type="number" id="masterPort" name="masterPort" value=")rawliteral" + String(config->masterPort) + R"rawliteral(" required min="1" max="65535">
        </div>
      </div>
      
      <div class="section">
        <h2>🔑 Identification</h2>
        <div class="form-group">
          <label for="uniqueId">Unique ID (your DDNS address)</label>
          <input type="text" id="uniqueId" name="uniqueId" value=")rawliteral" + String(config->uniqueId) + R"rawliteral(" required>
        </div>
        <div class="form-group">
          <label for="mdnsHostname">mDNS Hostname (without .local)</label>
          <input type="text" id="mdnsHostname" name="mdnsHostname" value=")rawliteral" + String(mdnsHostname) + R"rawliteral(" required pattern="[a-z0-9\-]+">
          <small style="color: #6c757d; font-size: 12px; display: block; margin-top: 5px;">
            Access device at <span style="color: #667eea; font-weight: bold;">http://)rawliteral" + String(mdnsHostname) + R"rawliteral(.local</span>
          </small>
        </div>
      </div>
      
      <div class="section">
        <h2>⚙️ Advanced Settings</h2>
        <div class="form-group checkbox-group">
          <input type="checkbox" id="debug" name="debug" value="true" )rawliteral" + String(config->debug ? "checked" : "") + R"rawliteral(>
          <label for="debug">Enable Debug Logging</label>
        </div>
        <div class="form-group">
          <label>Maximum Connections: )rawliteral" + String(maxConnections) + R"rawliteral( (compile-time setting)</label>
          <label>Connection Check Interval: )rawliteral" + String(connectionCheckIntervalSeconds) + R"rawliteral(s (compile-time setting)</label>
        </div>
      </div>

      <div class="section">
        <h2>🌐 Network Settings</h2>
        <div class="form-group checkbox-group">
          <input type="checkbox" id="useDHCP" name="useDHCP" value="true" onchange="toggleStaticIPFields()" )rawliteral" + String(config->useDHCP ? "checked" : "") + R"rawliteral(>
          <label for="useDHCP">Use DHCP (automatic IP configuration)</label>
        </div>
        <div id="staticIPFields" style=")rawliteral" + String(config->useDHCP ? "display: none;" : "") + R"rawliteral(">
          <div class="form-group">
            <label for="staticIP">Static IP Address</label>
            <input type="text" id="staticIP" name="staticIP" value=")rawliteral" + String(config->staticIP) + R"rawliteral(" pattern="^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$" placeholder="192.168.1.100">
          </div>
          <div class="form-group">
            <label for="gateway">Gateway Address</label>
            <input type="text" id="gateway" name="gateway" value=")rawliteral" + String(config->gateway) + R"rawliteral(" pattern="^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$" placeholder="192.168.1.1">
          </div>
          <div class="form-group">
            <label for="subnet">Subnet Mask</label>
            <input type="text" id="subnet" name="subnet" value=")rawliteral" + String(config->subnet) + R"rawliteral(" pattern="^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$" placeholder="255.255.255.0">
          </div>
          <div class="form-group">
            <label for="dns">DNS Server</label>
            <input type="text" id="dns" name="dns" value=")rawliteral" + String(config->dns) + R"rawliteral(" pattern="^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$" placeholder="8.8.8.8">
          </div>
        </div>
      </div>
      
      <div class="button-group">
        <button type="submit" class="btn btn-primary">💾 Save Configuration</button>
        <button type="button" class="btn btn-secondary" onclick="location.reload()">🔄 Reload</button>
      </div>
    </form>
    
    <div class="footer">
      Version )rawliteral" + VERSION + R"rawliteral( - Duotecno Cloud Proxy © 2025
    </div>
  </div>
  
  <script>
    let connectionDetailsVisible = false;
    let lastConnectionData = null;
    
    function updateStatus() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          document.getElementById('connCount').textContent = data.connectionCount;
          document.getElementById('freeCount').textContent = data.freeConnections;
          document.getElementById('bytesTransferred').textContent = formatBytes(data.bytesTransferred);
          document.getElementById('clientConnections').textContent = data.clientConnections;
          document.getElementById('uptime').textContent = formatUptime(data.uptime);
          document.getElementById('connections').textContent = (data.connectionCount+data.freeConnections) + '  🔍';
          // Store connection data for details view
          lastConnectionData = data.connections;
          if (connectionDetailsVisible) {
            updateConnectionDetails();
          }
        })
        .catch(err => console.error('Status update failed:', err));
    }
    
    function toggleConnectionDetails() {
      connectionDetailsVisible = !connectionDetailsVisible;
      const detailsDiv = document.getElementById('connectionDetails');
      detailsDiv.style.display = connectionDetailsVisible ? 'block' : 'none';
      if (connectionDetailsVisible) {
        updateConnectionDetails();
      }
    }
    
    function updateConnectionDetails() {
      const listDiv = document.getElementById('connectionList');
      if (!lastConnectionData || lastConnectionData.length === 0) {
        listDiv.innerHTML = '<p style="color: #888;">No connections in array</p>';
        return;
      }
      
      let html = '<style>th {padding: 8px; text-align: left} td {padding: 8px; } </style>';
      html += '<table style="width: 100%; border-collapse: collapse;">';
      html += '<tr style="border-bottom: 2px solid #ddd; font-weight: bold;">';
      html += '<th style="">Slot</th>';
      html += '<th>Conn ID</th>';
      html += '<th>Status</th>';
      html += '<th>Cloud Socket</th>';
      html += '<th>Cloud Conn</th>';
      html += '<th>Device Socket</th>';
      html += '<th>Device Conn</th>';
      html += '</tr>';
      
      lastConnectionData.forEach(conn => {
        const statusColor = conn.status === 'FREE' ? '#4CAF50' : '#2196F3';
        html += '<tr style="border-bottom: 1px solid #eee;">';
        html += '<td>' + conn.slot + '</td>';
        html += '<td>#' + conn.id + '</td>';
        html += '<td style="color: ' + statusColor + '; font-weight: bold;">' + conn.status + '</td>';
        html += '<td style="text-align: center">' + (conn.cloudSocket ? '✓' : '✗') + '</td>';
        html += '<td style="text-align: center">' + (conn.cloudConnected ? '✓' : '✗') + '</td>';
        html += '<td style="text-align: center">' + (conn.deviceSocket ? '✓' : '✗') + '</td>';
        html += '<td style="text-align: center">' + (conn.deviceConnected ? '✓' : '✗') + '</td>';
        html += '</tr>';
      });
      
      html += '</table>';
      listDiv.innerHTML = html;
    }
    
    function formatBytes(bytes) {
      if (bytes === 0) return '0 B';
      const k = 1024;
      const sizes = ['B', 'KB', 'MB', 'GB'];
      const i = Math.floor(Math.log(bytes) / Math.log(k));
      return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
    }
    
    function formatUptime(seconds) {
      const days = Math.floor(seconds / 86400);
      const hours = Math.floor((seconds % 86400) / 3600);
      const mins = Math.floor((seconds % 3600) / 60);
      if (days > 0) return days + 'd ' + hours + 'h';
      if (hours > 0) return hours + 'h ' + mins + 'm';
      return mins + 'm';
    }
    
    function toggleStaticIPFields() {
      const useDHCP = document.getElementById('useDHCP').checked;
      const staticIPFields = document.getElementById('staticIPFields');
      staticIPFields.style.display = useDHCP ? 'none' : 'block';
      
      // Update required attribute based on DHCP setting
      const staticIPInputs = staticIPFields.querySelectorAll('input');
      staticIPInputs.forEach(input => {
        if (useDHCP) {
          input.removeAttribute('required');
        } else {
          input.setAttribute('required', 'required');
        }
      });
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

#endif // CONFIGPAGE_H
