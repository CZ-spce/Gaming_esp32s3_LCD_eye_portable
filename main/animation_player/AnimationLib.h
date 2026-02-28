#pragma once

#include "AnimationPlayer.h"

// 2. 配置第二个动画 (例如：dizzy.gif 转换来的序列帧)
AnimationConfig angry_b(
    "/spiffs/",        // 基础路径
    "angry_b_",          // 文件前缀
    ".jpg",            // 后缀
    67,                // 总帧数
    50,                // 延时 50ms (约 20FPS，需结合解码时间)
    PlayMode::PLAY_ONCE // 播放一次
);

// 2. 配置第二个动画 (例如：dizzy.gif 转换来的序列帧)
AnimationConfig angry_g(
    "/spiffs/",        // 基础路径
    "angry_g_",          // 文件前缀
    ".jpg",            // 后缀
    67,                // 总帧数
    50,                // 延时 50ms (约 20FPS，需结合解码时间)
    PlayMode::PLAY_ONCE // 播放一次
);

