#include "gif_encoder/my_gifdec.h"
#include "lvgl_UI_display.h"
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lvgl.h"

static const char *TAG = "UI_GIF";

// ▼▼▼ 调试打印开关：1 开启性能打印，0 关闭 ▼▼▼
#define DEBUG_DECODE_PERF 1

#define RGB888_TO_RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
// 配置参数
#define GIF_RES_W 240
#define GIF_RES_H 240

// 新增一个全局标志位，告诉 LVGL 是否有新图
volatile bool has_new_frame = false;

// 核心修改1：双缓冲区（解码缓冲区 + 显示缓冲区）
static uint16_t *g_gif_decode_buf = NULL;  // 解码任务写入的缓冲区（PSRAM）
static uint16_t *g_gif_display_buf = NULL; // LVGL 显示的缓冲区（内部SRAM）
static lv_obj_t *g_gif_img_obj = NULL;
static lv_image_dsc_t g_gif_dsc;

#define gd_get_pixel_rgb(gif, x, y, r, g, b) \
    do { \
        uint8_t *_c = &(gif)->canvas[((y) * (gif)->width + (x)) * 3]; \
        *(r) = _c[0]; *(g) = _c[1]; *(b) = _c[2]; \
    } while(0)

static void gif_manual_decode_task(void *arg) {
    const char *filename = (const char *)arg;
    char full_path[64];
    snprintf(full_path, sizeof(full_path), "/spiffs/%s", filename);

    ESP_LOGI(TAG, "Opening GIF: %s", full_path);

    FILE *f = fopen(full_path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s", full_path);
        vTaskDelete(NULL);
        return;
    }
    fseek(f, 0, SEEK_END);
    long f_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *file_data = (uint8_t*)heap_caps_malloc(f_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!file_data) {
        ESP_LOGE(TAG, "PSRAM memory allocation failed for file data");
        fclose(f);
        vTaskDelete(NULL);
        return;
    }
    fread(file_data, 1, f_size, f);
    fclose(f);
#if DEBUG_DECODE_PERF
        ESP_LOGI(TAG, "图片申请的内存%ld字节",f_size);
#endif
    gd_GIF *gif = my_gd_open_gif_buffer(file_data, f_size);
    if (!gif) {
        ESP_LOGE(TAG, "GIF parsing failed");
        heap_caps_free(file_data);
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        int64_t start_us = esp_timer_get_time();
        
        int ret = my_gd_get_frame(gif);
        if (ret == 0) { my_gd_rewind(gif); continue; } 
        else if (ret == -1) break;

        // 1. 恢复极速指针转换 (写到 decode_buf)
        uint8_t *src = gif->canvas;
        uint16_t *dst = g_gif_decode_buf;
        int pixel_count = GIF_RES_W * GIF_RES_H;
        for (int i = 0; i < pixel_count; i++) {
            *dst++ = RGB888_TO_RGB565(src[0], src[1], src[2]);
            src += 3;
        }

        // 2. 纯粹的解码转换时间
        int64_t decode_end_us = esp_timer_get_time();
        int64_t decode_time_ms = (decode_end_us - start_us) / 1000;

        // 3. 瞬间同步数据并置标志位
        memcpy(g_gif_display_buf, g_gif_decode_buf, GIF_RES_W * GIF_RES_H * 2);
        has_new_frame = true; // 告诉刷新任务：可以画了！

        // 4. 动态延时控制
        int target_ms = gif->gce.delay * 10;
        if (target_ms < 10) target_ms = 10;
        int wait_ms = target_ms - (int)decode_time_ms;
        if (wait_ms < 10) wait_ms = 10; 

#if DEBUG_DECODE_PERF
        static int64_t last_log_time = 0;
        if (decode_end_us - last_log_time > 1000000) {
            ESP_LOGI(TAG, "GIF Decode: %lld ms | Target: %d ms | Actual Wait: %d ms", 
                     decode_time_ms, target_ms, wait_ms);
            last_log_time = decode_end_us;
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }

    my_gd_close_gif(gif);
    heap_caps_free(file_data);
    vTaskDelete(NULL);
}

void start_manual_gif_display(const char * filename) {
    size_t buf_size = GIF_RES_W * GIF_RES_H * 2;
    // 解码缓冲区：PSRAM（大内存，不占用内部SRAM）
    g_gif_decode_buf = (uint16_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    // 显示缓冲区：内部SRAM（LVGL访问速度快）
    g_gif_display_buf = (uint16_t *)heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!g_gif_decode_buf || !g_gif_display_buf) {
        ESP_LOGE(TAG, "Failed to allocate buffer for GIF");
        if (g_gif_decode_buf) heap_caps_free(g_gif_decode_buf);
        if (g_gif_display_buf) heap_caps_free(g_gif_display_buf);
        return;
    }

    memset(g_gif_decode_buf, 0, buf_size);
    memset(g_gif_display_buf, 0, buf_size);

     // 初始化LVGL图片描述符（指向显示缓冲区）
    memset(&g_gif_dsc, 0, sizeof(g_gif_dsc));
    g_gif_dsc.header.w = GIF_RES_W;
    g_gif_dsc.header.h = GIF_RES_H;
    g_gif_dsc.header.cf = LV_COLOR_FORMAT_RGB565; 
    g_gif_dsc.data_size = buf_size;
    g_gif_dsc.data = (const uint8_t *)g_gif_display_buf;

    g_gif_img_obj = lv_image_create(lv_screen_active());
    lv_image_set_src(g_gif_img_obj, &g_gif_dsc);
    lv_obj_center(g_gif_img_obj);

    // 优先级设定为 2，略高于 main 任务但不过度抢占
    xTaskCreatePinnedToCore(gif_manual_decode_task, "gif_task", 8192, (void*)filename, 2, NULL, 1);
}

// 供main函数调用的LVGL刷新函数
void lvgl_refresh_task(void *arg) {
    while (1) {
        // 只有解码出新的一帧，才允许标记刷新
        if (has_new_frame) {
            if (g_gif_img_obj) {
                lv_obj_invalidate(g_gif_img_obj); 
            }
            has_new_frame = false; // 消费掉这个标志位
        }
        
        // 维持 LVGL 基础心跳（处理动画、定时器等）
        lv_timer_handler(); 
        
        // 稍微加长一点延时，给解码任务更多总线资源
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}
