idf_component_register(SRCS "strip_animations.c" "RGB.c"
                    INCLUDE_DIRS "."
                    REQUIRES led_strip driver esp_timer)