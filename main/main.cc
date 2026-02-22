#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"   
#include "esp_err.h"  

/* 显示组件 */
#include "LCD_gc9a01/my_gc9a01.h"
#include "lv_port_disp.h"
#include "lvgl.h"
#include "lvgl_tick_timer.h"
#include "lvgl_UI_display.h"
static const char *TAG = "main";


extern "C" void app_main(void)
{
    printf("enter app_main\n");

    ESP_LOGI(TAG, "Initialize LVGL...");
    lv_init();
    lv_port_disp_init();
    
    lvgl_tick_init();

    ESP_LOGI(TAG, "4. Create UI...");
    
    // 获取当前活动屏幕
    lv_obj_t * scr = lv_screen_active();
    
    create_cool_ui(scr);


    ESP_LOGI(TAG, "5. Enter main loop...");
    // 运行 LVGL 任务处理循环
    while (1) {
        // 处理 LVGL 的绘制、动画和输入事件
        lv_timer_handler();
        
        // 释放 CPU 资源，防止触发看门狗 (Watchdog) 报错
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }

}
