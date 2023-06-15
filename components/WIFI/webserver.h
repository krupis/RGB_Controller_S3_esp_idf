#ifndef WEBSERVER_H
#define WEBSERVER_H


#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "RGB.h"


esp_err_t example_mount_storage(const char* base_path);
esp_err_t example_start_file_server(const char *base_path);





#endif