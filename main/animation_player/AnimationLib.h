#pragma once

#include "AnimationPlayer.h"

AnimationConfig angry(
    "/spiffs/",        // 基础路径
    "angry_b_",          // 文件前缀
    ".jpg",            // 后缀
    67,                // 总帧数
    50,                // 延时 50ms (约 20FPS，需结合解码时间)
    PlayMode::PLAY_LOOP // 播放一次
);

AnimationConfig blink(
    "/spiffs/",        // 基础路径
    "blink_b_",          // 文件前缀
    ".jpg",            // 后缀
    67,                // 总帧数
    50,                // 延时 50ms (约 20FPS，需结合解码时间)
    PlayMode::PLAY_LOOP // 播放一次
);

AnimationConfig like(
    "/spiffs/",        // 基础路径
    "like_b_",          // 文件前缀
    ".jpg",            // 后缀
    67,                // 总帧数
    50,                // 延时 50ms (约 20FPS，需结合解码时间)
    PlayMode::PLAY_LOOP // 播放一次
);


