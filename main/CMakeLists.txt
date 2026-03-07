idf_component_register(SRCS "main.cc"
                            "LCD_display/LCD_gc9a01/my_gc9a01.c"
                            "LCD_display/my_lvgl/lv_port_disp.c"
                            "LCD_display/my_lvgl/lvgl_UI_display.c"
                            "LCD_display/my_lvgl/lvgl_tick_timer.c"
                            "LCD_display/gif_encoder/my_gifdec.cc"
                            "LCD_display/jpeg_decoder/my_jpeg_decoder.c"
                            "LCD_display/animation_player/AnimationPlayer.cc"
                            "LCD_display/LCD_ST7735/my_ST7735.c"
                            "QMA6100P/qma6100p.c"
                            "IIC/iic.c"
                    PRIV_REQUIRES spi_flash
                    PRIV_REQUIRES spi_flash spiffs  # 添加 spiffs 依赖
                    INCLUDE_DIRS "."
                                 "LCD_display"

                                 
                #     REQUIRES lvgl # 把 LVGL 链接进来
                #     REQUIRES esp_lcd driver

                    )
