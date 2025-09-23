# ESP-NOW Component Guide for M5StickC Plus 1.1

## Overview

The ESP-NOW component in this M5StickC Plus 1.1 project provides a connectionless Wi-Fi communication protocol that allows multiple devices to communicate without connecting to a Wi-Fi router. This implementation is based on the official ESP-IDF ESP-NOW example and has been adapted for the M5StickC Plus hardware platform.

## How ESP-NOW Works

ESP-NOW is a connectionless communication protocol developed by Espressif that enables direct communication between ESP32 devices without the need for a Wi-Fi router. Key characteristics:

- **Connectionless**: No need to establish a connection before sending data
- **Low Power**: Minimal overhead for communication
- **Fast**: Low latency data transmission
- **Peer-to-Peer**: Direct device-to-device communication
- **Broadcast/Unicast**: Supports both broadcast to all devices and unicast to specific devices

### Communication Flow

1. **Device Discovery**: Devices broadcast their presence with a magic number
2. **Role Negotiation**: Devices compare magic numbers to determine sender/receiver roles
3. **Peer Registration**: Devices add each other as peers for unicast communication
4. **Data Exchange**: Encrypted data transmission between peers

## Architecture Components

### Core Files

- **`espnow_manager.h/.c`** - Main ESP-NOW management layer
- **`espnow_example.h`** - Official ESP-IDF example definitions and structures
- **`page_manager_espnow.h/.c`** - LVGL page integration for ESP-NOW status display
- **`Kconfig.projbuild`** - Configuration options and parameters

### Key Data Structures

#### ESP-NOW Statistics (`espnow_stats_t`)
```c
typedef struct {
    uint32_t packets_sent;        // Total packets sent
    uint32_t packets_received;    // Total packets received  
    uint32_t send_success;        // Successful sends
    uint32_t send_failed;         // Failed sends
    uint32_t magic_number;        // Device's magic number
    uint8_t peer_mac[6];         // Connected peer's MAC address
    bool is_sender;              // Device role (sender/receiver)
    bool is_connected;           // Connection status
    uint32_t last_recv_time;     // Last packet receive timestamp
    uint32_t last_seq_num;       // Last received sequence number
} espnow_stats_t;
```

#### ESP-NOW Data Packet (`example_espnow_data_t`)
```c
typedef struct {
    uint8_t type;                 // Broadcast or unicast
    uint8_t state;               // Device state indicator
    uint16_t seq_num;            // Sequence number
    uint16_t crc;                // CRC16 checksum
    uint32_t magic;              // Magic number for device identification
    uint8_t payload[0];          // Actual data payload
} __attribute__((packed)) example_espnow_data_t;
```

## Available Configuration Options

All ESP-NOW options are configurable through `idf.py menuconfig` under "Example Configuration":

### WiFi Mode Configuration
```
CONFIG_ESPNOW_WIFI_MODE_STATION     - Station mode (default)
CONFIG_ESPNOW_WIFI_MODE_STATION_SOFTAP - SoftAP mode
```

### Security Configuration
```
CONFIG_ESPNOW_PMK="pmk1234567890123"    - Primary Master Key (16 bytes)
CONFIG_ESPNOW_LMK="lmk1234567890123"    - Local Master Key (16 bytes)
```

### Communication Parameters
```
CONFIG_ESPNOW_CHANNEL=1           - WiFi channel (0-14, default: 1)
CONFIG_ESPNOW_SEND_COUNT=1000     - Total unicast packets to send (1-65535)
CONFIG_ESPNOW_SEND_DELAY=5000     - Delay between packets in ms (0-65535)
CONFIG_ESPNOW_SEND_LEN=10         - Packet length in bytes (10-1470)
```

### Advanced Features
```
CONFIG_ESPNOW_ENABLE_LONG_RANGE=n    - Enable long range mode (512Kbps/256Kbps)
CONFIG_ESPNOW_ENABLE_POWER_SAVE=n    - Enable power save mode
CONFIG_ESPNOW_WAKE_WINDOW=50         - Wake window in ms (power save mode)
CONFIG_ESPNOW_WAKE_INTERVAL=100      - Wake interval in ms (power save mode)
```

## Current Project Configuration

Based on `sdkconfig`, the current configuration is:
- **WiFi Mode**: SoftAP mode (`CONFIG_ESPNOW_WIFI_MODE_STATION_SOFTAP=y`)
- **Channel**: 1
- **Send Count**: 1000 packets
- **Send Delay**: 5000ms (5 seconds)
- **Packet Length**: 10 bytes
- **Long Range**: Disabled
- **Power Save**: Disabled

