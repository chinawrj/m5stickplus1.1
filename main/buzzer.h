#ifndef BUZZER_H
#define BUZZER_H

#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include <stdint.h>

// M5StickC Plus 1.1 Buzzer Configuration
#define BUZZER_PIN              GPIO_NUM_2
#define BUZZER_LEDC_TIMER       LEDC_TIMER_0
#define BUZZER_LEDC_MODE        LEDC_LOW_SPEED_MODE
#define BUZZER_LEDC_CHANNEL     LEDC_CHANNEL_0
#define BUZZER_LEDC_RESOLUTION  LEDC_TIMER_13_BIT
#define BUZZER_LEDC_FREQUENCY   1000  // Default frequency (Hz)

// Musical note frequencies (Hz) for easy melody creation
#define NOTE_C4     262
#define NOTE_CS4    277
#define NOTE_D4     294
#define NOTE_DS4    311
#define NOTE_E4     330
#define NOTE_F4     349
#define NOTE_FS4    370
#define NOTE_G4     392
#define NOTE_GS4    415
#define NOTE_A4     440
#define NOTE_AS4    466
#define NOTE_B4     494
#define NOTE_C5     523
#define NOTE_CS5    554
#define NOTE_D5     587
#define NOTE_DS5    622
#define NOTE_E5     659
#define NOTE_F5     698
#define NOTE_FS5    740
#define NOTE_G5     784
#define NOTE_GS5    831
#define NOTE_A5     880
#define NOTE_AS5    932
#define NOTE_B5     988
#define NOTE_C6     1047

// Common durations (milliseconds)
#define DURATION_WHOLE      2000
#define DURATION_HALF       1000
#define DURATION_QUARTER    500
#define DURATION_EIGHTH     250
#define DURATION_SIXTEENTH  125

/**
 * @brief Initialize the passive buzzer
 * 
 * This function configures GPIO2 as a PWM output using LEDC peripheral
 * for driving the passive buzzer on M5StickC Plus.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t buzzer_init(void);

/**
 * @brief Deinitialize the buzzer and free resources
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t buzzer_deinit(void);

/**
 * @brief Play a tone at specified frequency
 * 
 * @param frequency Frequency in Hz (20-20000)
 * @param duration Duration in milliseconds (0 = continuous)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t buzzer_tone(uint32_t frequency, uint32_t duration);

/**
 * @brief Stop the buzzer (silence)
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t buzzer_stop(void);

/**
 * @brief Play a simple beep sound
 * 
 * @param frequency Beep frequency in Hz
 * @param duration Duration in milliseconds
 * @return esp_err_t ESP_OK on success
 */
esp_err_t buzzer_beep(uint32_t frequency, uint32_t duration);

/**
 * @brief Play startup melody
 * 
 * Plays a short ascending melody to indicate successful boot.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t buzzer_play_startup(void);

/**
 * @brief Play success melody
 * 
 * Plays a pleasant melody to indicate successful operation.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t buzzer_play_success(void);

/**
 * @brief Play error melody
 * 
 * Plays a warning melody to indicate error or failure.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t buzzer_play_error(void);

/**
 * @brief Play notification melody
 * 
 * Plays a short melody for notifications or alerts.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t buzzer_play_notification(void);

/**
 * @brief Test buzzer functionality
 * 
 * Plays various test patterns to verify buzzer operation:
 * - Frequency sweep
 * - Musical scale
 * - Common melodies
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t buzzer_test_patterns(void);

/**
 * @brief Set buzzer volume (duty cycle)
 * 
 * @param volume Volume level (0-100, where 0=silent, 100=max)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t buzzer_set_volume(uint8_t volume);

#endif // BUZZER_H