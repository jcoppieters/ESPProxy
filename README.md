# ESP32 Duotecno Proxy

A C++ implementation of the Duotecno cloud proxy for ESP32 with Ethernet, translated from the original TypeScript version.

## 🚀 Quick Start

**Want to deploy this to your ESP32?** See **[DEPLOYMENT.md](DEPLOYMENT.md)** for step-by-step instructions.

## Overview

This proxy server:
- Connects to the Duotecno cloud server and registers with a unique ID
- Maintains a pool of "free" connections ready for incoming clients
- Forwards bidirectional data between cloud clients and local Duotecno devices
- Automatically creates new free connections when clients connect
- Monitors connection health and automatically restarts if needed

## Understanding the Traffic

When monitoring with debug mode enabled (via web interface or `DEBUG_MODE=true`), you'll see two types of heartbeats:

### 1. Proxy ↔ Cloud Heartbeats (Free Connections)
```
[CLOUD] -> [PROXY] (conn #2) Heartbeat request, responding...
```
- Message: `[215,3]` (request) / `[72,3]` (response)
- These happen every ~20 seconds
- Occur on **free connections** (no device attached)
- Keep the cloud connection alive

### 2. Cloud Client ↔ Device Heartbeats (Active Connections)
```
[CLOUD] -> [DEVICE] (conn #1) Forwarding 8 bytes: [215,1]
[DEVICE] -> [CLOUD] (conn #1) Forwarding 8 bytes: [72,1]
```
- Message: `[215,1]` (request) / `[72,1]` (response)
- Can happen frequently (every few seconds)
- Occur on **active connections** (device attached)
- These are NOT proxy heartbeats - they're being forwarded between cloud client and device



## Architecture

```
[ Cloud Server ] ←→ [ ESP32 Proxy ] ←→ [ Duotecno Master Device ]
       ↑                                        ↑
   Port 5097                              Port 5001 (local)
```

## Hardware Requirements

### Supported ESP32 Boards with Ethernet

1. **WT32-ETH01** - ESP32 with built-in Ethernet PHY (LAN8720)
2. **Olimex ESP32-POE** - ESP32 with Power-over-Ethernet
3. **ESP32 + W5500 Ethernet module**
4. **ESP32 + ENC28J60 Ethernet module**

### Wiring

The wiring depends on your specific hardware:

#### WT32-ETH01 (Recommended)
- No additional wiring needed - Ethernet is built-in
- Uses RMII interface with LAN8720 PHY


## Software Requirements

### Arduino IDE Setup

1. **Install Options**
   - Download Arduino IDE** (version 1.8.x or 2.x) from https://www.arduino.cc/en/software
   - Use VSCode with PlatformIO plugin.

2. **Install ESP32 Board Support in Arduino IDE**
   - Open Arduino IDE
   - Go to File → Preferences
   - Add to "Additional Board Manager URLs":
     ```
     https://espressif.github.io/arduino-esp32/package_esp32_index.json
     ```
   - Go to Tools → Board → Boards Manager
   - Search for "esp32" and install "ESP32 by Espressif Systems"

3. **Install Required Libraries for Arduino IDE**
   - Go to Sketch → Include Library → Manage Libraries
   - Install the appropriate Ethernet library for your hardware:
     - **For WT32-ETH01 / Olimex**: No additional library needed (built-in)
     - **For W5500**: Install "Ethernet" by Arduino
     - **For ENC28J60**: Install "EthernetENC" by Jandrassy

### Library Configuration

Depending on your Ethernet hardware, you may need to modify the includes in `ESPProxy.ino` and the library files:

**For W5500 (standard Arduino Ethernet library):**
```cpp
#include <Ethernet.h>  // Already configured
```

**For ENC28J60:**
```cpp
#include <EthernetENC.h>  // Change in all files
```

**For WT32-ETH01 (native ESP32 Ethernet):**
```cpp
#include <ETH.h>
// Requires modifications to use ETH instead of Ethernet class
```

## Installation

1. **Clone or download this project**
   ```
   ESPProxy/
   ├── ESPProxy.ino        (Main Arduino sketch)
   ├── ESPProxy.h          (Header file)
   ├── ESPProxy.cpp        (Implementation)
   └── config.h.example    (Configuration template)
   ```

2. **Create your configuration file**
   ```bash
   cp config.h.example config.h
   ```

3. **Edit `config.h` with your settings**
   ```cpp
   // Network settings
   #define MAC_ADDRESS { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }
   #define LOCAL_IP      192, 168, 1, 177
   #define GATEWAY_IP    192, 168, 1, 1
   
   // Cloud server
   #define CLOUD_SERVER "masters.duotecno.eu"
   #define CLOUD_PORT 5097
   
   // Local device
   #define MASTER_ADDRESS "192.168.1.100"
   #define MASTER_PORT 5001
   
   // Unique identifier
   #define UNIQUE_ID "mydevice.ddns.net:5001"
   
   // Debug mode
   #define DEBUG_MODE false
   ```

4. **Open `ESPProxy.ino` in Arduino IDE**

5. **Select your board**
   - Tools → Board → ESP32 Arduino → (select your board)
   - Common boards:
     - "ESP32 Dev Module" for generic ESP32
     - "WT32-ETH01 Ethernet Module" for WT32-ETH01
     - "OLIMEX ESP32-POE" for Olimex board

