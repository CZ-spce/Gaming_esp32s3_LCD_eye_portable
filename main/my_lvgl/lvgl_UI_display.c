#include "LCD_gc9a01/my_gc9a01.h"
#include "lv_port_disp.h"
#include "lvgl.h"
#include "lvgl_UI_display.h"

/*lvgl测试函数
  UI效果：深蓝色背景，清晰可见的白色文字
  用途：主要用于测试lvgl的硬件搭建是否成功，以及字节顺序是否正确
*/
void lvgl_UI_test(lv_obj_t * scr)
{
  // 给屏幕设置一个深蓝色的背景色，方便观察边缘
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x001428), 0); 
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // 创建一个标签 (文字)
    lv_obj_t * label = lv_label_create(scr);
    // 设置文字内容
    lv_label_set_text(label, "Hello ESP32-S3!\nGC9A01 & LVGL 9");
    // 设置文字颜色为白色
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    // 居中对齐
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0); 
}

// -----------------------------------------------------------------
// 动画回调函数：更新圆弧的值
// -----------------------------------------------------------------
static void set_arc_value(void * obj, int32_t v)
{
    lv_arc_set_value((lv_obj_t *)obj, v);
}

// -----------------------------------------------------------------
// 动画回调函数：更新中心文字的百分比
// -----------------------------------------------------------------
static void set_text_value(void * obj, int32_t v)
{
    lv_label_set_text_fmt((lv_obj_t *)obj, "%" LV_PRId32 "%%", v);
}

/*UI2
 创建酷炫的UI
 效果：圆形进度条
*/
void create_cool_ui(lv_obj_t * parent)
{
    // 1. 设置纯黑背景
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);

    // 2. 创建一个圆弧 (能量条)
    lv_obj_t * arc = lv_arc_create(parent);
    lv_obj_set_size(arc, 220, 220);
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_rotation(arc, 270); // 从顶部开始
    lv_arc_set_bg_angles(arc, 0, 360); // 完整的圆
    
    // 隐藏默认的旋钮 (那个小圆点)
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB); 

    // 设置圆弧的颜色和厚度
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x1e1e1e), LV_PART_MAIN); // 轨道颜色 (暗灰)
    lv_obj_set_style_arc_width(arc, 15, LV_PART_MAIN);
    
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x00FFFF), LV_PART_INDICATOR); // 进度颜色 (青色)
    lv_obj_set_style_arc_width(arc, 15, LV_PART_INDICATOR);
    
    // 给圆弧加上发光效果 (阴影)
    lv_obj_set_style_shadow_color(arc, lv_color_hex(0x00FFFF), LV_PART_INDICATOR);
    lv_obj_set_style_shadow_width(arc, 20, LV_PART_INDICATOR);

    // 3. 创建中心的数值标签
    lv_obj_t * label = lv_label_create(parent);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0); // 使用大一点的内置字体

    // 4. 为圆弧创建动画
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_exec_cb(&a, set_arc_value);
    lv_anim_set_time(&a, 2000);             // 动画时长 2 秒
    lv_anim_set_playback_time(&a, 1000);    // 倒退时长 1 秒
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE); // 无限循环
    lv_anim_set_values(&a, 0, 100);
    lv_anim_start(&a);

    // 5. 为文字创建同步动画
    lv_anim_t a_text;
    lv_anim_init(&a_text);
    lv_anim_set_var(&a_text, label);
    lv_anim_set_exec_cb(&a_text, set_text_value);
    lv_anim_set_time(&a_text, 2000);
    lv_anim_set_playback_time(&a_text, 1000);
    lv_anim_set_repeat_count(&a_text, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_values(&a_text, 0, 100);
    lv_anim_start(&a_text);
}
