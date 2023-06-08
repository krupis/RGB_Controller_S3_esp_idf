idf_component_register(SRCS "spiffs_custom.c"
                    INCLUDE_DIRS "."
                    REQUIRES spiffs app_update)

spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)