/*
 * Configuration file for ESP32 Proxy
 * 
 * Copy this file to config.h and edit the values to match your setup
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// Network Configuration
// ============================================

// Uncomment to use DHCP instead of static IP
#define USE_DHCP

// Static IP Configuration (used if USE_DHCP is not defined)
#define LOCAL_IP      "192.168.1.177"
#define GATEWAY_IP    "192.168.1.1"
#define SUBNET_MASK   "255.255.255.0"
#define DNS_SERVER    "192.168.1.1"

// ============================================
// Cloud Server Configuration
// ============================================

// Cloud server address (can be hostname or IP)
#define CLOUD_SERVER "masters.duotecno.eu"

// Cloud server port
#define CLOUD_PORT 5097

// ============================================
// Local Device (Master) Configuration
// ============================================

// Local master device IP address
#define MASTER_ADDRESS "192.168.0.97"

// Local master device port
#define MASTER_PORT 5001

// ============================================
// Proxy Configuration
// ============================================

// Unique ID for this device (e.g., your no-ip address + port)
// This identifies your device to the cloud server
#define UNIQUE_ID "johan.esp32.net:5001"

// Enable debug logging (set to true for verbose output)
#define DEBUG_MODE false

// Enable data forwarding logging (prints actual data being forwarded)
#define ENABLE_DATA_LOGGING true

// ============================================
// Advanced Configuration
// ============================================

// Maximum number of simultaneous connections
// Increase if you need more simultaneous clients (uses more RAM)
#define MAX_CONNECTIONS 10

// Connection check interval in milliseconds
// How often to check if we need a new free connection
#define CONNECTION_CHECK_INTERVAL 16000  // 16 seconds

#endif // CONFIG_H
