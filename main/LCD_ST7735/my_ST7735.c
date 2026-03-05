#include <stdlib.h>
#include "esp_log.h"
#include "esp_check.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "my_st7735.h"
#include "esp_lcd_st7735.h" // 引入你提供的底层驱动头文件

static const char *TAG = "my_st7735";

my_st7735_handle_t lcd_handle = NULL;

my_st7735_config_t config = {
        .spi_host    = ST7735_LCD_HOST,        // 修改前缀
        .sclk_io_num = ST7735_PIN_NUM_CLK,     // 修改前缀
        .mosi_io_num = ST7735_PIN_NUM_MOSI,    // 修改前缀
        .cs_io_num   = ST7735_PIN_NUM_CS,      // 修改前缀
        .dc_io_num   = ST7735_PIN_NUM_DC,      // 修改前缀
        .rst_io_num  = ST7735_PIN_NUM_RST,     // 修改前缀
        .bl_io_num   = ST7735_PIN_NUM_BCKL,    // 修改前缀
        .lcd_width   = 128,
        .lcd_height  = 128,
        .offset_x    = 0,    
        .offset_y    = 32,
        .bgr_order   = true,
        .invert_color= true 
    };

// 内部上下文结构体
struct my_st7735_ctx_t {
    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_panel_handle_t panel_handle;
    int bl_io_num;
};

/**
 * @brief ST7735 基础功能测试：分区域填充红、绿、蓝、白
 * 用于验证颜色顺序和显示区域是否完整
 */
void st7735_test_pattern(my_st7735_handle_t handle) {
    const int w = 128;
    const int h = 128; // 根据你的屏幕实际高度调整，ST7735 常见有 128 或 160
    
    // 1. 分配一行像素的缓冲区 (256 字节)，使用 DMA 内存以获得最佳性能
    uint16_t *line_buffer = (uint16_t *)heap_caps_malloc(w * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!line_buffer) {
        ESP_LOGE("TEST", "Failed to alloc line buffer");
        return;
    }

    ESP_LOGI("TEST", "Drawing color bands...");

    // 定义四种测试颜色 (RGB565)
    uint16_t test_colors[] = {
        0xF800, // 红色 (Red)
        0x07E0, // 绿色 (Green)
        0x001F, // 蓝色 (Blue)
        0xFFFF  // 白色 (White)
    };

    for (int i = 0; i < 4; i++) {
        // 填充颜色行缓冲区
        for (int j = 0; j < w; j++) {
            line_buffer[j] = test_colors[i];
        }

        // 绘制屏幕的 1/4 区域
        int y_start = i * (h / 4);
        int y_end = (i + 1) * (h / 4);
        
        // 逐行刷入，这种方式比一次性申请全屏内存更节省系统 RAM
        for (int y = y_start; y < y_end; y++) {
            my_st7735_draw_bitmap(handle, 0, y, w, y + 1, line_buffer);
        }
    }

    heap_caps_free(line_buffer);
    ESP_LOGI("TEST", "Test pattern done.");
}

esp_err_t my_st7735_init(const my_st7735_config_t *config, my_st7735_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(config && out_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    struct my_st7735_ctx_t *ctx = calloc(1, sizeof(struct my_st7735_ctx_t));
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_NO_MEM, TAG, "No memory for ST7735 context");

    ctx->bl_io_num = config->bl_io_num;

    // 1. 初始化 SPI 总线
    spi_bus_config_t buscfg = {
        .sclk_io_num = config->sclk_io_num,
        .mosi_io_num = config->mosi_io_num,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = config->lcd_width * config->lcd_height * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(config->spi_host, &buscfg, SPI_DMA_CH_AUTO));

    // 2. 配置 LCD IO 句柄
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = config->dc_io_num,
        .cs_gpio_num = config->cs_io_num,
        .pclk_hz = 40 * 1000 * 1000, // 40MHz
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)config->spi_host, &io_config, &ctx->io_handle));

    // 3. 配置 LCD 面板句柄 (调用你提供的 esp_lcd_new_panel_st7735)
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = config->rst_io_num,
        .rgb_endian = config->bgr_order ? LCD_RGB_ENDIAN_BGR : LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7735(ctx->io_handle, &panel_config, &ctx->panel_handle));

    // 4. 初始化屏幕
    ESP_ERROR_CHECK(esp_lcd_panel_reset(ctx->panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(ctx->panel_handle));
    
    // 在 my_st7735_init 的 esp_lcd_panel_init(ctx->panel_handle) 之后添加：
    esp_lcd_panel_io_handle_t io = ctx->io_handle;

    // 参考商家例程 0xC0 指令 (Power Control 1)
    esp_lcd_panel_io_tx_param(io, 0xC0, (uint8_t[]){0xA2, 0x02, 0x84}, 3);
    // 参考商家例程 0xC1 指令
    esp_lcd_panel_io_tx_param(io, 0xC1, (uint8_t[]){0xC5}, 1);
    // ... 以此类推，把商家那段长长的 0xE0/0xE1 补进去


    // 设置偏移量 (很多 ST7735 屏幕并非从 0,0 坐标开始物理显存)
    if (config->offset_x != 0 || config->offset_y != 0) {
        esp_lcd_panel_set_gap(ctx->panel_handle, config->offset_x, config->offset_y);
    }
    
    // 设置是否反色 (IPS 屏幕和 TN 屏幕在此配置上通常相反)
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(ctx->panel_handle, config->invert_color));

    // 开启显示
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(ctx->panel_handle, true));

    // 5. 初始化并点亮背光 (如果有)
    if (ctx->bl_io_num >= 0) {
        gpio_config_t bl_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << ctx->bl_io_num
        };
        ESP_ERROR_CHECK(gpio_config(&bl_gpio_config));
        my_st7735_set_backlight(ctx, true);
    }

    *out_handle = ctx;
    ESP_LOGI(TAG, "ST7735 initialized successfully.");
    return ESP_OK;
}

esp_err_t my_st7735_draw_bitmap(my_st7735_handle_t handle, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    if (!handle) return ESP_ERR_INVALID_ARG;
    return esp_lcd_panel_draw_bitmap(handle->panel_handle, x_start, y_start, x_end, y_end, color_data);
}

esp_err_t my_st7735_set_backlight(my_st7735_handle_t handle, bool on)
{
    if (!handle || handle->bl_io_num < 0) return ESP_ERR_INVALID_ARG;
    return gpio_set_level(handle->bl_io_num, on ? 1 : 0);
}