## API Functions

### Initialization and Lifecycle
```c
esp_err_t espnow_manager_init(void);        // Initialize ESP-NOW manager
esp_err_t espnow_manager_start(void);       // Start ESP-NOW communication
esp_err_t espnow_manager_stop(void);        // Stop ESP-NOW communication
esp_err_t espnow_manager_deinit(void);      // Deinitialize ESP-NOW manager
```

### Data Operations
```c
esp_err_t espnow_manager_send_test_packet(void);           // Send test packet manually
esp_err_t espnow_manager_get_stats(espnow_stats_t *stats); // Get communication statistics
```

### Page Management Integration
```c
const page_controller_t* get_espnow_page_controller(void); // Get page controller
void espnow_page_notify_data_update(void);                // Notify page of data updates
```

## Device Discovery Mechanism

The implementation includes an automatic device discovery system:

### Discovery Process
1. **Broadcast Phase**: Device sends broadcast packets every 5 seconds with state=1
2. **Magic Number Comparison**: Devices compare magic numbers to determine roles
3. **Role Assignment**: 
   - Higher magic number → Sender role
   - Lower magic number → Receiver role
4. **Peer Registration**: Devices add each other as peers for unicast communication

### Current Role Configuration
The M5StickC Plus is configured as **RECEIVER-ONLY** through magic number logic:
```c
// Force M5StickC Plus to always be receiver
if (false && send_param->unicast == false && send_param->magic >= recv_magic) {
    // Never becomes sender
} else {
    ESP_LOGI(TAG, "📥 M5StickC Plus acting as RECEIVER-ONLY");
    s_stats.is_sender = false;
}
```

## Usage Examples

### Basic Initialization
```c
// Initialize and start ESP-NOW
esp_err_t ret = espnow_manager_init();
if (ret == ESP_OK) {
    ret = espnow_manager_start();
}
```

### Get Communication Statistics
```c
espnow_stats_t stats;
esp_err_t ret = espnow_manager_get_stats(&stats);
if (ret == ESP_OK) {
    printf("Packets sent: %lu\n", stats.packets_sent);
    printf("Packets received: %lu\n", stats.packets_received);
    printf("Is sender: %s\n", stats.is_sender ? "Yes" : "No");
    printf("Magic number: 0x%08lX\n", stats.magic_number);
    if (stats.is_connected) {
        printf("Connected to: %02X:%02X:%02X:%02X:%02X:%02X\n",
               stats.peer_mac[0], stats.peer_mac[1], stats.peer_mac[2],
               stats.peer_mac[3], stats.peer_mac[4], stats.peer_mac[5]);
    }
}
```

### Manual Test Packet
```c
// Trigger immediate discovery broadcast
esp_err_t ret = espnow_manager_send_test_packet();
if (ret == ESP_OK) {
    ESP_LOGI("APP", "Test packet triggered successfully");
} else {
    ESP_LOGE("APP", "Failed to trigger test packet: %s", esp_err_to_name(ret));
}
```

### Integration with LVGL Page System
```c
// Register ESP-NOW page controller
const page_controller_t *espnow_controller = get_espnow_page_controller();

// The page will automatically:
// - Display real-time statistics
// - Show device MAC address
// - Update packet counters
// - Display connection status
// - Show system uptime and memory usage
```

### Button Integration Example
```c
// In button interrupt handler or main loop
if (button_a_pressed()) {
    // Trigger immediate ESP-NOW discovery broadcast
    espnow_manager_send_test_packet();
}

if (button_b_pressed()) {
    // Switch to ESP-NOW status page
    page_manager_switch_to_page(PAGE_ESPNOW);
}
```

### Monitoring ESP-NOW Activity
```c
// Enable debug logging for detailed packet information
esp_log_level_set("ESPNOW_MGR", ESP_LOG_DEBUG);

// Monitor statistics periodically
void monitor_espnow_task(void *pvParameters) {
    espnow_stats_t stats;
    while (1) {
        if (espnow_manager_get_stats(&stats) == ESP_OK) {
            printf("ESP-NOW Status: %s\n", stats.is_connected ? "Connected" : "Discovering");
            printf("Role: %s\n", stats.is_sender ? "Sender" : "Receiver");
            printf("Packets: %lu sent, %lu received\n", stats.packets_sent, stats.packets_received);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));  // Check every 5 seconds
    }
}
```

## LVGL Integration

The ESP-NOW component integrates with the LVGL GUI framework through `page_manager_espnow.c`:

### Display Information
- **System Uptime**: HH:MM:SS format
- **Free Memory**: Available heap in KB
- **ESP-NOW Status**: Connection and role status
- **Packet Statistics**: Sent/received packet counts
- **Device MAC Address**: WiFi MAC address display

### Real-time Updates
The page uses atomic variables and change detection to efficiently update only when data changes:
```c
static atomic_bool g_espnow_data_updated = ATOMIC_VAR_INIT(false);
```

## Callback System

### Send Callback
```c
static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
```
**Functionality:**
- Called when packet transmission completes (success or failure)
- Updates send statistics (success/failed counters)
- Notifies discovery task of completion via `send_completed` flag
- Triggers page manager UI updates
- Logs transmission status with MAC address

**Status Values:**
- `ESP_NOW_SEND_SUCCESS` - Packet sent successfully
- `ESP_NOW_SEND_FAIL` - Transmission failed

### Receive Callback
```c
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
```
**Functionality:**
- Called when any ESP-NOW packet is received
- Validates packet data and calculates CRC
- Parses ESP-NOW data structure (type, state, sequence, magic)
- Logs raw packet data and parsed information for debugging
- Updates connection tracking (peer MAC, last receive time, sequence number)
- Automatically adds new peers to peer list with encryption
- Processes both broadcast and unicast packets
- Updates receive statistics and notifies page manager

**Data Processing:**
1. **Raw Data Logging**: Logs hex dump of received data
2. **Data Parsing**: Extracts type, state, sequence number, and magic number
3. **Payload Extraction**: Logs payload content if present
4. **Peer Management**: Adds unknown peers with LMK encryption
5. **Statistics Update**: Increments receive counters and updates timestamps

## Task Architecture

### Device Discovery Task
- **Purpose**: Sends periodic broadcast packets for device discovery
- **Function**: `device_discovery_task(void *pvParameter)`
- **Interval**: 5 seconds (configurable via `DISCOVERY_INTERVAL_MS`)
- **Priority**: 4
- **Stack Size**: 4096 bytes
- **Features**: 
  - Immediate trigger support via task notifications (`xTaskNotifyWait`)
  - Send completion tracking with timeout (1 second)
  - Automatic retry on failure
  - Incremental sequence numbering
  - Random payload generation for testing

**Operation Flow:**
1. Prepare broadcast data with state=1 and incrementing sequence
2. Send broadcast packet to all devices (`FF:FF:FF:FF:FF:FF`)
3. Wait for send completion callback or timeout
4. Sleep for discovery interval or until immediate trigger
5. Repeat until ESP-NOW is stopped

### Receive-Only Task  
- **Purpose**: Processes incoming ESP-NOW events from the event queue
- **Function**: `espnow_recv_only_task(void *pvParameter)`
- **Priority**: 4
- **Stack Size**: 6144 bytes
- **Features**:
  - Queue-based event processing (`xQueueReceive` with blocking wait)
  - Separate handling for send and receive callbacks
  - Broadcast and unicast packet differentiation
  - Automatic peer addition with encryption
  - Statistics updates and page notifications

**Event Processing:**
- **EXAMPLE_ESPNOW_SEND_CB**: Handles send completion events
- **EXAMPLE_ESPNOW_RECV_CB**: Processes received packet data
- **Error Handling**: Validates data integrity and handles parsing failures

### Task Synchronization
- **Event Queue**: `s_espnow_queue` with size `ESPNOW_QUEUE_SIZE` (6 events)
- **Task Notifications**: Used for immediate discovery trigger requests
- **Atomic Flags**: `send_completed` flag for discovery task coordination
- **Statistics Sharing**: Thread-safe access to global `s_stats` structure

## Data Processing and Validation

### Packet Structure Validation
ESP-NOW packets follow the official ESP-IDF example structure:
```c
typedef struct {
    uint8_t type;         // EXAMPLE_ESPNOW_DATA_BROADCAST (0) or EXAMPLE_ESPNOW_DATA_UNICAST (1)
    uint8_t state;        // Device state (typically 0 or 1)
    uint16_t seq_num;     // Incremental sequence number
    uint16_t crc;         // CRC16 checksum for data integrity
    uint32_t magic;       // Device identification number
    uint8_t payload[0];   // Variable-length payload data
} __attribute__((packed)) example_espnow_data_t;
```

### Data Parsing Function
```c
static int espnow_data_parse(uint8_t *data, uint16_t data_len, uint8_t *state, uint16_t *seq, uint32_t *magic)
```

