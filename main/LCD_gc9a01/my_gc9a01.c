#include "my_gc9a01.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"  // 必须引入 IO 层操作
#include "esp_lcd_panel_ops.h" // 必须引入 Panel 绘制/控制操作
#include <stdio.h>

// 全局变量
static spi_device_handle_t spi;
static esp_lcd_panel_handle_t panel_handle;

// 内部函数声明
static void init_spi_bus(void);
static void init_lcd_io(void);
static void init_panel(void);

// 统合初始化函数 (供 main.cc 调用)
void my_gc9a01_init(void)
{
    init_spi_bus();
    init_lcd_io();
    init_panel();
    printf("GC9A01 Initialization completed.\n");
}

// 初始化 SPI 总线
static void init_spi_bus(void)
{
    spi_bus_config_t buscfg = GC9A01_PANEL_BUS_SPI_CONFIG(PIN_NUM_SCLK, PIN_NUM_MOSI, LCD_H_RES * LCD_V_RES * 2);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
}

// 初始化 LCD IO 接口
static void init_lcd_io(void)
{
    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_panel_io_spi_config_t io_config = GC9A01_PANEL_IO_SPI_CONFIG(PIN_NUM_CS, PIN_NUM_DC, NULL, NULL);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    // 创建 GC9A01 面板实例
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_endian = LCD_RGB_ENDIAN_RGB, // 如果显示颜色颠倒，可以尝试改成 LCD_RGB_ENDIAN_BGR
        .bits_per_pixel = 16, //一个像素占用16bit,即2个字节
        .vendor_config = NULL,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
}

// 初始化面板
static void init_panel(void)
{
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    
    //解决 IPS 屏幕的颜色反转问题
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));

    // 关键修正：开启显示，否则屏幕默认黑屏
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
}

// 绘制简单图形进行测试
void my_gc9a01_test_display(void)
{
    uint16_t color = 0xF800; // 测试红色 (RGB565格式)
    
    // GC9A01 是圆屏，分配整屏显存需要注意内存大小
    uint16_t *buffer = (uint16_t *)heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buffer) {
        printf("Failed to allocate frame buffer\n");
        return;
    }

    // 填充整个屏幕为红色
    for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
        buffer[i] = color;
    }

    // 绘制到屏幕上
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, buffer);

    free(buffer);
    printf("Display test completed (Screen should be RED).\n");
}
