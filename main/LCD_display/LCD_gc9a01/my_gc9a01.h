#ifndef __MY_GC9A01_H
#define __MY_GC9A01_H

#include "esp_lcd_gc9a01.h"

// 兼容 C++ 调用
#ifdef __cplusplus
extern "C" {
#endif

// 定义引脚
#define LCD_HOST      SPI2_HOST
#define PIN_NUM_SCLK  12
#define PIN_NUM_MOSI  11
#define PIN_NUM_CS    21
#define PIN_NUM_DC    40
#define PIN_NUM_RST   5

// 屏幕分辨率
#define LCD_H_RES   240
#define LCD_V_RES   240

// RGB565 颜色宏定义（16-bit）
#define GC9A01_BLACK       0x0000    /* 黑色 */
#define GC9A01_WHITE       0xFFFF    /* 白色 */
#define GC9A01_RED         0xF800    /* 红色 */
#define GC9A01_GREEN       0x07E0    /* 绿色 */
#define GC9A01_BLUE        0x001F    /* 蓝色 */
#define GC9A01_CYAN        0x07FF    /* 青色 */
#define GC9A01_MAGENTA     0xF81F    /* 品红 */
#define GC9A01_YELLOW      0xFFE0    /* 黄色 */
#define GC9A01_ORANGE      0xFC00    /* 橙色 */
#define GC9A01_GRAY        0x8410    /* 灰色 */
#define GC9A01_DARKGRAY    0x4208    /* 深灰 */
#define GC9A01_LIGHTGRAY   0xC618    /* 浅灰 */

// 对外暴露的公共接口
void my_gc9a01_init(void);
void my_gc9a01_test_display(void);
void my_gc9a01_draw_string(int16_t x, int16_t y, const char *str, uint16_t color);
void my_gc9a01_clear_screen(uint16_t color);
void my_gc9a01_clear_lines(int16_t y_start, int16_t y_end, uint16_t color);
void my_gc9a01_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *color_data);
#ifdef __cplusplus
}
#endif

#endif // __MY_GC9A01_H
