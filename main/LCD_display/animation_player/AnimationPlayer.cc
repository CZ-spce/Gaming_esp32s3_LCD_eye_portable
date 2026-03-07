#include "AnimationPlayer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstring>
#include "jpeg_decoder/my_jpeg_decoder.h"
#include "esp_jpeg_common.h" 
#include "LCD_ST7735/my_ST7735.h"


#define AnimationPlayer_LCD_H_RES 240
#define AnimationPlayer_LCD_W_RES 240


//调试开始宏
#define AnimationPlayer_DEBUG_MODE 0

#define AnimationPlayer_gender_MODE 0 //性别模式，选择是否衔接播放 ，0代表衔接

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
      current_gender(GenderMode::MEN), 
      gender_switch(false), 
      m_is_playing(false),
      m_should_stop(false),
      m_has_new_animation(false)
      {
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


// 性别切换并重新触发播放
void AnimationPlayer::switchGenderAnimation(void) {
    // 1. 切换性别枚举
    if(current_gender != GenderMode::Genderless) {
        current_gender = (current_gender == GenderMode::MEN) ? GenderMode::WOMEN : GenderMode::MEN;
    }

    // 2. 将当前配置复制给待处理配置，假装我们收到了一个“新”的动画请求
    m_pending_config = m_current_config;


#if AnimationPlayer_gender_MODE
    m_has_new_animation = true;
#endif
    

    // 3. 必须发送队列命令，唤醒 playerTask 重新加载配置
    if (m_cmd_queue != nullptr) {
        PlayerCommand cmd = PlayerCommand::CMD_SWITCH_ANIMATION;
        xQueueSend(m_cmd_queue, &cmd, 0); 
    }
}

//性别设置函数
void AnimationPlayer::setGenderAnimation(GenderMode gender){
   current_gender = gender;
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
        //进行居中绘图
        int x_start = (AnimationPlayer_LCD_H_RES - width) / 2;
        int y_start = (AnimationPlayer_LCD_W_RES - height) / 2;


#if AnimationPlayer_DEBUG_MODE
    // 2. 记录结束时间并计算耗时
        int64_t end_time = esp_timer_get_time();
        int64_t duration_us = end_time - start_time;
        float duration_ms = duration_us / 1000.0f; // 转换为毫秒
#endif

        // 调用原有的绘图函数
        my_gc9a01_draw_bitmap(x_start, y_start, x_start + width, y_start + height, buffer);//gc9a01屏幕
        // st7735_draw_bitmap(x_start, y_start, x_start + width, y_start + height, buffer);//st7735屏幕
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
    
    static char *current_gernder_alphabet;
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
                    // ESP_LOGI(TAG, "Animation loaded: %s (%d frames)", 
                    //          player->m_current_config.file_prefix.c_str(), 
                    //          player->m_current_config.total_frames);
                }
            }
        }
        
        // 2. 如果正在播放，执行帧循环
        if (player->m_is_playing && !player->m_should_stop) {
            for (int i = 0; i < player->m_current_config.total_frames; ++i) {

               if (player->m_should_stop || player->m_has_new_animation) break;

                // 动态生成文件名
                char path_buf[128];

                current_gernder_alphabet=&player->m_current_config.file_prefix[player->m_current_config.file_prefix.length() - 2];
                if(player->current_gender==GenderMode::MEN &&  *current_gernder_alphabet=='g')
                {
                    *current_gernder_alphabet='b';
                }else if(player->current_gender==GenderMode::WOMEN  &&  *current_gernder_alphabet=='b')
                {
                    *current_gernder_alphabet='g';
                }
                
#if AnimationPlayer_DEBUG_MODE
                ESP_LOGI(TAG, "name:%s", player->m_current_config.file_prefix.c_str());
                ESP_LOGI(TAG, "current gender: %d", static_cast<int>(player->current_gender));
#endif
            
                snprintf(path_buf, sizeof(path_buf), "%s%s%04d%s", 
                         player->m_current_config.base_path.c_str(),
                         player->m_current_config.file_prefix.c_str(),
                         i,
                         player->m_current_config.file_suffix.c_str());
                
#if AnimationPlayer_DEBUG_MODE
                ESP_LOGI(TAG, "star jpeg_decoder...");
#endif
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



// #include "AnimationPlayer.h"
// #include <esp_log.h>
// #include <dirent.h> // 用于探测文件数量 (可选)
// #include <sys/stat.h>
// #include <cstring>
// #include <algorithm>

// static const char* TAG = "AnimPlayer";

// AnimationPlayer::AnimationPlayer() 
//     : m_task_handle(nullptr), 
//       m_is_playing(false), 
//       m_should_stop(false), 
//       m_has_new_animation(false) {
//     m_cmd_queue = xQueueCreate(5, sizeof(PlayerCommand));
//     m_current_config.base_path = "/spiffs";
//     m_pending_config.base_path = "/spiffs";
// }

// AnimationPlayer::~AnimationPlayer() {
//     if (m_cmd_queue) vQueueDelete(m_cmd_queue);
//     if (m_task_handle) vTaskDelete(m_task_handle);
// }

// void AnimationPlayer::begin(const std::string& spiffs_root) {
//     m_current_config.base_path = spiffs_root;
//     m_pending_config.base_path = spiffs_root;
    
//     xTaskCreate(playerTask, "anim_player", 4096, this, 5, &m_task_handle);
// }

// // 【核心实现】切换动画
// void AnimationPlayer::switchAnimation(const std::string& anim_name, Gender gender, int delay_ms, PlayMode mode) {
//     if (m_cmd_queue == nullptr) return;

//     // 1. 准备新配置
//     m_pending_config.anim_name = anim_name;
//     m_pending_config.gender = gender;
//     m_pending_config.delay_ms = delay_ms;
//     m_pending_config.mode = mode;
//     m_pending_config.base_path = m_current_config.base_path;

//     // 2. 自动探测帧数 (简单策略：假设最多 200 帧，或者你可以硬编码)
//     // 这里为了演示，先硬编码一个较大的值，或者你可以实现 detectFrameCount
//     // 实际项目中建议实现文件扫描来确认总帧数，避免播放到不存在的文件报错
//     m_pending_config.total_frames = 100; // 默认值，稍后在播放循环中做存在性检查会更稳健
    
//     // 3. 标记有新动画
//     m_has_new_animation = true;

//     // 4. 发送切换命令
//     PlayerCommand cmd = PlayerCommand::CMD_SWITCH_ANIMATION;
//     // 使用最高优先级发送，确保不被阻塞太久 (队列满时等待 10ms)
//     if (xQueueSend(m_cmd_queue, &cmd, pdMS_TO_TICKS(10)) != pdTRUE) {
//         ESP_LOGW(TAG, "Failed to send switch command, queue full?");
//     } else {
//         ESP_LOGI(TAG, "Switch requested: %s_%c (Delay: %dms)", 
//                  anim_name.c_str(), 
//                  (gender == Gender::MALE ? 'b' : 'g'), 
//                  delay_ms);
//     }
// }

// // 【便捷功能】仅切换性别
// void AnimationPlayer::toggleGender() {
//     if (!m_is_playing) return;
    
//     // 保持当前动作，反转性别
//     Gender new_gender = (m_current_config.gender == Gender::MALE) ? Gender::FEMALE : Gender::MALE;
//     switchAnimation(m_current_config.anim_name, new_gender, m_current_config.delay_ms, m_current_config.mode);
// }

// void AnimationPlayer::stop() {
//     if (m_cmd_queue) {
//         PlayerCommand cmd = PlayerCommand::CMD_STOP;
//         xQueueSend(m_cmd_queue, &cmd, portMAX_DELAY);
//     }
// }

// // 简单的文件存在性检查 (替代硬编码总帧数)
// // 返回 false 表示文件不存在，播放结束或跳过
// bool AnimationPlayer::displayFrame(const std::string& full_path) {
//     FILE* f = fopen(full_path.c_str(), "r");
//     if (f == nullptr) {
//         return false; // 文件不存在
//     }
//     fclose(f);

//     // 这里调用你原有的 JPEG 解码和 LCD 显示代码
//     // ... (保留你原有的 decode_jpeg 和 lcd_draw 逻辑)
//     // 示例伪代码：
//     // jpeg_decode_and_draw(full_path.c_str());
    
//     ESP_LOGV(TAG, "Displaying: %s", full_path.c_str());
    
//     // TODO: 把你的实际显示代码填在这里
//     // 假设你有一个函数 void drawJpeg(const char* path);
//     // drawJpeg(full_path.c_str());
    
//     return true;
// }

// void AnimationPlayer::playerTask(void* pvParameters) {
//     AnimationPlayer* player = (AnimationPlayer*)pvParameters;
//     PlayerCommand cmd;

//     while (true) {
//         // --- 1. 检查命令队列 (带超时，兼顾响应与低功耗) ---
//         if (xQueueReceive(player->m_cmd_queue, &cmd, pdMS_TO_TICKS(20)) == pdTRUE) {
//             if (cmd == PlayerCommand::CMD_STOP) {
//                 player->m_is_playing = false;
//                 player->m_should_stop = true;
//                 ESP_LOGI(TAG, "Stopped.");
//                 continue;
//             } 
//             else if (cmd == PlayerCommand::CMD_PLAY || cmd == PlayerCommand::CMD_SWITCH_ANIMATION) {
//                 if (player->m_has_new_animation) {
//                     player->m_current_config = player->m_pending_config;
//                     player->m_is_playing = true;
//                     player->m_should_stop = false;
//                     player->m_has_new_animation = false;
                    
//                     // 如果是切换，重置状态，下一轮循环直接从第 0 帧开始
//                     ESP_LOGI(TAG, "Animation applied: %s", player->m_current_config.getPrefix().c_str());
//                     continue; 
//                 }
//             }
//         }

//         // --- 2. 播放循环 ---
//         if (player->m_is_playing && !player->m_should_stop) {
//             const auto& cfg = player->m_current_config;
//             std::string prefix = cfg.getPrefix(); // 例如 "angry_b_"
            
//             int frame_idx = 0;
//             while (!player->m_should_stop) {
//                 // 【关键优化】每帧开始前再次快速检查队列，实现无缝中断
//                 if (xQueueReceive(player->m_cmd_queue, &cmd, 0) == pdTRUE) {
//                     if (cmd == PlayerCommand::CMD_STOP) {
//                         player->m_should_stop = true;
//                         break;
//                     }
//                     if (cmd == PlayerCommand::CMD_SWITCH_ANIMATION || cmd == PlayerCommand::CMD_PLAY) {
//                         if (player->m_has_new_animation) {
//                             player->m_current_config = player->m_pending_config;
//                             player->m_has_new_animation = false;
//                             player->m_should_stop = false; // 确保不停止
//                             ESP_LOGI(TAG, "Mid-stream switch!");
                            
//                             // 重置计数器，立即开始新动画
//                             frame_idx = -1; 
//                             // 更新 cfg 引用 (因为 m_current_config 变了)
//                             // 注意：在 C++ 中引用不会自动更新指向新对象，我们需要重新获取或直接用成员
//                             // 这里为了简单，我们让外层 while 循环继续，frame_idx=-1 后 ++ 变为 0
//                         }
//                     }
//                 }
                
//                 if (player->m_should_stop) break;

//                 // 构建完整路径: /spiffs/angry_b_0000.jpg
//                 char path_buf[128];
//                 snprintf(path_buf, sizeof(path_buf), "%s/%s%04d.jpg", 
//                          cfg.base_path.c_str(), 
//                          prefix.c_str(), 
//                          frame_idx);

//                 // 尝试显示
//                 if (!player->displayFrame(std::string(path_buf))) {
//                     // 文件不存在，可能是到了最后一帧
//                     ESP_LOGD(TAG, "Frame %d not found, end of animation.", frame_idx);
                    
//                     if (cfg.mode == PlayMode::PLAY_ONCE) {
//                         player->m_is_playing = false;
//                         break;
//                     } else {
//                         // 循环播放：重置为 0
//                         frame_idx = 0;
//                         // 再次检查是否在这一瞬间有切换命令
//                         continue; 
//                     }
//                 }

//                 frame_idx++;
                
//                 // 限制最大帧数防止死循环 (如果文件命名不连续)
//                 if (frame_idx > 500) { 
//                     ESP_LOGW(TAG, "Reached max frames limit, stopping/looping.");
//                     if (cfg.mode == PlayMode::PLAY_ONCE) {
//                         player->m_is_playing = false;
//                         break;
//                     }
//                     frame_idx = 0;
//                 }

//                 // --- 3. 智能延时 (可被中断) ---
//                 int delay_left = cfg.delay_ms;
//                 while (delay_left > 0 && !player->m_should_stop) {
//                     int step = (delay_left > 10) ? 10 : delay_left;
//                     vTaskDelay(pdMS_TO_TICKS(step));
//                     delay_left -= step;
                    
//                     // 快速轮询停止/切换标志
//                     if (xQueueReceive(player->m_cmd_queue, &cmd, 0) == pdTRUE) {
//                          if (cmd == PlayerCommand::CMD_STOP) {
//                              player->m_should_stop = true;
//                              break;
//                          }
//                          if (cmd == PlayerCommand::CMD_SWITCH_ANIMATION) {
//                              if (player->m_has_new_animation) {
//                                  player->m_current_config = player->m_pending_config;
//                                  player->m_has_new_animation = false;
//                                  player->m_should_stop = false;
//                                  // 强制跳出延时，外层循环会处理帧重置
//                                  delay_left = 0; 
//                                  // 设置一个特殊标志或直接 break，让外层逻辑知道要重置
//                                  // 这里简单处理：break 出延时循环，外层 frame_idx 逻辑会继续
//                                  // 但为了立即生效，我们最好直接在这里触发重置逻辑
//                                  // 由于代码结构限制，最简单的是让外层检测到 m_has_new_animation 已处理? 
//                                  // 不，外层是在帧开头检测。
//                                  // 修正：我们在延时里收到了切换，需要通知外层立刻重置 frame_idx
//                                  // 可以在这里直接重置 frame_idx = -1 并 break 延时
//                                  frame_idx = -1; 
//                                  break; 
//                              }
//                          }
//                     }
//                 }
//                 if (player->m_should_stop) break;
//                 if (frame_idx < 0) continue; // 发生了切换，重启循环
//             }
            
//             // 播放结束处理
//             if (!player->m_should_stop && !player->m_is_playing) {
//                  ESP_LOGI(TAG, "Animation finished.");
//             }
//         } else {
//             vTaskDelay(pdMS_TO_TICKS(50));
//         }
//     }
// }

