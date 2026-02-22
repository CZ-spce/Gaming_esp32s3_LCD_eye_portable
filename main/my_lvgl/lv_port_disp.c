/**
 * @file lv_port_disp_templ.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include <stdbool.h>

#include "../LCD_gc9a01/my_gc9a01.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
/*********************
 *      DEFINES
 *********************/
#ifndef MY_DISP_HOR_RES
    #warning Please define or replace the macro MY_DISP_HOR_RES with the actual screen width, default value 320 is used for now.
    #define MY_DISP_HOR_RES    240
#endif

#ifndef MY_DISP_VER_RES
    #warning Please define or replace the macro MY_DISP_VER_RES with the actual screen height, default value 240 is used for now.
    #define MY_DISP_VER_RES    240
#endif

#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
    lv_display_t * disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    lv_display_set_flush_cb(disp, disp_flush);

    // /* Example 1
    //  * One buffer for partial rendering*/
    // LV_ATTRIBUTE_MEM_ALIGN
    // static uint8_t buf_1_1[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];            /*A buffer for 10 rows*/
    // lv_display_set_buffers(disp, buf_1_1, NULL, sizeof(buf_1_1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Example 2
     * Two buffers for partial rendering
     * In flush_cb DMA or similar hardware should be used to update the display in the background.*/
    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t buf_2_1[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];
    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t buf_2_2[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];
    lv_display_set_buffers(disp, buf_2_1, buf_2_2, sizeof(buf_2_1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // /* Example 3
    //  * Two buffers screen sized buffer for double buffering.
    //  * Both LV_DISPLAY_RENDER_MODE_DIRECT and LV_DISPLAY_RENDER_MODE_FULL works, see their comments*/
    // LV_ATTRIBUTE_MEM_ALIGN
    // static uint8_t buf_3_1[MY_DISP_HOR_RES * MY_DISP_VER_RES * BYTE_PER_PIXEL];

    // LV_ATTRIBUTE_MEM_ALIGN
    // static uint8_t buf_3_2[MY_DISP_HOR_RES * MY_DISP_VER_RES * BYTE_PER_PIXEL];
    // lv_display_set_buffers(disp, buf_3_1, buf_3_2, sizeof(buf_3_1), LV_DISPLAY_RENDER_MODE_DIRECT);

    // /* -------------------------------------------------------------
    //  * Example 3 (PSRAM 终极形态)：全屏双缓冲，放飞自我！
    //  * -------------------------------------------------------------*/
    // // 计算单张全屏画面的总字节数 (240 * 240 * 2 = 115200 字节)
    // uint32_t buf_size = MY_DISP_HOR_RES * MY_DISP_VER_RES * BYTE_PER_PIXEL;

    // // 使用 heap_caps_malloc 强制在外部 PSRAM (MALLOC_CAP_SPIRAM) 中分配两块巨大的内存
    // uint8_t *buf_3_1 = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    // uint8_t *buf_3_2 = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    // // 安全检查：如果分配失败，说明 menuconfig 里的 PSRAM 没开成功，或者硬件不支持
    // if (buf_3_1 == NULL || buf_3_2 == NULL) {
    //     ESP_LOGE("LVGL_PORT", "Failed to allocate display buffers in PSRAM! Check menuconfig.");
    //     return; // 直接返回，防止后续空指针死机
    // }

    // ESP_LOGI("LVGL_PORT", "Successfully allocated %lu bytes in PSRAM for display buffers.", buf_size * 2);

    // vTaskDelay(pdMS_TO_TICKS(10)); 

    // lv_display_set_buffers(disp, buf_3_1, buf_3_2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

}
    
/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    /*You code here*/
    my_gc9a01_init();
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_display_flush_ready()' has to be called when it's finished.*/
static void disp_flush(lv_display_t * disp_drv, const lv_area_t * area, uint8_t * px_map)
{
    if(disp_flush_enabled) {
        
        // 计算当前要刷新的区域包含多少个像素
        uint32_t px_cnt = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);

        // ▼▼▼ 添加这一行：交换 RGB565 的高低字节，完美解决花屏噪点 ▼▼▼
        // (这是 LVGL v9 内置的高效字节翻转函数)
        lv_draw_sw_rgb565_swap(px_map, px_cnt);

        // 调用我们的底层驱动，发送到屏幕
        my_gc9a01_draw_bitmap(
            area->x1, 
            area->y1, 
            area->x2 + 1, 
            area->y2 + 1, 
            px_map
        );
    }

    lv_display_flush_ready(disp_drv);
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
