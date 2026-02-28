#pragma once

#include "AnimationPlayer.h"

AnimationConfig angry(
    "/spiffs/",        // 基础路径
    "angry_b_",          // 文件前缀
    ".jpg",            // 后缀
    67,                // 总帧数
    50,                // 延时 50ms (约 20FPS，需结合解码时间)
    PlayMode::PLAY_ONCE // 播放一次
);



