// globals.cpp - Define all global variables here (once only)
#include "boardconfig.h"

// Debug and WiFi settings
bool debugMode = true;
bool wifiUDPEnabled = false;

// BLE Configuration
const char* telemetryServiceUUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const char* telemetryCharUUID    = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
const char* configCharUUID       = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
const char* fileTransferCharUUID = "6e400005-b5a3-f393-e0a9-e50e24dcca9e";

// System state
bool sdCardAvailable = false;
bool loggingActive = false;
char currentLogFilename[64] = "";

// Performance monitoring
unsigned long lastPacketTime = 0;
unsigned long lastDebugTime = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastPerfReset = 0;

// File transfer deferred operations
volatile bool pendingListFiles = false;
volatile bool pendingStartTransfer = false;
volatile bool pendingDeleteFile = false;
volatile bool pendingCancelTransfer = false;