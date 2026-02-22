#ifndef __LVGL_UI_DISPLAY_H
#define __LVGL_UI_DISPLAY_H

#include "lvgl.h"

// 兼容 C++ 调用
#ifdef __cplusplus
extern "C" {
#endif

void lvgl_UI_test(lv_obj_t * scr);
void create_cool_ui(lv_obj_t * parent);
void show_angry_gif(void);
#ifdef __cplusplus
}
#endif

#endif 
