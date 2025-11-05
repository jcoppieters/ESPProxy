import * as net from 'net';
import { spawn } from 'child_process';
import { argv, cwd, execPath } from 'process';

//
// [ device (TCP) ] ←→ [ Proxy = TCP client ] ←→ [ Cloud = TCP server ] ←→ [ browser client ]
//
/*
proxy.ts is a proxy server registering itself to our cloud server, 
on that side the cloudsocket and on the other side connecting to a hardware device.

whenever there comes data from the cloudserver, 
it means that an application wants to talk to the device. 
The connection is given to that application 
and a new connection is set up (ready for use) between the cloudserver and the device.

Now that this works, we want to translate this file into C++ for usage on an ESP32 with ethernet port, 
probably loaded with the Arduino IDE

*/

export interface ProxyConfig {
  cloudServer: string;   // cloud server address
  cloudPort: number;     // cloud server port
  masterAddress: string; // local master address
  masterPort: number;    // local master port
  uniqueId: string;      // unique ID for the device (for now the no-ip address + port)
  debug?: boolean;       // debug mode
  kind?: "pm2" | "gw" | "solo"; 
}

export const kEmptyProxy: ProxyConfig = {
  cloudServer: "masters.duotecno.eu",
  cloudPort: 5097,
  masterAddress: "",
  masterPort: 5001,
  uniqueId: "",         // no unique ID -> don't start the proxy
  debug: false,         // default nodebug mode
  kind: "solo"          // default running under PM2 or other process manager -> don't restart yourself
}

const config = {...kEmptyProxy};

export function setProxyConfig(c?: ProxyConfig): ProxyConfig {
  // set the proxy config
  if (c) {
    config.cloudServer = c.cloudServer || kEmptyProxy.cloudServer;
    config.cloudPort = c.cloudPort || kEmptyProxy.cloudPort;
    config.masterAddress = c.masterAddress || kEmptyProxy.masterAddress;
    config.masterPort = c.masterPort || kEmptyProxy.masterPort;
    config.uniqueId = c.uniqueId || kEmptyProxy.uniqueId;
    config.debug = !!c.debug;
    config.kind = c.kind || kEmptyProxy.kind;
  }
  return config;
}


try {
  const configPath = require.resolve('../config-proxy.json');
  console.log(`[PROXY] Using config file at: ${configPath}`);
  setProxyConfig(require(configPath));

} catch (e) {
  setProxyConfig();
  log("[PROXY] No config-proxy.json found, using default values from homebridge or other.");
}


let connectionChecker: NodeJS.Timeout | null = null;

const gCloudConnections: Array<Context> = [];
const kDebug = config.debug || false; // enable debug mode from config

/////////////
// Logging //
/////////////
function debug(str: string) {
  if (kDebug) {
    console.log(`DEBUG: ${str}`);
  }
}
function warning(str: string) {
  console.warn(`WARNING: ${str}`);
}
function log(str: string) {
  console.log(`LOG: ${str}`);
}
function error(str: string) {
  console.error(`ERROR: **** ${str} ****`);
}


///////////////////////////////
// Start up the proxy server //
///////////////////////////////
cleanStart(true);


