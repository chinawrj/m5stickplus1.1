#ifndef ESPNOW_MANAGER_H
#define ESPNOW_MANAGER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ESP-NOW statistics structure
 */
typedef struct {
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t send_success;
    uint32_t send_failed;
    uint32_t magic_number;
    uint8_t peer_mac[6];
    bool is_sender;
    bool is_connected;
    uint32_t last_recv_time;    // Timestamp of last received packet
    uint32_t last_seq_num;      // Last received sequence number for restart detection
} espnow_stats_t;

/**
 * @brief Initialize ESP-NOW manager (follows official example pattern)
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t espnow_manager_init(void);

/**
 * @brief Start ESP-NOW communication (official example logic)
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t espnow_manager_start(void);

/**
 * @brief Stop ESP-NOW communication
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t espnow_manager_stop(void);

/**
 * @brief Deinitialize ESP-NOW manager
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t espnow_manager_deinit(void);

/**
 * @brief Get ESP-NOW statistics
 * 
 * @param stats Pointer to statistics structure to fill
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t espnow_manager_get_stats(espnow_stats_t *stats);

/**
 * @brief Send a test packet manually
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t espnow_manager_send_test_packet(void);

#ifdef __cplusplus
}
#endif

#endif // ESPNOW_MANAGER_H