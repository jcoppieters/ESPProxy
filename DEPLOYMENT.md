# Deployment Guide - ESP32 Duotecno Proxy

This guide explains how to deploy the ESP32 Duotecno Proxy to an Olimex ESP32-POE board.

## Prerequisites

### Hardware Required
- **Olimex ESP32-POE board** (ESP32 with LAN8720 PHY)
- **POE-capable Ethernet switch** OR **5V power supply** (via GPIO header)
- **Ethernet cable**
- **USB cable** for programming (USB-Serial adapter for the 3-pin UART connector)

### Software Required
- **PlatformIO** (recommended) or Arduino IDE with ESP32 support
- **Git** (optional, for cloning repository)

## Option 1: Using PlatformIO (Recommended)

### Step 1: Install PlatformIO

#### On Windows/Mac/Linux:
1. Install **Visual Studio Code** from https://code.visualstudio.com/
2. Open VS Code and install the **PlatformIO IDE** extension
3. Restart VS Code

#### Or use PlatformIO Core (command line):
```bash
# Install Python 3 first, then:
pip install platformio
```

### Step 2: Get the Code

```bash
# Clone the repository
git clone https://github.com/yourusername/ESPProxy.git
cd ESPProxy

# Or download and extract the ZIP file
```

### Step 3: Configure Your Settings

Edit `include/config.h` with your specific settings:

```cpp
// Network Configuration
#define USE_DHCP false  // Set to true for DHCP, false for static IP

#ifndef USE_DHCP
  #define LOCAL_IP "192.168.0.143"      // Your ESP32 IP address
  #define GATEWAY_IP "192.168.0.1"       // Your router IP
  #define SUBNET_MASK "255.255.255.0"    // Your subnet mask
  #define DNS_SERVER "8.8.8.8"           // DNS server
#endif

// Duotecno Cloud Server
#define CLOUD_SERVER "masters.duotecno.eu"
#define CLOUD_PORT 5097

// Local Duotecno Master Device
#define MASTER_ADDRESS "192.168.0.97"  // Your Duotecno master IP
#define MASTER_PORT 5001

// Unique Identifier (must be unique per installation)
#define UNIQUE_ID "yourname.esp32.net:5001"  // Change this!

// mDNS Hostname (for accessing web interface)
#define MDNS_HOSTNAME "duotecno-cloud"  // Access at http://duotecno-cloud.local

// Debug Mode
#define DEBUG_MODE false  // Set to true for verbose logging

// Connection Settings
#define MAX_CONNECTIONS 10
#define CONNECTION_CHECK_INTERVAL 16000  // Check every 16 seconds

// Version
#define VERSION "1.0.0 R1"
```

**Important**: Change `UNIQUE_ID` to something unique for your installation!

### Step 4: Connect Your ESP32

1. Connect the **USB-Serial adapter** to the ESP32's 3-pin UART header:
   - GND (black) → GND
   - TX (white) → RX
   - RX (green) → TX

2. Connect the USB cable to your computer

3. The device should appear as `/dev/cu.usbserial-*` (Mac) or `COM*` (Windows)

### Step 5: Build and Upload

#### Using VS Code with PlatformIO:
1. Open the ESPProxy folder in VS Code
2. Wait for PlatformIO to initialize
3. Click the **Upload** button (→) in the bottom toolbar
4. Or use the command palette (Cmd/Ctrl+Shift+P) → "PlatformIO: Upload"

#### Using command line:
```bash
# Build the project
platformio run

# Upload to ESP32 (will auto-detect port)
platformio run --target upload

# Or specify the port explicitly
platformio run --target upload --upload-port /dev/cu.usbserial-210

# Upload and monitor serial output
platformio run --target upload && platformio device monitor
```

### Step 6: Monitor and Verify

```bash
# Open serial monitor
platformio device monitor

# Or with specific baud rate
platformio device monitor -b 115200
```

