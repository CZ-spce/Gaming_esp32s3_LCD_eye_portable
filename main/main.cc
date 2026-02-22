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

#define GIF_FILE_PATH "dog3.gif"

static const char *TAG = "main";
static const char *T_TAG = "SYS_MONITOR";

// 声明互斥锁
SemaphoreHandle_t lvgl_mutex;

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
    lvgl_mutex = xSemaphoreCreateRecursiveMutex(); // 创建递归锁

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
    
    start_manual_gif_display(GIF_FILE_PATH);


    ESP_LOGI(TAG, "5. Enter main loop...");

 // ▼▼▼ 条件编译区开始 ▼▼▼
#if DEBUG_MODE
    xTaskCreate(system_monitor_task, "sys_monitor", 4096, NULL, 1, NULL);
#endif
// ▲▲▲ 条件编译区结束 ▲▲▲

    // 运行 LVGL 任务处理循环
    while (1) {
        // 处理 LVGL 的绘制、动画和输入事件
        if (xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY)) {
            lv_timer_handler();
            xSemaphoreGiveRecursive(lvgl_mutex);
        }


        // 释放 CPU 资源，防止触发看门狗 (Watchdog) 报错
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }

}
