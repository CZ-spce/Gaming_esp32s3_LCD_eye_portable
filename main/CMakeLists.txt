idf_component_register(SRCS "main.cc"
                            "LCD_gc9a01/my_gc9a01.c"
                    PRIV_REQUIRES spi_flash
                    INCLUDE_DIRS "LCD_gc9a01"  
                    )
