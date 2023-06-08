/* SPIFFS Image Generation on Build Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "spiffs_custom.h"


static const char *TAG = "example";
#define SEND_DATA   100





void read_FS(char* file_name)
{
    char file_location[30];
    //char buf[1024];
    memset(file_location, 0, sizeof(file_location));
    strcat(file_location,"/spiffs/");
    strcat(file_location,file_name);
    FILE* f = fopen(file_location, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file or its empty with the name = %s ",file_name);
        return;
    }
    fseek(f, 0L, SEEK_END); // seek to the end of the file
    int size = ftell(f);
    ESP_LOGI(TAG,"total file size = %u",size);
    if(size == 0){
        return;
    }
    rewind(f);// seek back to the beggining of a file
    while(size > 0){
        ESP_LOGI(TAG,"reading in chunks");
        char buf[1024];
        uint16_t chunk_read_size = 0;
        ESP_LOGI(TAG,"size left to read = %u",size);
        if(size > 1024){
            chunk_read_size = 1024;
        }
        else{
            chunk_read_size = size;
        }
        fread(buf, 1, chunk_read_size, f);
        for(int i = 0; i <chunk_read_size; i++){
            ESP_LOGI(TAG,"buf[%i]=%u",i,buf[i]);
        }
        size = size - chunk_read_size;
    }

}


void write_append_FS(char* file_name,uint8_t* payload,uint16_t payload_length)
{
    char file_location[30];
    memset(file_location, 0, sizeof(file_location));
    strcat(file_location,"/spiffs/");
    strcat(file_location,file_name);
    FILE* fw = fopen(file_location, "a+"); // a+ for appending
    fwrite (payload , sizeof(uint8_t), payload_length, fw);
    fclose (fw);
}





void init_SPIFFS(){
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = false
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
}





