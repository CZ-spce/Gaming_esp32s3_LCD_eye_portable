#include "my_jpeg_decoder.h"
#include "esp_jpeg_dec.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include "esp_err.h"  

static const char *TAG = "jpeg_decoder";



esp_err_t decode_jpeg_to_rgb565(const char *path, uint16_t **out_buffer, int *width, int *height)
{
    // 1. 读取文件
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s", path);
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *jpeg_data = (uint8_t *)malloc(file_size);
    if (!jpeg_data) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    fread(jpeg_data, 1, file_size, f);
    fclose(f);


    // 2. 配置解码器
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_BE; // GC9A01 通常用小端 RGB565

    jpeg_dec_handle_t decoder;
    esp_err_t ret = jpeg_dec_open(&config, &decoder);
    if (ret != JPEG_ERR_OK) {
        free(jpeg_data);
        return ESP_FAIL;
    }

    // 3. 解析头部获取尺寸
    jpeg_dec_io_t io = {
        .inbuf = jpeg_data,
        .inbuf_len = file_size,
    };
    jpeg_dec_header_info_t header_info;
    ret = jpeg_dec_parse_header(decoder, &io, &header_info);
    if (ret != JPEG_ERR_OK) {
        jpeg_dec_close(decoder);
        free(jpeg_data);
        return ESP_FAIL;
    }
    ret = jpeg_dec_parse_header(decoder, &io, &header_info);
    if (ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "Failed to parse header, error code: %d", ret);
        // 错误码 -6 (JPEG_ERR_UNSUPPORT_FMT) 表示格式不支持
        jpeg_dec_close(decoder);
        free(jpeg_data);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Parsed header: %dx%d, subsampling: ?", header_info.width, header_info.height);
    // 注意：esp_new_jpeg 的 header_info 不直接提供采样信息，但尺寸正确说明解析到了 SOF0

    *width = header_info.width;
    *height = header_info.height;

    // 4. 分配输出缓冲区（必须 16 字节对齐！）
    int out_len;
    ret = jpeg_dec_get_outbuf_len(decoder, &out_len);
    if (ret != JPEG_ERR_OK) {
        jpeg_dec_close(decoder);
        free(jpeg_data);
        return ESP_FAIL;
    }

    uint8_t *raw_buffer = (uint8_t *)jpeg_calloc_align(out_len, 16);
    if (!raw_buffer) {
        jpeg_dec_close(decoder);
        free(jpeg_data);
        return ESP_ERR_NO_MEM;
    }

    // 5. 执行解码
    io.outbuf = raw_buffer;
    ret = jpeg_dec_process(decoder, &io);
    if (ret != JPEG_ERR_OK) {
        jpeg_free_align(raw_buffer);
        jpeg_dec_close(decoder);
        free(jpeg_data);
        return ESP_FAIL;
    }

    // 6. 清理并返回
    jpeg_dec_close(decoder);
    free(jpeg_data);
    *out_buffer = (uint16_t *)raw_buffer; // 直接转换为 uint16_t*
    return ESP_OK;
}
