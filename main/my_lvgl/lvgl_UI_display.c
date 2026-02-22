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

extern SemaphoreHandle_t lvgl_mutex; // 引用 main 中的锁

// ▼▼▼ 调试打印开关：1 开启性能打印，0 关闭 ▼▼▼
#define DEBUG_DECODE_PERF 1

#define RGB888_TO_RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
// 配置参数
#define GIF_RES_W 240
#define GIF_RES_H 240

static uint16_t *g_gif_frame_buf = NULL;
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

    gd_GIF *gif = my_gd_open_gif_buffer(file_data, f_size);
    if (!gif) {
        ESP_LOGE(TAG, "GIF parsing failed");
        heap_caps_free(file_data);
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        int64_t start_us = esp_timer_get_time();
        
        // 1. 解码一帧（纯计算，不需要拿锁）
        int ret = my_gd_get_frame(gif);
        if (ret == 0) {
            my_gd_rewind(gif);
            continue; 
        } else if (ret == -1) {
            ESP_LOGE(TAG, "Decode error occurred");
            break;
        }

        // 2. 指针转换优化
        uint8_t *src = gif->canvas;
        uint16_t *dst = g_gif_frame_buf;
        int pixel_count = GIF_RES_W * GIF_RES_H;
        for (int i = 0; i < pixel_count; i++) {
            *dst++ = RGB888_TO_RGB565(src[0], src[1], src[2]);
            src += 3;
            // 每处理 120 行像素释放一次 CPU，防止长时间占用总线，确保双核并发流畅
            if (i % (GIF_RES_W * 120) == 0) vTaskDelay(1); 
        }

        // 3. 安全刷新 UI：缩小锁的持有时间
        // 尝试获取锁，不建议死等，防止主任务渲染耗时太长导致解码任务卡死
        if (xSemaphoreTakeRecursive(lvgl_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (g_gif_img_obj) {
                lv_obj_invalidate(g_gif_img_obj);
            }
            xSemaphoreGiveRecursive(lvgl_mutex);
        }

        // 4. 动态计算等待时间
        int64_t decode_end_us = esp_timer_get_time();
        int64_t decode_time_ms = (decode_end_us - start_us) / 1000;
        int target_ms = gif->gce.delay * 10;
        if (target_ms < 10) target_ms = 10;
        
        // 强制最小 30ms 延时，确保主渲染任务和 IDLE 任务有足够的执行窗口
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
    // 重要修改：将 GIF 帧缓存申请在内部 SRAM (INTERNAL)，提升访问速度
    g_gif_frame_buf = (uint16_t *)heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!g_gif_frame_buf) {
        ESP_LOGE(TAG, "Failed to allocate Internal SRAM for GIF frame buffer");
        return;
    }

    memset(g_gif_frame_buf, 0, buf_size);

    memset(&g_gif_dsc, 0, sizeof(g_gif_dsc));
    g_gif_dsc.header.w = GIF_RES_W;
    g_gif_dsc.header.h = GIF_RES_H;
    g_gif_dsc.header.cf = LV_COLOR_FORMAT_RGB565; 
    g_gif_dsc.data_size = buf_size;
    g_gif_dsc.data = (const uint8_t *)g_gif_frame_buf;

    g_gif_img_obj = lv_image_create(lv_screen_active());
    lv_image_set_src(g_gif_img_obj, &g_gif_dsc);
    lv_obj_center(g_gif_img_obj);

    // 优先级设定为 2，略高于 main 任务但不过度抢占
    xTaskCreatePinnedToCore(gif_manual_decode_task, "gif_task", 8192, (void*)filename, 1, NULL, 1);
}