6. **Configure board settings**
   - Tools → Upload Speed: 921600
   - Tools → Flash Mode: QIO
   - Tools → Flash Size: 4MB (or your board's size)
   - Tools → Partition Scheme: Default

7. **Select your COM port**
   - Tools → Port → (select your ESP32's port)

8. **Upload the sketch**
   - Click the Upload button (→)

## Configuration

### Network Configuration

#### DHCP (Automatic IP)
Uncomment in `config.h`:
```cpp
#define USE_DHCP
```

#### Static IP
Define your network settings:
```cpp
#define LOCAL_IP      192, 168, 1, 177
#define GATEWAY_IP    192, 168, 1, 1
#define SUBNET_MASK   255, 255, 255, 0
#define DNS_SERVER    192, 168, 1, 1
```

### MAC Address
Change the MAC address to be unique on your network:
```cpp
#define MAC_ADDRESS { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }
```

### Cloud Server
Configure your Duotecno cloud server:
```cpp
#define CLOUD_SERVER "masters.duotecno.eu"
#define CLOUD_PORT 5097
```

### Local Device
Set your Duotecno master device address:
```cpp
#define MASTER_ADDRESS "192.168.1.100"
#define MASTER_PORT 5001
```

### Unique ID
Set a unique identifier for your device (e.g., your no-ip address):
```cpp
#define UNIQUE_ID "myhouse.ddns.net:5001"
```

### Debug Mode
Enable verbose logging:
```cpp
#define DEBUG_MODE true
```

## Usage

1. **Power on the ESP32**
   - Connect via USB or external power
   - Ensure Ethernet cable is connected

2. **Monitor Serial Output**
   - Open Tools → Serial Monitor
   - Set baud rate to 115200
   - You should see:
     ```
     =================================
     ESP32 Duotecno Proxy
     =================================
     
     Initializing Ethernet...
     IP Address: 192.168.1.177
     ...
     Proxy started successfully!
     ```

3. **Verify Connection**
   - Look for: `[PROXY -> CLOUD] Connected to cloud at masters.duotecno.eu:5097`
   - Look for: `[PROXY -> CLOUD] Sent unique ID: mydevice.ddns.net:5001`

4. **Monitor Operation**
   - The proxy will automatically:
     - Maintain free connections to the cloud
     - Accept incoming client connections
     - Forward data bidirectionally
     - Create new free connections as needed

## Troubleshooting

### Ethernet Not Working

1. **Check wiring** - Verify all connections
2. **Check power** - Ensure adequate 3.3V power supply
3. **Check cable** - Try a different Ethernet cable
4. **Check library** - Ensure correct Ethernet library for your hardware

### Cannot Connect to Cloud Server

1. **Check network** - Verify ESP32 has IP address
2. **Ping test** - Try pinging cloud server from your network
3. **DNS** - If using hostname, verify DNS is working
4. **Firewall** - Check if port 5097 is blocked
5. **Unique ID** - Verify your unique ID is registered on cloud server

### Cannot Connect to Local Device

1. **Check IP** - Verify master device IP address
2. **Check port** - Verify master device port (usually 5001)
3. **Network** - Ensure ESP32 and master are on same network
4. **Device status** - Verify master device is powered on

### Frequent Restarts

1. **Power supply** - Ensure stable power (min 500mA for ESP32)
2. **Network** - Check for network interruptions
3. **Timeout** - Increase `CONNECTION_CHECK_INTERVAL` in ESPProxy.h
4. **Memory** - Reduce `MAX_CONNECTIONS` if running out of memory

### Debug Output

Enable debug mode in `config.h`:
```cpp
#define DEBUG_MODE true
```

This will show detailed logging of all operations.

## Advanced Configuration

### Modify Connection Pool Size

In `ESPProxy.h`, change:
```cpp
static const int MAX_CONNECTIONS = 3;  // Increase for more simultaneous clients
```

### Modify Health Check Interval

In `ESPProxy.h`, change:
```cpp
static const unsigned long CONNECTION_CHECK_INTERVAL = 16000;  // milliseconds
```

### Buffer Size

In `ESPProxy.cpp`, modify buffer size if needed:
```cpp
uint8_t buffer[512];  // Increase for larger messages
```

## Protocol Details

### Registration
When connecting to cloud server, proxy sends:
```
[<unique_id>]
```

### Heartbeat
Cloud server sends: `[215,3]`
Proxy responds: `[72,3]`

### Connection Response
Cloud server sends: `[OK]` or `[ERROR-...]`

## Memory Usage

Approximate memory usage:
- **Flash**: ~50-60 KB
- **RAM**: ~15-20 KB + (connections × ~2 KB each)

With 3 connections: ~21-26 KB RAM

## Known Limitations

1. **Maximum connections**: Limited to `MAX_CONNECTIONS` (default 3)
2. **Buffer size**: 512 bytes per message
3. **No SSL/TLS**: Connection to cloud server is not encrypted
4. **No persistent storage**: Configuration is in flash, not EEPROM

## Differences from TypeScript Version

1. **No dynamic restarts**: ESP32 restarts entire proxy, not just process
2. **Fixed connection pool**: TypeScript version has dynamic array
3. **Simplified logging**: No separate log levels beyond debug/info/warning/error
4. **No PM2 support**: No process manager integration
5. **No config file**: Configuration via `config.h` instead of JSON

## License

Same license as the original TypeScript version.

## Support

For issues specific to ESP32 implementation, check:
1. Serial monitor output with DEBUG_MODE enabled
2. Network connectivity (ping test)
3. Ethernet library compatibility
4. Power supply stability

For Duotecno protocol issues, consult original documentation.

## Contributing

Feel free to submit issues or pull requests for improvements.
