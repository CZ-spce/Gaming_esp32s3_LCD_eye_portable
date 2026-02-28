#pragma once

#include <string>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// 性别模式枚举
enum class GenderMode {
    MEN,      
    WOMEN,
    Genderless
};

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
    void switchGenderAnimation(void);
    void setGenderAnimation(GenderMode gender);

    // 获取当前状态
    bool isPlaying() const { return m_is_playing; }

private:
    // 内部任务函数
    static void playerTask(void* pvParameters);
    
    // 内部解码显示函数 (调用原有的 C 函数)
    bool displayFrame(const std::string& path);

    // 成员变量
    TaskHandle_t m_task_handle;  // 保存 FreeRTOS 动画播放任务的句柄，可用于后续控制（如挂起/删除），
    QueueHandle_t m_cmd_queue;   //FreeRTOS 队列句柄，用于主线程向播放任务发送控制命令（如播放/停止）。
    
    GenderMode current_gender;  
    //当前性别

    bool gender_switch;  
    // 性别切换标志位


    // 当前动画配置
    AnimationConfig m_current_config;
    // ▶ 当前正在播放（或即将播放）的动画参数。
    //   - 包含路径、帧数、延时、播放模式等
    //   - 由 playerTask 读取并用于生成帧文件名

    bool m_is_playing;         
    // ▶ 播放状态标志：
    //   - true：正在播放动画（playerTask 会进入帧循环）
    //   - false：空闲状态（任务休眠）

    bool m_should_stop;               
    // ▶ 停止请求标志：
    //   - 当收到 CMD_STOP 时设为 true
    //   - playerTask 在每帧开始前检查此标志，实现快速中断

    bool m_has_new_animation;   
    // ▶ 新动画待切换标志：
    //   - 当调用 playAnimation()/switchAnimation() 时设为 true
    //   - playerTask 收到 CMD_PLAY/CMD_SWITCH 后，
    //     若此标志为 true，则加载 m_pending_config 到 m_current_config
       
    
    AnimationConfig m_pending_config;
    // ▶ 待切换的动画配置缓存：
    //   - playAnimation() 等接口先将新配置存入此处
    //   - 避免在中断上下文或非任务上下文中直接修改 m_current_config（线程安全）


    // 静态实例指针
    static AnimationPlayer* s_instance;
    // ▶ 单例模式的全局唯一实例指针。
    //   - 通过 getInstance() 初始化和访问
    //   - 确保整个系统只有一个动画播放器实例
};


// /*  下面是添加了性别区别的版本 */
// #pragma once

// #include <string>
// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>
// #include <freertos/queue.h>

// // 定义性别枚举，比用字符更安全
// enum class Gender {
//     MALE,   // 对应 'b' (boy)
//     FEMALE  // 对应 'g' (girl)
// };

// // 播放模式
// enum class PlayMode {
//     PLAY_ONCE,
//     PLAY_LOOP
// };

// // 命令枚举
// enum class PlayerCommand {
//     CMD_STOP,
//     CMD_PLAY,
//     CMD_SWITCH_ANIMATION
// };

// struct AnimationConfig {
//     std::string anim_name;      // 动画名称，如 "angry", "like", "blink"
//     Gender gender;              // 性别
//     int total_frames;           // 总帧数 (可以在加载时自动探测，或者预设)
//     int delay_ms;               // 帧延时
//     PlayMode mode;              // 播放模式
    
//     // 辅助函数：根据名称和性别自动生成文件前缀 (如 "angry_b_")
//     std::string getPrefix() const {
//         char g_char = (gender == Gender::MALE) ? 'b' : 'g';
//         return anim_name + "_" + g_char + "_";
//     }
    
//     // 基础路径 (SPIFFS 根目录通常是 "/spiffs" 或 "/")
//     std::string base_path; 
// };

// class AnimationPlayer {
// public:
//     AnimationPlayer();
//     ~AnimationPlayer();

//     // 初始化
//     void begin(const std::string& spiffs_root = "/spiffs");

//     // 【核心修改】新的切换接口
//     // 参数：动画名 (如 "angry"), 性别, 帧延时, 是否循环
//     void switchAnimation(const std::string& anim_name, Gender gender, int delay_ms = 100, PlayMode mode = PlayMode::PLAY_LOOP);
    
//     // 便捷函数：仅切换当前动画的性别 (保持动作不变)
//     void toggleGender(); 

//     // 停止播放
//     void stop();

// private:
//     // FreeRTOS 任务
//     static void playerTask(void* pvParameters);
    
//     // 显示单帧 (内部使用完整路径)
//     bool displayFrame(const std::string& full_path);
    
//     // 探测帧数 (可选：如果不想硬编码总帧数，可以写一个简单的文件存在性检查)
//     int detectFrameCount(const std::string& prefix);

//     // 成员变量
//     TaskHandle_t m_task_handle;
//     QueueHandle_t m_cmd_queue;
    
//     AnimationConfig m_current_config;
//     AnimationConfig m_pending_config;
    
//     bool m_is_playing;
//     bool m_should_stop;
//     bool m_has_new_animation;

//     // 简单的互斥锁保护配置读写 (可选，如果只在任务内写则不需要，但为了安全建议加上)
//     // 这里简化处理，依靠队列串行化命令
// };

