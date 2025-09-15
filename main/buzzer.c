#include "buzzer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

static const char *TAG = "BUZZER";

// Buzzer state
static bool buzzer_initialized = false;
static uint8_t current_volume = 50;  // Default 50% volume

/**
 * @brief Initialize the passive buzzer using LEDC PWM
 */
esp_err_t buzzer_init(void)
{
    ESP_LOGI(TAG, "Initializing passive buzzer on GPIO%d", BUZZER_PIN);
    
    esp_err_t ret = ESP_OK;
    
    // Configure LEDC timer
    ledc_timer_config_t timer_config = {
        .speed_mode = BUZZER_LEDC_MODE,
        .timer_num = BUZZER_LEDC_TIMER,
        .duty_resolution = BUZZER_LEDC_RESOLUTION,
        .freq_hz = BUZZER_LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    
    ret = ledc_timer_config(&timer_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure LEDC channel
    ledc_channel_config_t channel_config = {
        .speed_mode = BUZZER_LEDC_MODE,
        .channel = BUZZER_LEDC_CHANNEL,
        .timer_sel = BUZZER_LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = BUZZER_PIN,
        .duty = 0,  // Start silent
        .hpoint = 0
    };
    
    ret = ledc_channel_config(&channel_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    buzzer_initialized = true;
    ESP_LOGI(TAG, "Passive buzzer initialized successfully");
    
    return ESP_OK;
}

/**
 * @brief Deinitialize the buzzer
 */
esp_err_t buzzer_deinit(void)
{
    if (!buzzer_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing buzzer");
    
    // Stop any ongoing tone
    buzzer_stop();
    
    // Reset GPIO to input mode to reduce power consumption
    gpio_reset_pin(BUZZER_PIN);
    
    buzzer_initialized = false;
    ESP_LOGI(TAG, "Buzzer deinitialized");
    
    return ESP_OK;
}

/**
 * @brief Play a tone at specified frequency
 */
esp_err_t buzzer_tone(uint32_t frequency, uint32_t duration)
{
    if (!buzzer_initialized) {
        ESP_LOGE(TAG, "Buzzer not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Validate frequency range
    if (frequency < 20 || frequency > 20000) {
        ESP_LOGW(TAG, "Frequency %lu Hz out of range (20-20000 Hz)", frequency);
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = ESP_OK;
    
    // Set frequency
    ret = ledc_set_freq(BUZZER_LEDC_MODE, BUZZER_LEDC_TIMER, frequency);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set frequency: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Calculate duty cycle based on volume (50% duty cycle for square wave)
    uint32_t max_duty = (1 << BUZZER_LEDC_RESOLUTION) - 1;
    uint32_t duty = (max_duty * current_volume) / 200;  // 50% max duty * volume%
    
    // Set duty cycle to start the tone
    ret = ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty cycle: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty cycle: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "Playing tone: %lu Hz for %lu ms", frequency, duration);
    
    // If duration is specified, stop after duration
    if (duration > 0) {
        vTaskDelay(pdMS_TO_TICKS(duration));
        buzzer_stop();
    }
    
    return ESP_OK;
}

/**
 * @brief Stop the buzzer
 */
esp_err_t buzzer_stop(void)
{
    if (!buzzer_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop buzzer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
    ESP_LOGD(TAG, "Buzzer stopped");
    
    return ret;
}

/**
 * @brief Play a simple beep
 */
esp_err_t buzzer_beep(uint32_t frequency, uint32_t duration)
{
    return buzzer_tone(frequency, duration);
}

/**
 * @brief Set buzzer volume
 */
esp_err_t buzzer_set_volume(uint8_t volume)
{
    if (volume > 100) {
        ESP_LOGW(TAG, "Volume %d%% clipped to 100%%", volume);
        volume = 100;
    }
    
    current_volume = volume;
    ESP_LOGI(TAG, "Buzzer volume set to %d%%", volume);
    
    return ESP_OK;
}

/**
 * @brief Play startup melody
 */
esp_err_t buzzer_play_startup(void)
{
    if (!buzzer_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Playing startup melody");
    
    // Ascending melody: C4-E4-G4-C5
    buzzer_tone(NOTE_C4, 150);
    vTaskDelay(pdMS_TO_TICKS(50));
    buzzer_tone(NOTE_E4, 150);
    vTaskDelay(pdMS_TO_TICKS(50));
    buzzer_tone(NOTE_G4, 150);
    vTaskDelay(pdMS_TO_TICKS(50));
    buzzer_tone(NOTE_C5, 300);
    
    return ESP_OK;
}

/**
 * @brief Play success melody
 */
esp_err_t buzzer_play_success(void)
{
    if (!buzzer_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Playing success melody");
    
    // Happy melody: C5-E5-G5
    buzzer_tone(NOTE_C5, 200);
    vTaskDelay(pdMS_TO_TICKS(50));
    buzzer_tone(NOTE_E5, 200);
    vTaskDelay(pdMS_TO_TICKS(50));
    buzzer_tone(NOTE_G5, 400);
    
    return ESP_OK;
}

/**
 * @brief Play error melody
 */
esp_err_t buzzer_play_error(void)
{
    if (!buzzer_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Playing error melody");
    
    // Warning melody: Low tone repeated
    for (int i = 0; i < 3; i++) {
        buzzer_tone(NOTE_C4, 200);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    return ESP_OK;
}

/**
 * @brief Play notification melody
 */
esp_err_t buzzer_play_notification(void)
{
    if (!buzzer_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Playing notification melody");
    
    // Simple notification: Two quick beeps
    buzzer_tone(NOTE_A4, 100);
    vTaskDelay(pdMS_TO_TICKS(100));
    buzzer_tone(NOTE_A5, 100);
    
    return ESP_OK;
}

/**
 * @brief Test buzzer functionality
 */
esp_err_t buzzer_test_patterns(void)
{
    if (!buzzer_initialized) {
        ESP_LOGE(TAG, "Buzzer not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting buzzer test patterns");
    
    // Test 1: Frequency sweep
    ESP_LOGI(TAG, "Test 1: Frequency sweep (400Hz - 2000Hz)");
    for (uint32_t freq = 400; freq <= 2000; freq += 200) {
        ESP_LOGI(TAG, "  Playing %lu Hz", freq);
        buzzer_tone(freq, 300);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Test 2: Musical scale (C4 to C5)
    ESP_LOGI(TAG, "Test 2: Musical scale (C4 to C5)");
    uint32_t scale[] = {NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5};
    for (int i = 0; i < 8; i++) {
        ESP_LOGI(TAG, "  Note %d: %lu Hz", i + 1, scale[i]);
        buzzer_tone(scale[i], 250);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Test 3: Volume test
    ESP_LOGI(TAG, "Test 3: Volume levels (25%%, 50%%, 75%%, 100%%)");
    uint8_t volumes[] = {25, 50, 75, 100};
    for (int i = 0; i < 4; i++) {
        buzzer_set_volume(volumes[i]);
        ESP_LOGI(TAG, "  Volume %d%%", volumes[i]);
        buzzer_tone(NOTE_A4, 400);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    // Reset to default volume
    buzzer_set_volume(50);
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Test 4: Predefined melodies
    ESP_LOGI(TAG, "Test 4: Predefined melodies");
    
    ESP_LOGI(TAG, "  Startup melody");
    buzzer_play_startup();
    vTaskDelay(pdMS_TO_TICKS(800));
    
    ESP_LOGI(TAG, "  Success melody");
    buzzer_play_success();
    vTaskDelay(pdMS_TO_TICKS(800));
    
    ESP_LOGI(TAG, "  Notification melody");
    buzzer_play_notification();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "  Error melody");
    buzzer_play_error();
    vTaskDelay(pdMS_TO_TICKS(800));
    
    ESP_LOGI(TAG, "Buzzer test patterns completed successfully");
    
    return ESP_OK;
}