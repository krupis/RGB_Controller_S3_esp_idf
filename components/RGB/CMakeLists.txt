idf_component_register(SRCS "RGB.c"
                    INCLUDE_DIRS "."
                    REQUIRES led_strip driver esp_timer)