You should see:
```
============================
=== ESP32 Duotecno Proxy ===
=== Version: 1.0.0 R1    ===
============================

[ETH] Initializing Ethernet hardware...
[ETH] Started setup
[ETH] Waiting for IP address...
[ETH] Connected
[ETH] Got IP: 192.168.0.143
[ETH] FULL_DUPLEX, 100Mbps
[INFO] ESP Proxy started successfully!
[INFO] === Web configuration interface ready!
[INFO] === Access at: http://duotecno-cloud.local - http://192.168.0.143
[INFO] === Publishing 'yourname.esp32.net:5001' to: masters.duotecno.eu:5097
[INFO] === Proxy is running on 192.168.0.97:5001...
```

---

## Option 2: Using Arduino IDE

### Step 1: Install Arduino IDE

1. Download Arduino IDE 2.x from https://www.arduino.cc/en/software
2. Install and open Arduino IDE

### Step 2: Install ESP32 Board Support

1. Go to **File → Preferences**
2. In "Additional Board Manager URLs", add:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. Click OK
4. Go to **Tools → Board → Boards Manager**
5. Search for "esp32" and install "**esp32 by Espressif Systems**"

### Step 3: Get the Code and Configure

Same as PlatformIO steps 2-3 above.

### Step 4: Open the Project

1. Navigate to the project folder
2. Open `src/main.cpp` (you may need to rename it to `main.ino` for Arduino IDE)
3. Ensure all `.cpp` and `.h` files are in the same folder

### Step 5: Select Board and Port

1. Go to **Tools → Board → ESP32 Arduino**
2. Select "**ESP32 Dev Module**" or "**OLIMEX ESP32-POE**" if available
3. Go to **Tools → Port** and select your USB-Serial port
4. Set **Tools → Upload Speed** to "460800" (or "115200" if you have issues)

### Step 6: Upload

1. Click the **Upload** button (→)
2. Wait for compilation and upload to complete
3. Open **Tools → Serial Monitor** (set to 115200 baud)

---

## Web Configuration Interface

After successful upload, you can configure the proxy through the web interface:

### Access Methods:
- **Via mDNS**: http://duotecno-cloud.local
- **Via IP**: http://192.168.0.143 (use your configured IP)

### Features:
- **Status Dashboard**: View connection statistics, uptime, data transferred
- **Configuration**: Change cloud server, device IP, unique ID
- **Debug Mode**: Enable/disable verbose logging
- **Restart**: Restart the proxy without re-uploading firmware
- **Connection Details**: Click "Max Connections 🔍" to see detailed connection states

### Configuration Fields:
- **Cloud Server Address**: Default `masters.duotecno.eu`
- **Cloud Server Port**: Default `5097`
- **Master Device Address**: Your local Duotecno device IP
- **Master Device Port**: Default `5001`
- **Unique ID**: Must be unique (format: `name.domain:5001`)
- **Debug Mode**: Enable for troubleshooting

After making changes, click **Save Configuration** and the device will restart with new settings.

---

## Troubleshooting

### Ethernet Not Connecting

**Symptoms**: "Failed to connect to Ethernet" or frequent disconnects

**Solutions**:
1. Check POE power supply is working (or use 5V GPIO power)
2. Try a different Ethernet cable
3. Increase timeout in code if connection is slow
4. Check your network allows the configured static IP
5. Try DHCP mode by setting `#define USE_DHCP true`

### Cannot Connect to Cloud Server

**Symptoms**: No connections created, or immediate disconnects

**Solutions**:
1. Verify `CLOUD_SERVER` and `CLOUD_PORT` are correct
2. Check firewall isn't blocking outgoing connections on port 5097
3. Verify DNS resolution works (try using IP address instead)
4. Enable debug mode to see connection attempts

### Cannot Connect to Local Device

**Symptoms**: "Failed to connect to device" messages

**Solutions**:
1. Verify `MASTER_ADDRESS` is correct and device is reachable
2. Ping the device from your network to verify it's online
3. Check `MASTER_PORT` is correct (usually 5001)
4. Ensure device isn't already connected to another proxy