export function makeNewCloudConnection(master: string, masterPort: number, server: string, serverPort: number, uniqueId: string, count = 1) {
  // retry making a cloud connection
  function reconnect(err: NodeJS.ErrnoException) {
    error(`[PROXY] → [CLOUD] Failed to connect ${count} times to the Cloud server: ${err.code || err.message}`);
    if (count < 3) {
      setTimeout(() => {
        makeNewCloudConnection(master, masterPort, server, serverPort, uniqueId, count+1);
      }, 1000 * count * count);  // exponential backoff: 1 second, 4 seconds, 9 seconds -> give up
      // we check every 16 seconds for a free connection, if none found -> restart
    }
  }

  const cloudSocket = new net.Socket();
  cloudSocket.once('error', reconnect);

  log(`[PROXY] → [CLOUD] Attempt ${count} to start a new free connection for ${config.uniqueId} to the Cloud at ${server}:${serverPort}`);

  cloudSocket.connect(serverPort, server, () => {
    log(`[PROXY] → [CLOUD] Connected to Cloud at ${server}:${serverPort}`);
    cloudSocket.removeListener('error', reconnect);
    
    // Send unique ID as the first message to identify this proxy/device
    cloudSocket.write(`[${uniqueId}]`);
    debug(`[PROXY] → [CLOUD] Sent unique ID: ${uniqueId}`);

    const context = new Context(cloudSocket, master, masterPort, server, serverPort, uniqueId);
    gCloudConnections.push(context);
    log(`[PROXY] → [CLOUD] New free connection #${gCloudConnections.length} to the Cloud at ${server}:${serverPort}`);
  });
}


////////////////
// Restarting //
////////////////
export function cleanStart(restart: boolean = false) {
  if (gCloudConnections.length) {
    gCloudConnections.forEach(ctx => ctx.cleanupSockets());
    log(`[PROXY] → [CLOUD] Cleaned up all ${gCloudConnections.length} cloud connections.`);

    gCloudConnections.splice(0, gCloudConnections.length); // clear the array
  }

  if (connectionChecker) {
    clearInterval(connectionChecker);
    connectionChecker = null;
  }

  if (restart) {
    if (config.uniqueId) {
      log(`[PROXY] Starting proxy with config: ${JSON.stringify(config, null, 2)}`);
      makeNewCloudConnection(config.masterAddress, config.masterPort, config.cloudServer, config.cloudPort, config.uniqueId);

      // check every 16 second for a free connection
      connectionChecker = setInterval(() => {
        const freeConnection = gCloudConnections.find((ctx: Context) =>
          (!ctx.deviceSocket) && ctx.cloudSocket && (ctx.cloudSocket.readyState === 'open')
        );
        if (freeConnection) {
          debug(`[PROXY] → [CLOUD] Found free connection #${freeConnection.uniqueId} to the Cloud -> we're still ok.`);
        } else {
          error(`[PROXY] → [CLOUD] No free connections available, restarting proxy. ***************************`);
          cleanStart(true);
        }
      }, 16 * 1000);

    } else {
      log(`[PROXY] → [CLOUD] Not restarting, no unique ID configured, not starting the proxy.`);
    }
  } else {
    log(`[PROXY] → [CLOUD] Not restarting, no new cloud connection, just cleaning up.`);
  }
}


export class Context {
  deviceSocket: net.Socket | null;
  master: string;
  masterPort: number;

  cloudSocket: net.Socket | null;
  server: string;
  serverPort: number;

  // unique identifier for this device-gateway combination
  uniqueId: string; 

  constructor(cloudSocket: net.Socket, master: string, masterPort: number, server: string, serverPort: number, uniqueId: string) {
    this.deviceSocket = null;
    this.master = master;
    this.masterPort = masterPort;

    this.cloudSocket = cloudSocket;
    this.server = server;
    this.serverPort = serverPort;

    this.uniqueId = uniqueId;

    this.setupCloudSocket();
  }

  setupCloudSocket() {
    // set up handlers for the cloud socket
    this.cloudSocket.on('data', (data: Buffer) => {
      this.handleDataFromCloud(data);
    });

    this.cloudSocket.on('close', () => {
      warning('[CLOUD] Connection closed');
      if (this.deviceSocket) {
        this.deviceSocket.end();
        this.deviceSocket = null;
      }
    });

    this.cloudSocket.on('error', err => {
      error(`[CLOUD] Error: ${err.message}`);
      if (this.deviceSocket) {
        this.deviceSocket.end();
        this.deviceSocket = null;
      }
    });
  }

