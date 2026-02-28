#include "AnimationPlayer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstring>
#include "jpeg_decoder/my_jpeg_decoder.h"
#include "esp_jpeg_common.h" 

//调试开始宏
#define AnimationPlayer_DEBUG_MODE 0


// 引入你原有的 C 语言解码函数声明
// 假设这些函数在 jpeg_decoder/my_jpeg_decoder.h 和 LCD_gc9a01/my_gc9a01.h 中
extern "C" {
    #include "jpeg_decoder/my_jpeg_decoder.h"
    #include "LCD_gc9a01/my_gc9a01.h"
    // 假设 LCD_H_RES 和 LCD_V_RES 在这里定义，或者你需要手动定义
    #ifndef LCD_H_RES
    #define LCD_H_RES 240
    #endif
    #ifndef LCD_V_RES
    #define LCD_V_RES 240
    #endif
}

static const char* TAG = "AnimPlayer";

AnimationPlayer* AnimationPlayer::s_instance = nullptr; //全局单例

//获取单例
AnimationPlayer* AnimationPlayer::getInstance() {
    if (s_instance == nullptr) {
        s_instance = new AnimationPlayer();
    }
    return s_instance;
}

//构造函数-初始化句柄和状态变量
AnimationPlayer::AnimationPlayer() 
    : m_task_handle(nullptr), 
      m_cmd_queue(nullptr),
      m_is_playing(false),
      m_should_stop(false),
      m_has_new_animation(false) {
}

//创建队列和任务
void AnimationPlayer::begin() {
    // 创建命令队列 (深度 5)
    m_cmd_queue = xQueueCreate(5, sizeof(PlayerCommand));
    
    // 创建播放任务
    // 栈大小建议 4096 或更大，因为涉及字符串操作和文件系统
    //this代表正在调用begin函数的对象
    xTaskCreate(playerTask, "anim_player", 4096, this, 2, &m_task_handle);
    
    ESP_LOGI(TAG, "Animation Player Task Started");
}

//播放任务
void AnimationPlayer::playAnimation(const AnimationConfig& config) {
    if (m_cmd_queue == nullptr) return;
    
    m_pending_config = config;
    
    PlayerCommand cmd = PlayerCommand::CMD_PLAY;
    xQueueSend(m_cmd_queue, &cmd, portMAX_DELAY);
}

//停止播放
void AnimationPlayer::stop() {
    if (m_cmd_queue == nullptr) return;
    PlayerCommand cmd = PlayerCommand::CMD_STOP;
    xQueueSend(m_cmd_queue, &cmd, portMAX_DELAY);
}

//中断并切换动画
void AnimationPlayer::switchAnimation(const AnimationConfig& config) {
    // 切换动画本质上是发送一个新的播放命令，但标记为切换
    m_pending_config = config;
    m_has_new_animation = true;
    
    PlayerCommand cmd = PlayerCommand::CMD_SWITCH_ANIMATION;
    xQueueSend(m_cmd_queue, &cmd, portMAX_DELAY);
}

//底层解码和播放
bool AnimationPlayer::displayFrame(const std::string& path) {
    uint16_t* buffer = nullptr;
    int width = 0, height = 0;

#if AnimationPlayer_DEBUG_MODE
    // 1. 记录开始时间
    int64_t start_time = esp_timer_get_time(); // 单位是微秒 (us)
#endif
    
    // 调用原有的 C 函数解码
    // 注意：确保 decode_jpeg_to_rgb565 在你的工程中是可链接的
    if (decode_jpeg_to_rgb565(path.c_str(), &buffer, &width, &height) == ESP_OK) {
        int x_start = (LCD_H_RES - width) / 2;
        int y_start = (LCD_V_RES - height) / 2;
        int x_end = x_start + width;
        int y_end = y_start + height;
#if AnimationPlayer_DEBUG_MODE
    // 2. 记录结束时间并计算耗时
        int64_t end_time = esp_timer_get_time();
        int64_t duration_us = end_time - start_time;
        float duration_ms = duration_us / 1000.0f; // 转换为毫秒
#endif

        // 调用原有的绘图函数
        my_gc9a01_draw_bitmap(x_start, y_start, x_start + width, y_start + height, buffer);

        // 释放内存
        jpeg_free_align(buffer);

#if AnimationPlayer_DEBUG_MODE
        ESP_LOGI("JPEG", "Displayed %s (%dx%d) in %.2f ms", path.c_str(), width, height, duration_ms);
#endif
        

        return true;
    } else {
        ESP_LOGE(TAG, "Failed to decode: %s", path.c_str());
        return false;
    }
}


//播放任务
void AnimationPlayer::playerTask(void* pvParameters) {
    AnimationPlayer* player = (AnimationPlayer*)pvParameters;
    PlayerCommand cmd;
    
    while (true) {
        // 1. 检查是否有新命令
        if (xQueueReceive(player->m_cmd_queue, &cmd, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (cmd == PlayerCommand::CMD_STOP) {
                player->m_is_playing = false;
                player->m_should_stop = true;
                ESP_LOGI(TAG, "Stop command received");
                continue;
            } 
            else if (cmd == PlayerCommand::CMD_PLAY || cmd == PlayerCommand::CMD_SWITCH_ANIMATION) {
                if (player->m_has_new_animation) {
                    player->m_current_config = player->m_pending_config;
                    player->m_is_playing = true;
                    player->m_should_stop = false;
                    player->m_has_new_animation = false;
                    ESP_LOGI(TAG, "Animation loaded: %s (%d frames)", 
                             player->m_current_config.file_prefix.c_str(), 
                             player->m_current_config.total_frames);
                }
            }
        }

        // 2. 如果正在播放，执行帧循环
        if (player->m_is_playing && !player->m_should_stop) {
            for (int i = 0; i < player->m_current_config.total_frames; ++i) {
                // 再次检查是否被中断
                // if (player->m_should_stop) break;

               if (player->m_should_stop || player->m_has_new_animation) break;


                // 动态生成文件名
                char path_buf[128];
                snprintf(path_buf, sizeof(path_buf), "%s%s%04d%s", 
                         player->m_current_config.base_path.c_str(),
                         player->m_current_config.file_prefix.c_str(),
                         i,
                         player->m_current_config.file_suffix.c_str());
                
                // 显示帧
                player->displayFrame(std::string(path_buf));
                
                // 延时控制帧率
                vTaskDelay(pdMS_TO_TICKS(player->m_current_config.delay_ms));
            }

            // 3. 一轮播放结束后的处理
            if (!player->m_should_stop) {
                if (player->m_current_config.mode == PlayMode::PLAY_LOOP) {
                    ESP_LOGD(TAG, "Looping animation...");
                    // 继续下一轮循环 (for 循环会重新开始)
                    continue; 
                } else {
                    // PLAY_ONCE: 播放一次后停止
                    player->m_is_playing = false;
                    ESP_LOGI(TAG, "Animation finished (Play Once)");
                }
            }
        } else {
            // 空闲时稍微休眠，避免空转占用 CPU
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