### Serial Upload Fails

**Solutions**:
1. Check USB-Serial adapter is connected correctly (TX↔RX, RX↔TX)
2. Try holding the **BOOT** button on ESP32 during upload
3. Reduce upload speed to 115200
4. Try a different USB cable or port
5. Check correct serial port is selected

### Web Interface Not Accessible

**Solutions**:
1. Check ESP32 has obtained IP address (check serial monitor)
2. Verify you're on the same network
3. Try IP address instead of mDNS name
4. Check firewall isn't blocking port 80
5. Wait 30 seconds after boot for network to fully initialize

### Debug Mode

To enable detailed logging:
1. Access web interface
2. Check "Enable Debug Mode"
3. Click "Save Configuration"
4. Or edit `config.h` and set `#define DEBUG_MODE true`

Debug output shows:
- All connection events
- Data forwarding details
- Heartbeat messages
- Error conditions

---

## Network Requirements

### Firewall Rules
- **Outgoing**: Allow TCP connections to `masters.duotecno.eu:5097`
- **Incoming**: Allow TCP connections on port 80 (for web interface)

### Port Forwarding
If you want to access the web interface from outside your network:
1. Forward port 80 (or another port) to your ESP32 IP
2. Use strong authentication if exposing to internet
3. Consider using a VPN instead

---

## Updating Firmware

### To Update the Code:
1. Make changes to your local files
2. Connect USB-Serial adapter
3. Run upload command (same as initial deployment)
4. Settings stored in NVRAM will be preserved

### To Factory Reset:
1. Access web interface
2. Clear all configuration fields
3. Click "Save Configuration"
4. Or reflash with default `config.h` values

---

## Support

### Getting Help:
1. Check serial monitor output with debug mode enabled
2. Review the main README.md for architecture details
3. Verify hardware connections and power supply
4. Check network connectivity

### Common Issues:
- **[OK-2] response**: Server rejected connection (too many connections from same ID)
- **Frequent disconnects**: Check POE power quality or cable
- **No heartbeats**: Verify cloud server is reachable
- **Connection timeout**: Increase timeout or check network latency

---

## Files You Need to Modify

**Before deployment, customize these files:**

1. **`include/config.h`** - Main configuration
   - Network settings (IP, gateway, etc.)
   - Cloud server details
   - Local device address
   - **Your unique ID** (most important!)

2. **`platformio.ini`** (optional)
   - Upload port (if not auto-detected)
   - Upload speed
   - Build flags

**Files you should NOT modify** (unless you know what you're doing):
- `src/ESPProxy.cpp` - Core proxy logic
- `src/WebConfig.cpp` - Web interface
- `include/ESPProxy.h` - Class definitions
- `include/ConfigPage.h` - HTML templates

---

## Quick Reference Commands

```bash
# Build only (check for errors)
platformio run

# Upload firmware
platformio run --target upload

# Upload and monitor
platformio run --target upload && platformio device monitor

# Monitor only (view serial output)
platformio device monitor

# Clean build
platformio run --target clean

# Show connected devices
platformio device list
```

---

## Success Checklist

- [ ] PlatformIO or Arduino IDE installed
- [ ] ESP32 board support installed
- [ ] Code downloaded/cloned
- [ ] `config.h` edited with your settings
- [ ] Unique ID configured
- [ ] USB-Serial adapter connected
- [ ] Uploaded successfully
- [ ] Serial monitor shows "ESP Proxy started successfully"
- [ ] Ethernet connected and IP address obtained
- [ ] Web interface accessible
- [ ] Connection to cloud server established
- [ ] Free connection available
- [ ] Test client connection works

---

## Next Steps

After successful deployment:
1. Test connection from a client
2. Monitor statistics in web interface
3. Set up logging/monitoring if needed
4. Document your unique ID and settings
5. Consider backup of configuration

For production use:
- Disable debug mode (reduces serial output)
- Set up monitoring/alerts
- Document network configuration
- Keep firmware version noted
