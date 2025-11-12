/*
 * Configuration file for ESP32 Proxy
 * 
 * Copy this file to config.h and edit the values to match your setup
 */

#ifndef CONFIG_H
#define CONFIG_H

#define VERSION "1.0.1 R1"

// ============================================
// Network Configuration
// ============================================

// Set to true to use DHCP, false for static IP
#define USE_DHCP      true

// Static IP Configuration (only used if USE_DHCP is false)
#define LOCAL_IP      "192.168.0.143"
#define GATEWAY_IP    "192.168.0.1"
#define SUBNET_MASK   "255.255.255.0"
#define DNS_SERVER    "8.8.8.8"

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
#define UNIQUE_ID "duotecno.esp32.net:5001"

// Enable debug logging (set to true for verbose output, including data forwarding details)
#define DEBUG_MODE false

// ============================================
// Advanced Configuration
// ============================================

// Maximum number of simultaneous connections
// Increase if you need more simultaneous clients (uses more RAM)
#define MAX_CONNECTIONS 10

// Connection check interval in milliseconds
// How often to check if we need a new free connection
#define CONNECTION_CHECK_INTERVAL 16000  // 16 seconds

// ============================================
// Web Server Configuration
// ============================================

// Web server port
#define WEB_SERVER_PORT 80

// mDNS hostname (access via http://duotecno-cloud.local)
#define MDNS_HOSTNAME "duotecno-cloud"

#endif // CONFIG_H
