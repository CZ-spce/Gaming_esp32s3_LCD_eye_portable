#pragma once

#include <string>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// 播放模式枚举
enum class PlayMode {
    PLAY_ONCE,      // 播放一次后停止
    PLAY_LOOP       // 无限循环播放
};

// 控制命令枚举 (用于任务间通信)
enum class PlayerCommand {
    CMD_STOP,
    CMD_PLAY,
    CMD_SWITCH_ANIMATION
};

// 动画配置结构体
struct AnimationConfig {
    std::string base_path;      // 基础路径，例如 "/spiffs/"
    std::string file_prefix;    // 文件前缀，例如 "frame_"
    std::string file_suffix;    // 文件后缀，例如 ".jpg"
    int total_frames;           // 总帧数
    int delay_ms;               // 帧延时 (ms)
    PlayMode mode;              // 播放模式

    // 构造函数方便初始化
     // 修改这里：给所有参数添加默认值
    AnimationConfig(const std::string& path = "", 
                    const std::string& prefix = "", 
                    const std::string& suffix = "", 
                    int frames = 0, 
                    int delay = 0, 
                    PlayMode m = PlayMode::PLAY_ONCE) // 默认播一次
        : base_path(path), file_prefix(prefix), file_suffix(suffix), 
          total_frames(frames), delay_ms(delay), mode(m) {}
};

class AnimationPlayer {
public:
    // 单例模式获取实例 (可选，或者直接 new)
    static AnimationPlayer* getInstance();

    // 构造函数
    AnimationPlayer();
    
    // 初始化播放器 (创建任务)
    void begin();

    // 控制接口
    void playAnimation(const AnimationConfig& config);
    void stop();
    void switchAnimation(const AnimationConfig& config);
    
    // 获取当前状态
    bool isPlaying() const { return m_is_playing; }

private:
    // 内部任务函数
    static void playerTask(void* pvParameters);
    
    // 内部解码显示函数 (调用原有的 C 函数)
    bool displayFrame(const std::string& path);

    // 成员变量
    TaskHandle_t m_task_handle;
    QueueHandle_t m_cmd_queue;
    
    // 当前动画配置
    AnimationConfig m_current_config;
    bool m_is_playing;
    bool m_should_stop;
    bool m_has_new_animation;
    AnimationConfig m_pending_config;

    // 静态实例指针
    static AnimationPlayer* s_instance;
};
