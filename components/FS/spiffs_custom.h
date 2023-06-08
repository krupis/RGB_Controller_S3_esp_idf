#ifndef SPIFFS_CUSTOM_H
#define SPIFFS_CUSTOM_H

#include "esp_partition.h"
#include "esp_random.h"
#include "esp_rom_sys.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_ota_ops.h"


#ifdef __cplusplus
extern "C" {
#endif


void read_FS(char* file_name);

void write_append_FS(char* file_name,uint8_t* payload,uint16_t payload_length);
void init_SPIFFS();
//esp_err_t update_handler();

#ifdef __cplusplus
}
#endif


#endif