  handleDataFromCloud(data: Buffer) {
    debug(`[CLOUD] → [PROXY]: Data from Cloud: ${data.toString()}`);

    if (!this.deviceSocket) {
      // we either have a real client that wants to connect to the device,
      // or we receive a heartbeat from the cloud server
      // or we receive a connection response [OK], [OK-=2], [ERROR-...]

      if (this.isHeartbeatRequest(data)) {
        debug(`[CLOUD] → [PROXY]: Received heartbeat from Cloud, returning response.`);
         // respond to the heartbeat of the cloud server
        this.cloudSocket.write("[72,3]"); // command([Command.HeartbeatStatus, CommandMethod.Heartbeat.server])

      } else if (this.isConnectionResponse(data)) {
        debug(`[CLOUD] → [PROXY]: Received connection response from Cloud: ${data.toString()}`);
        // Handle connection response from the cloud server
        
      } else {
        // There is real incomming data, it's a fresh connection, so: a new client wants to connect to the device
        // 1) create a new (free) connection to the cloud server for the next client
        // 2) connect to the device and send the initial data
        log(`[PROXY] → [CLOUD] New client connection, creating new free connection for this proxy/device: ${config.uniqueId}.`);
        makeNewCloudConnection(this.master, this.masterPort, this.server, this.serverPort, this.uniqueId);
        this.makeDeviceConnection(this.master, this.masterPort, data);
      }

    } else if (this.deviceSocket.readyState === 'open') {
      this.deviceSocket.write(data);
      debug(`[PROXY] → [DEVICE] forwarding data to device: ${data.toString().trim()}`);

    } else {
      warning(`[PROXY] → [DEVICE] Device socket is not open yet, waiting for connection... what to do with the data??`);
    }
  }


  makeDeviceConnection(address: string, port: number, data: Buffer) {
    log(`[PROXY] → [DEVICE] Attempting to connect to local device at ${address}:${port}`);

    if (!this.deviceSocket) {
      this.deviceSocket = new net.Socket();
    
      // Connect to local device and send the data that came in from the cloud
      this.deviceSocket.connect(port, address, () => {
        // Check if socket is still valid (not cleaned up by removeConnection)
        if (!this.deviceSocket) {
          warning(`[PROXY] → [DEVICE] Device socket was cleaned up before connection completed`);
          return;
        }
        
        log(`[PROXY] → [DEVICE] Connected to local device at ${address}:${port}`);

        this.setUpDeviceSocket();

        log(`[PROXY] → [DEVICE] Sending initial message: ${data.toString().trim()}`);
        this.deviceSocket.write(data);
      });

    } else if (this.deviceSocket.readyState === 'open') {
        warning(`[PROXY] → [DEVICE] Device socket already open, sending data directly -- STRANGE !!`);
        this.deviceSocket.write(data);

    } else {
      error(`[PROXY] → [DEVICE] Device socket exists, but is not open yet !!`);
    }
  }

  setUpDeviceSocket() {
    if (!this.deviceSocket) {
      error(`[PROXY] → [DEVICE] Cannot set up device socket - socket is null`);
      return;
    }

    this.deviceSocket.on('data', (data: Buffer) => {
      const message = data.toString('utf-8');
      debug(`[DEVICE] → [PROXY] forwarding data to Cloud: ${message.trim()}`);
      this.cloudSocket.write(data);
    });

    this.deviceSocket.on('close', () => {
      log(`[DEVICE] → [PROXY] Device socket closed`);
      this.removeConnection()  
    });

    this.deviceSocket.on('error', (err) => {
      error(`[DEVICE] → [PROXY] Device socket error: ${err.message}`);
      this.removeConnection()  
    });
  }

  cleanupSockets() {
    // Clean up the device socket
    if (this.deviceSocket) {
      if (!this.deviceSocket.destroyed) {
        this.deviceSocket.removeAllListeners();
        this.deviceSocket.end();
        this.deviceSocket.destroy();
      }
      this.deviceSocket = null;
    }

    // Clean up the cloud socket
    if (this.cloudSocket) {
      if (!this.cloudSocket.destroyed) {
        this.cloudSocket.removeAllListeners();
        this.cloudSocket.end();
        this.cloudSocket.destroy();
      }
      this.cloudSocket = null;
    }
  }