**Validation Steps:**
1. **Length Check**: Ensures packet is at least as large as header structure
2. **CRC Verification**: Validates data integrity using CRC16
3. **Type Validation**: Checks for valid broadcast/unicast type
4. **Data Extraction**: Safely extracts state, sequence, and magic number

**Return Values:**
- `EXAMPLE_ESPNOW_DATA_BROADCAST` (0) - Valid broadcast packet
- `EXAMPLE_ESPNOW_DATA_UNICAST` (1) - Valid unicast packet  
- `-1` - Invalid packet (CRC failure or malformed data)

### Payload Handling
- **Variable Length**: Payload size = `data_len - sizeof(example_espnow_data_t)`
- **Random Data**: Discovery packets contain random payload for testing
- **Hex Logging**: All payload data is logged in hex format for debugging
- **Maximum Size**: Limited by `CONFIG_ESPNOW_SEND_LEN` (default: 10 bytes total)

## Peer Management

### Automatic Peer Discovery
When a packet is received from a new device:
1. **Peer Existence Check**: `esp_now_is_peer_exist(mac_addr)` 
2. **Peer Registration**: Automatically adds new peers with encryption enabled
3. **Encryption Setup**: Uses `CONFIG_ESPNOW_LMK` for peer-specific encryption
4. **Channel Configuration**: Sets peer to same channel as device (`CONFIG_ESPNOW_CHANNEL`)

### Peer Configuration Structure
```c
esp_now_peer_info_t peer = {
    .channel = CONFIG_ESPNOW_CHANNEL,    // WiFi channel
    .ifidx = ESPNOW_WIFI_IF,            // WiFi interface (STA/AP)
    .encrypt = true,                     // Enable encryption
    .lmk = CONFIG_ESPNOW_LMK,           // Local Master Key (16 bytes)
    .peer_addr = {MAC_ADDRESS}           // 6-byte MAC address
};
```

### Security Features
- **PMK (Primary Master Key)**: Set globally with `esp_now_set_pmk()`
- **LMK (Local Master Key)**: Per-peer encryption key
- **Automatic Key Exchange**: Secure key negotiation between devices
- **Encrypted Communication**: All unicast packets are encrypted by default

## Error Handling

The implementation includes comprehensive error handling:
- **Memory Allocation**: Validates malloc/free operations for all buffers
- **WiFi Initialization**: Checks WiFi stack setup and channel configuration  
- **ESP-NOW Initialization**: Validates ESP-NOW subsystem startup
- **Send/Receive Timeouts**: 1-second timeout for send operations
- **Queue Operations**: Handles queue full/empty conditions gracefully
- **Peer Addition**: Manages peer list overflow and encryption failures
- **CRC Validation**: Detects and rejects corrupted packets
- **Data Parsing**: Validates packet structure and content integrity

## Power Management

When power save mode is enabled (`CONFIG_ESPNOW_ENABLE_POWER_SAVE=y`):
- Device can sleep between wake intervals
- Configurable wake window and interval
- Automatic WiFi disconnected power management
- Suitable for battery-powered applications

## Security Features

ESP-NOW provides encryption through:
- **PMK (Primary Master Key)**: 16-byte master key for initial authentication
- **LMK (Local Master Key)**: 16-byte local key for peer-specific encryption
- **Automatic Key Exchange**: Secure key negotiation between peers

## Troubleshooting

### Common Issues
1. **No Communication**: Check channel configuration and PMK/LMK keys match
2. **Role Issues**: Verify magic number comparison logic
3. **Memory Errors**: Check heap size and buffer allocations
4. **WiFi Conflicts**: Ensure no conflicts with other WiFi operations

### Debug Logging
Enable verbose logging with:
```c
esp_log_level_set("ESPNOW_MGR", ESP_LOG_DEBUG);
```

## Performance Considerations

- **Packet Size**: Optimal range is 10-250 bytes
- **Send Interval**: Minimum 10ms recommended between packets
- **Channel Selection**: Use channels 1, 6, or 11 for best performance
- **Range**: Up to 200m in open space, less in indoor environments
- **Throughput**: Approximately 1Mbps maximum sustained rate

## Integration with M5StickC Plus Features

The ESP-NOW component integrates seamlessly with:
- **Button Controls**: Button A/B can trigger manual send operations
- **LVGL Display**: Real-time status display on TFT screen
- **Power Management**: AXP192 power optimization support
- **LED Indicators**: Status indication through red LED
- **Buzzer Alerts**: Audio feedback for connection events

This ESP-NOW implementation provides a robust, configurable wireless communication solution specifically optimized for the M5StickC Plus 1.1 hardware platform.