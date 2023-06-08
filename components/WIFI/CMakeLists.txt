idf_component_register(SRCS "WIFI.c" "webserver.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_https_ota json esp_https_server spiffs app_update RGB esp_wifi spi_flash fatfs vfs)