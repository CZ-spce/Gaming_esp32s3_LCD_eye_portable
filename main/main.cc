#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* 显示组件 */
#include "LCD_gc9a01/my_gc9a01.h"




extern "C" void app_main(void)
{
    printf("enter app_main\n");

    // 初始化屏幕
    my_gc9a01_init();
    
    // 给一点时间稳定
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 刷纯色测试
    my_gc9a01_test_display();


}
