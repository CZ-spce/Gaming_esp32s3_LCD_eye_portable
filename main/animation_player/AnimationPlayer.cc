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

AnimationPlayer* AnimationPlayer::s_instance = nullptr;

AnimationPlayer* AnimationPlayer::getInstance() {
    if (s_instance == nullptr) {
        s_instance = new AnimationPlayer();
    }
    return s_instance;
}

AnimationPlayer::AnimationPlayer() 
    : m_task_handle(nullptr), 
      m_cmd_queue(nullptr),
      m_is_playing(false),
      m_should_stop(false),
      m_has_new_animation(false) {
}

void AnimationPlayer::begin() {
    // 创建命令队列 (深度 5)
    m_cmd_queue = xQueueCreate(5, sizeof(PlayerCommand));
    
    // 创建播放任务
    // 栈大小建议 4096 或更大，因为涉及字符串操作和文件系统
    xTaskCreate(playerTask, "anim_player", 4096, this, 2, &m_task_handle);
    
    ESP_LOGI(TAG, "Animation Player Task Started");
}

void AnimationPlayer::playAnimation(const AnimationConfig& config) {
    if (m_cmd_queue == nullptr) return;
    
    m_pending_config = config;
    m_has_new_animation = true;
    
    PlayerCommand cmd = PlayerCommand::CMD_PLAY;
    xQueueSend(m_cmd_queue, &cmd, portMAX_DELAY);
}

void AnimationPlayer::stop() {
    if (m_cmd_queue == nullptr) return;
    PlayerCommand cmd = PlayerCommand::CMD_STOP;
    xQueueSend(m_cmd_queue, &cmd, portMAX_DELAY);
}

void AnimationPlayer::switchAnimation(const AnimationConfig& config) {
    // 切换动画本质上是发送一个新的播放命令，但标记为切换
    m_pending_config = config;
    m_has_new_animation = true;
    
    PlayerCommand cmd = PlayerCommand::CMD_SWITCH_ANIMATION;
    xQueueSend(m_cmd_queue, &cmd, portMAX_DELAY);
}

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
                if (player->m_should_stop) break;
                
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


// void AnimationPlayer::playerTask(void* pvParameters) {
//     AnimationPlayer* player = (AnimationPlayer*)pvParameters;
//     PlayerCommand cmd;
    
//     while (true) {
//         // 1. 尝试接收命令 (阻塞 10ms，兼顾响应速度和 CPU 占用)
//         if (xQueueReceive(player->m_cmd_queue, &cmd, pdMS_TO_TICKS(10)) == pdTRUE) {
            
//             // 🔥 关键修复：收到任何播放相关命令，先重置停止标志
//             player->m_should_stop = false; 

//             if (cmd == PlayerCommand::CMD_STOP) {
//                 player->m_is_playing = false;
//                 ESP_LOGI(TAG, "Stop command received.");
//                 continue;
//             } 
//             else if (cmd == PlayerCommand::CMD_PLAY || cmd == PlayerCommand::CMD_SWITCH_ANIMATION) {
//                 if (player->m_has_new_animation) {
//                     // 🔥 强制加载新配置，无论当前状态如何
//                     player->m_current_config = player->m_pending_config;
//                     player->m_is_playing = true;      // 强制开始播放
//                     player->m_has_new_animation = false;
                    
//                     ESP_LOGI(TAG, "New animation loaded: %s (Frames: %d)", 
//                              player->m_current_config.file_prefix.c_str(),
//                              player->m_current_config.total_frames);
                    
//                     // 注意：不要 break 或 continue，让代码自然向下运行进入播放循环
//                 } else {
//                     ESP_LOGW(TAG, "Play command received but no new animation config pending!");
//                 }
//             }
//         }

//         // 2. 执行播放逻辑
//         if (player->m_is_playing && !player->m_should_stop) {
            
//             // 遍历所有帧
//             for (int i = 0; i < player->m_current_config.total_frames; ++i) {
                
//                 // 🔥 每一帧开始前，再次检查是否有新命令插队 (实现无缝切换)
//                 // 使用 0 延时非阻塞检查
//                 if (xQueueReceive(player->m_cmd_queue, &cmd, 0) == pdTRUE) {
//                     player->m_should_stop = false; // 重置停止标志
                    
//                     if (cmd == PlayerCommand::CMD_STOP) {
//                         player->m_is_playing = false;
//                         break; // 跳出 for 循环
//                     }
//                     else if (cmd == PlayerCommand::CMD_PLAY || cmd == PlayerCommand::CMD_SWITCH_ANIMATION) {
//                         if (player->m_has_new_animation) {
//                             // 🔥 强制切换：更新配置，重置索引，立即开始新动画
//                             player->m_current_config = player->m_pending_config;
//                             player->m_has_new_animation = false;
//                             // 注意：这里 break 后，外层 if 会重新判断 m_is_playing (仍为 true)
//                             // 但我们需要重新开始 for 循环 (i=0)。
//                             // 所以这里 break 是正确的，跳出后会让 for 循环结束，
//                             // 然后外层 while 再次进入，检测到 m_is_playing=true，重新进入 for(i=0...)
//                             ESP_LOGI(TAG, "Switching animation mid-stream...");
//                             break; 
//                         }
//                     }
//                 }

//                 // 显示当前帧
//                 char path_buf[128];
//                 snprintf(path_buf, sizeof(path_buf), "%s%s%04d%s", 
//                          player->m_current_config.base_path.c_str(),
//                          player->m_current_config.file_prefix.c_str(),
//                          i,
//                          player->m_current_config.file_suffix.c_str());
                
//                 player->displayFrame(std::string(path_buf));
                
//                 // 延时
//                 vTaskDelay(pdMS_TO_TICKS(player->m_current_config.delay_ms));
//             } // end for

//             // 3. 一轮播放结束后的处理
//             if (!player->m_should_stop) {
//                 if (player->m_current_config.mode == PlayMode::PLAY_LOOP) {
//                     // 循环模式：直接 continue，重新 for(i=0)
//                     continue; 
//                 } else {
//                     // 🔥 PLAY_ONCE 模式：播完一次，停止
//                     player->m_is_playing = false;
//                     ESP_LOGD(TAG, "Animation finished (PLAY_ONCE). Waiting for next command.");
//                 }
//             }
//         } else {
//             // 空闲状态，稍微休眠
//             vTaskDelay(pdMS_TO_TICKS(50));
//         }
//     }
// }
