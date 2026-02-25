#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"   
#include "esp_err.h"  
#include "esp_heap_caps.h"
#include "esp_timer.h"

// 自定义 GIF 库
#include "gif_encoder/my_gifdec.h"

//jpeg解码
#include "jpeg_decoder/my_jpeg_decoder.h"
#include "esp_jpeg_common.h" 

/* 显示组件 */
#include "lvgl_UI_display.h"
#include "LCD_gc9a01/my_gc9a01.h"
#include "lv_port_disp.h"
#include "lvgl.h"
#include "lvgl_tick_timer.h"


/* 文件系统 */
#include "esp_spiffs.h"
#include "esp_vfs.h"

// ▼▼▼ 调试打印开关：1代表开启，0代表彻底关闭 ▼▼▼
#define DEBUG_MODE 1

#define GIF_FILE_PATH "dizzy.gif"

static const char *TAG = "main";
static const char *T_TAG = "SYS_MONITOR";


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
 
void display_jpeg_on_gc9a01(const char *path)
{
    // 1. 记录开始时间
    int64_t start_time = esp_timer_get_time(); // 单位是微秒 (us)

    uint16_t *buffer = nullptr;
    int width, height;

    if (decode_jpeg_to_rgb565(path, &buffer, &width, &height) == ESP_OK) {
        // 调整显示位置（居中）
        int x_start = (LCD_H_RES - width) / 2;
        int y_start = (LCD_V_RES - height) / 2;
        
        // 2. 记录结束时间并计算耗时
        int64_t end_time = esp_timer_get_time();
        int64_t duration_us = end_time - start_time;
        float duration_ms = duration_us / 1000.0f; // 转换为毫秒


                // 直接调用你的刷新函数
        my_gc9a01_draw_bitmap(x_start, y_start, x_start + width, y_start + height, buffer);

                // 重要：释放缓冲区
        jpeg_free_align(buffer);


        ESP_LOGI("JPEG", "Displayed %s (%dx%d) in %.2f ms", path, width, height, duration_ms);
    } else {
        ESP_LOGE("JPEG", "Failed to decode %s", path);
    }
}

void system_monitor_task(void *pvParameters) {
    // 分配两个缓冲区，或者复用一个足够大的
    char *stats_buffer = (char *)malloc(2048); 
    if (stats_buffer == NULL) {
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        printf("\n================ 任务状态统计 (vTaskList) ================\n");
        printf("任务名称\t状态\t优先级\t剩余栈\t任务编号\n");
        // 1. 获取 状态、优先级、栈等信息
        vTaskList(stats_buffer);
        printf("%s", stats_buffer);

        printf("\n================ CPU 占用统计 (RunTimeStats) ===============\n");
        printf("任务名称\t运行时间\t\t比例\n");
        // 2. 获取 CPU 运行时间百分比
        vTaskGetRunTimeStats(stats_buffer);
        printf("%s", stats_buffer);
        
        printf("----------------------------------------------------------\n");
        // 打印内存状态
        ESP_LOGI(T_TAG, "内部SRAM剩余: %d bytes | PSRAM剩余: %d bytes", 
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

        vTaskDelay(pdMS_TO_TICKS(5000)); 
    }
    free(stats_buffer);
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
    
    // // 获取当前活动屏幕
    // lv_obj_t * scr = lv_screen_active();
    
    // start_manual_gif_display(GIF_FILE_PATH);

    // 核心修改：创建LVGL刷新任务（优先级1，低于解码任务的2）
    // xTaskCreatePinnedToCore(lvgl_refresh_task, "lvgl_refresh", 4096, NULL, 1, NULL, 0);
    ESP_LOGI(TAG, "5. Enter main loop...");



 // ▼▼▼ 条件编译区开始 ▼▼▼
#if DEBUG_MODE
    xTaskCreate(system_monitor_task, "sys_monitor", 4096, NULL, 1, NULL);
#endif
// ▲▲▲ 条件编译区结束 ▲▲▲

    // 运行 LVGL 任务处理循环
    while (1) {
        // 释放 CPU 资源，防止触发看门狗 (Watchdog) 报错
        display_jpeg_on_gc9a01("/spiffs/temp_clean.jpeg");
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }

}

