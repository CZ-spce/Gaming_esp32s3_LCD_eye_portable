#include "lvgl_tick_timer.h"
#include "esp_timer.h" 
#include "esp_log.h"   
#include "esp_err.h"  
#include "lvgl.h"
static const char *TAG = "lvgl_tick_timer";

// LVGL 心跳定时器回调函数 (提供系统时间)
// -----------------------------------------------------------------
static void lv_tick_task(void *arg) {
    // 告诉 LVGL 过去了 2 毫秒
    lv_tick_inc(2); 
}


void lvgl_tick_init(void)
{
    ESP_LOGI(TAG, "Start LVGL Tick Timer...");
    // 配置并启动 LVGL 心跳定时器 (每 2ms 触发一次)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lv_tick_task,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 2 * 1000)); // 2000 微秒 = 2 毫秒
}
