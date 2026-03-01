#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"   
#include "esp_err.h"  
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "driver/gpio.h"

// 自定义 GIF 库
#include "gif_encoder/my_gifdec.h"

//jpeg解码
#include "jpeg_decoder/my_jpeg_decoder.h"
#include "esp_jpeg_common.h" 
#include "animation_player/AnimationPlayer.h" // 引入新模块

/* 显示组件 */
#include "lvgl_UI_display.h"
#include "LCD_gc9a01/my_gc9a01.h"
#include "lv_port_disp.h"
#include "lvgl.h"
#include "lvgl_tick_timer.h"

/* 动画播放器 */
#include "animation_player/AnimationPlayer.h"
#include "animation_player/AnimationLib.h"

/* 文件系统 */
#include "esp_spiffs.h"
#include "esp_vfs.h"

// ▼▼▼ 调试打印开关：1代表开启，0代表彻底关闭 ▼▼▼
#define DEBUG_MODE 1

#define GIF_FILE_PATH "dizzy.gif"

static const char *TAG = "main";
static const char *T_TAG = "SYS_MONITOR";


// ✅ 配置按键引脚 (ESP32-S3 Eye 板载按键通常是 GPIO0)
// 如果您的按键接在其他引脚，请修改这里
#define BUTTON_GPIO       GPIO_NUM_6
#define BUTTON_ACTIVE_LEVEL 0  // 0: 低电平有效 (按下接地), 1: 高电平有效



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

// ✅ 新增：按键初始化函数
void button_init(void) {
    gpio_config_t io_conf = {};
    // 禁用中断
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // 设置为输入模式
    io_conf.mode = GPIO_MODE_INPUT;
    // 设置引脚
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    // 启用上拉电阻 (防止悬空误触，如果是低电平有效按键)
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    
    gpio_config(&io_conf);
    ESP_LOGI(TAG, "Button initialized on GPIO%d", BUTTON_GPIO);
}
// ✅ 新增：简单的按键检测函数 (带去抖动)
// 返回 true 表示检测到一次有效的按下动作
bool check_button_press(void) {
    static bool last_state = true; // 假设初始为未按下 (高电平)
    static uint32_t press_time = 0;
    
    bool current_state = (gpio_get_level(BUTTON_GPIO) == BUTTON_ACTIVE_LEVEL);
    
    // 如果当前是按下状态
    if (current_state) {
        // 如果之前是松开状态，记录时间
        if (last_state == false) {
            press_time = esp_timer_get_time(); // 记录按下时刻 (微秒)
        }
        
        // 检查是否持续按下了足够长的时间 (去抖动，例如 20ms = 20000us)
        if ((esp_timer_get_time() - press_time) > 20000) {
            last_state = true;
            return true; // 确认是一次有效按下
        }
    } else {
        // 松开了
        last_state = false;
    }
    
    return false;
}

extern "C" void app_main(void)
{

    printf("enter app_main\n");

    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "Initial PSRAM free size: %d bytes", psram_free);

    // 1. 初始化文件系统
    ESP_LOGI(TAG, "Initialize spiffs...");
    init_spiffs();
    
    my_gc9a01_init();
    
    // ✅ 3. 初始化按键
    button_init();


// 创建并启动动画播放器
    AnimationPlayer* player = AnimationPlayer::getInstance();
    player->begin();
    angry.mode = PlayMode::PLAY_LOOP;
    player->switchAnimation(angry);
    // player->setGenderAnimation(GenderMode::WOMEN);
    
     
    // 3. 开始播放
    ESP_LOGI("main", "Starting first animation...");

    

 // ▼▼▼ 条件编译区开始 ▼▼▼
#if DEBUG_MODE
    xTaskCreate(system_monitor_task, "sys_monitor", 4096, NULL, 1, NULL);
#endif


    while (1) {
      // ✅ 检测按键是否被按下
        if (gpio_get_level(BUTTON_GPIO) == BUTTON_ACTIVE_LEVEL) {
            ESP_LOGI(TAG, "🔘 Button Pressed! Switching gender...");
            
            // 调用切换性别函数
            player->switchGenderAnimation();
            vTaskDelay(pdMS_TO_TICKS(1000));
            
        }
        
        // 让出 CPU 时间片，避免看门狗复位 (WDT Reset)
        // 10ms 的延时足够快以响应按键，又不会占用太多 CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
      
}

