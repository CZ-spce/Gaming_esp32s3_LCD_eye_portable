#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"   
#include "esp_err.h"  
#include "esp_heap_caps.h"

// 自定义 GIF 库
#include "gif_encoder/my_gifdec.h"

/* 显示组件 */
#include "lvgl_UI_display.h"
#include "LCD_gc9a01/my_gc9a01.h"
#include "lv_port_disp.h"
#include "lvgl.h"
#include "lvgl_tick_timer.h"


/* 文件系统 */
#include "esp_spiffs.h"
#include "esp_vfs.h"



static const char *TAG = "main";

void init_spiffs(void) {
    ESP_LOGI("SPIFFS", "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",         // 挂载点：以后读取文件就用 "/spiffs/loading.gif"
      .partition_label = "storage",   // 必须和 partitions.csv 里的名字一致
      .max_files = 5,                 // 最多同时打开的文件数
      .format_if_mount_failed = true  // 如果挂载失败（比如第一次运行）就自动格式化
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE("SPIFFS", "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE("SPIFFS", "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE("SPIFFS", "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE("SPIFFS", "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI("SPIFFS", "Partition size: total: %d, used: %d", total, used);
    }
}

extern "C" void app_main(void)
{
    printf("enter app_main\n");

    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "Initial PSRAM free size: %d bytes", psram_free);

    // 1. 初始化文件系统
    ESP_LOGI(TAG, "Initialize spiffs...");
    init_spiffs();

    ESP_LOGI(TAG, "Initialize LVGL...");
    lv_init();
    lv_port_disp_init();
    
    lvgl_tick_init();

    ESP_LOGI(TAG, "4. Create UI...");
    
    // 获取当前活动屏幕
    lv_obj_t * scr = lv_screen_active();
    
    show_angry_gif();


    ESP_LOGI(TAG, "5. Enter main loop...");

    uint16_t get_PSRAM_count = 0;
    // 运行 LVGL 任务处理循环
    while (1) {
        // 处理 LVGL 的绘制、动画和输入事件
        lv_timer_handler();
        ESP_LOGI(TAG, "running...");
        get_PSRAM_count++;
        if(get_PSRAM_count>100)
        {
            psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
            ESP_LOGI(TAG, "Current PSRAM free size: %d bytes", psram_free);
            get_PSRAM_count = 0;
        }
        

        // 释放 CPU 资源，防止触发看门狗 (Watchdog) 报错
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }

}
