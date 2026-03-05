idf_component_register(SRCS "main.cc"
                            "LCD_gc9a01/my_gc9a01.c"
                            "my_lvgl/lv_port_disp.c"
                            "my_lvgl/lvgl_UI_display.c"
                            "my_lvgl/lvgl_tick_timer.c"
                            "gif_encoder/my_gifdec.cc"
                            "jpeg_decoder/my_jpeg_decoder.c"
                            "animation_player/AnimationPlayer.cc"
                            "LCD_ST7735/my_ST7735.c"
                    PRIV_REQUIRES spi_flash
                    PRIV_REQUIRES spi_flash spiffs  # 添加 spiffs 依赖
                    INCLUDE_DIRS "LCD_gc9a01"  
                                 "my_lvgl"
                                 "."
                    REQUIRES lvgl # 把 LVGL 链接进来
                    REQUIRES esp_lcd driver

                    )