  removeConnection() {
    const index = gCloudConnections.indexOf(this);
    if (index !== -1) gCloudConnections.splice(index, 1);

    this.cleanupSockets();
    log(`[PROXY] → [CLOUD] Closed socket to Cloud and removed context for device ${this.master}`);

    if (gCloudConnections.length === 0) {
        log(`[PROXY] → [CLOUD] No more cloud connections, restarting proxy.`);
      // just to be sure: restart...
      if (config.kind === "pm2") {
        // PM2 should restart us.
        log(`[PROXY] → [CLOUD] PM2 should restart us, exiting now.`);
        setTimeout(() => process.exit(0), 100); 

      } else if (config.kind === "solo"){
        log(`[PROXY] → [CLOUD] Running solo, restarting proxy.`);
        restart();

      } else {
        log(`[PROXY] → [CLOUD] Running under gateway/homebridge, not restarting... not sure what to do here.`);
      }
    }
  }

  
  ///////////////
  // Utilities //
  ///////////////

  // check if it's a heartbeat request from the server
  isHeartbeatRequest(data: Buffer): boolean {
    const msg = data.toString('utf-8').trim();
    return msg === '[215,3]';
    // hearbeatKind = (poll = 1, poll_old = 2, serve = 3)
    // return ((data[0] === 91) &&           // [
    //         (data[1] === 50) &&           // 2
    //         (data[2] === 49) &&           // 1
    //         (data[3] === 53) &&           // 5
    //         (data[4] === 44) &&           // ,
    //         (data[5] === 51) &&           // 3
    //         (data[6] === 93));            // ]
  }

  // check if it's a server response after connecting and sending our uniqueID
  isConnectionResponse(data: Buffer): boolean {
    const message = data.toString();
    
    // Check for various server response patterns
    return (message.startsWith('[OK')) || (message.startsWith('[ERROR'));
  }
}

function restart() {
  console.log('🔄 Restarting...');

  setTimeout(() => {
    spawn(execPath, argv.slice(1), {
      cwd: cwd(),
      detached: true,
      stdio: 'inherit',
    });

    process.exit(0);
  }, 100);
}


// Graceful shutdown handler
function handleShutdown(signal: string) {
  log(`[PROXY] → [SYSTEM] Received ${signal}, shutting down gracefully...`);

  if (connectionChecker) {
    clearInterval(connectionChecker);
    connectionChecker = null;
  }

  // Close all cloud connections
  gCloudConnections.forEach(ctx => ctx.cleanupSockets());
  gCloudConnections.splice(0, gCloudConnections.length);

  log(`[PROXY] → [SYSTEM] All connections closed. Exiting.`);

  if (config.kind === "solo") {
    log(`[PROXY] → [SYSTEM] Restarting proxy in solo mode...`);
    restart();
  } else {
    log(`[PROXY] → [SYSTEM] Exiting without restart.`);
    setTimeout(() => process.exit(0), 100);
  }
}

// Listen for termination signals
process.on('SIGINT', () => handleShutdown('SIGINT'));
process.on('SIGTERM', () => handleShutdown('SIGTERM'));


process.on('SIGINT', (err) => {
  error(`[SYSTEM] received SIGINT`);
  handleShutdown('SIGINT');
});

process.on('SIGTERM', (err) => {
  error(`[SYSTEM] received SIGTERM`);
  handleShutdown('SIGTERM');
});

process.on('uncaughtException', (err) => {
  error(`[SYSTEM] Uncaught Exception: ${err.message}`);
  handleShutdown('uncaughtException');
});

process.on('unhandledRejection', (reason) => {
  error(`[SYSTEM] Unhandled Rejection: ${reason}`);
  handleShutdown('unhandledRejection');
});