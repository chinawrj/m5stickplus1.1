/*
 * RGB Color Test Demo for M5StickC Plus 1.1 
 * Four corner color blocks with labels - LANDSCAPE MODE (240x135)
 */

#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "LVGL_DEMO";

/**
 * Four corner RGB color test with labels - landscape mode
 */
void m5stick_lvgl_demo_ui(lv_display_t *disp)
{
    ESP_LOGI(TAG, "Creating landscape RGB color test UI (240x135)...");
    
    // Get the active screen
    lv_obj_t *scr = lv_display_get_screen_active(disp);
    
    // Set black background and clear all default styles
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);        // No border on screen
    lv_obj_set_style_outline_width(scr, 0, LV_PART_MAIN);       // No outline on screen
    lv_obj_set_style_shadow_width(scr, 0, LV_PART_MAIN);        // No shadow on screen
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);             // No padding on screen
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);             // No scrollbar on screen
    
    // Top-left: RED (landscape coordinates)
    lv_obj_t *rect_tl = lv_obj_create(scr);
    lv_obj_set_size(rect_tl, 40, 30);
    lv_obj_set_pos(rect_tl, 5, 5);  // 240x135 landscape mode
    lv_obj_set_style_bg_color(rect_tl, lv_color_hex(0xFF0000), LV_PART_MAIN);  // RED in BGR
    lv_obj_set_style_border_width(rect_tl, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(rect_tl, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(rect_tl, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(rect_tl, 0, LV_PART_MAIN);
    lv_obj_clear_flag(rect_tl, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *label_tl = lv_label_create(scr);
    lv_label_set_text(label_tl, "R");
    lv_obj_set_style_text_color(label_tl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_tl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(label_tl, 8, 38);  // Below red block
    
    // Top-right: GREEN (landscape coordinates)
    lv_obj_t *rect_tr = lv_obj_create(scr);
    lv_obj_set_size(rect_tr, 40, 30);
    lv_obj_set_pos(rect_tr, 195, 5);  // 240-40-5=195
    lv_obj_set_style_bg_color(rect_tr, lv_color_hex(0x0000FF), LV_PART_MAIN);  // GREEN in BGR
    lv_obj_set_style_border_width(rect_tr, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(rect_tr, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(rect_tr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(rect_tr, 0, LV_PART_MAIN);
    lv_obj_clear_flag(rect_tr, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *label_tr = lv_label_create(scr);
    lv_label_set_text(label_tr, "G");
    lv_obj_set_style_text_color(label_tr, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_tr, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(label_tr, 198, 38);  // Below green block
    
    // Bottom-left: BLUE (landscape coordinates)
    lv_obj_t *rect_bl = lv_obj_create(scr);
    lv_obj_set_size(rect_bl, 40, 30);
    lv_obj_set_pos(rect_bl, 5, 100);  // 135-30-5=100
    lv_obj_set_style_bg_color(rect_bl, lv_color_hex(0x00FF00), LV_PART_MAIN);  // BLUE in BGR
    lv_obj_set_style_border_width(rect_bl, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(rect_bl, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(rect_bl, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(rect_bl, 0, LV_PART_MAIN);
    lv_obj_clear_flag(rect_bl, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *label_bl = lv_label_create(scr);
    lv_label_set_text(label_bl, "B");
    lv_obj_set_style_text_color(label_bl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_bl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(label_bl, 8, 82);  // Above blue block
    
    // Bottom-right: WHITE (landscape coordinates)
    lv_obj_t *rect_br = lv_obj_create(scr);
    lv_obj_set_size(rect_br, 40, 30);
    lv_obj_set_pos(rect_br, 195, 100);  
    lv_obj_set_style_bg_color(rect_br, lv_color_hex(0xFFFFFF), LV_PART_MAIN);  // WHITE
    lv_obj_set_style_border_width(rect_br, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(rect_br, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(rect_br, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(rect_br, 0, LV_PART_MAIN);
    lv_obj_clear_flag(rect_br, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *label_br = lv_label_create(scr);
    lv_label_set_text(label_br, "W");
    lv_obj_set_style_text_color(label_br, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_br, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(label_br, 198, 82);  // Above white block
    
    // Center title (landscape coordinates)
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "RGB Test - Landscape");
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_pos(title, 70, 60);  // Center of 240x135 screen
    
    ESP_LOGI(TAG, "Landscape RGB test: 240x135 mode, R=Red, G=Green, B=Blue, W=White");
}