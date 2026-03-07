#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ST7735_LCD_HOST    SPI2_HOST
#define ST7735_PIN_NUM_MISO -1
#define ST7735_PIN_NUM_MOSI 11
#define ST7735_PIN_NUM_CLK  12
#define ST7735_PIN_NUM_CS   21
#define ST7735_PIN_NUM_DC   40
#define ST7735_PIN_NUM_RST  5
#define ST7735_PIN_NUM_BCKL -1



// ST7735 硬件配置参数
typedef struct {
    int spi_host;       // SPI 主机 (例如: SPI2_HOST 或 SPI3_HOST)
    int sclk_io_num;    // SPI 时钟引脚
    int mosi_io_num;    // SPI MOSI 引脚
    int cs_io_num;      // 片选引脚 (CS)
    int dc_io_num;      // 数据/命令控制引脚 (DC)
    int rst_io_num;     // 复位引脚 (RST, 如果不用可设为 -1)
    int bl_io_num;      // 背光引脚 (BL, 如果不用可设为 -1)
    
    int lcd_width;      // 屏幕像素宽度 (例如: 128)
    int lcd_height;     // 屏幕像素高度 (例如: 160)
    int offset_x;       // X 轴偏移 (不同厂家的 ST7735 可能有 1-2 像素偏移)
    int offset_y;       // Y 轴偏移
    
    bool bgr_order;     // 是否使用 BGR 颜色顺序 (通常为 false)
    bool invert_color;  // 是否反色 (根据屏幕实际显示效果调整)
} my_st7735_config_t;


// 先定义类型
typedef struct my_st7735_ctx_t* my_st7735_handle_t;

// 2. 再声明外部变量
extern my_st7735_handle_t lcd_handle;

extern my_st7735_config_t config;

//测试代码
void st7735_test_pattern(my_st7735_handle_t handle);

/**
 * @brief 初始化 ST7735 屏幕
 * * @param config 配置参数指针
 * @param out_handle 输出的设备句柄
 * @return esp_err_t ESP_OK 成功，否则失败
 */
esp_err_t my_st7735_init(const my_st7735_config_t *config, my_st7735_handle_t *out_handle);

/**
 * @brief 在指定区域绘制图像/颜色块
 * * @param handle 设备句柄
 * @param x_start 起始 X 坐标
 * @param y_start 起始 Y 坐标
 * @param x_end 结束 X 坐标 (不包含)
 * @param y_end 结束 Y 坐标 (不包含)
 * @param color_data 颜色数据 (RGB565 格式)
 * @return esp_err_t ESP_OK 成功，否则失败
 */
esp_err_t my_st7735_draw_bitmap(my_st7735_handle_t handle, int x_start, int y_start, int x_end, int y_end, const void *color_data);

/**
 * @brief 控制屏幕背光
 * * @param handle 设备句柄
 * @param on true 打开，false 关闭
 * @return esp_err_t 
 */
esp_err_t my_st7735_set_backlight(my_st7735_handle_t handle, bool on);

void st7735_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *color_data);

#ifdef __cplusplus
}
#endif


