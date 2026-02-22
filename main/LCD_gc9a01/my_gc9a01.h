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

// 对外暴露的公共接口
void my_gc9a01_init(void);
void my_gc9a01_test_display(void);

#ifdef __cplusplus
}
#endif

#endif // __MY_GC9A01_H
