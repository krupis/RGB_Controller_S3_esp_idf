/* RMT example -- RGB LED Strip

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <esp_event.h>


#include <stdio.h>
#include "esp_system.h"
#include "spi_flash_mmap.h"
#include "nvs_flash.h"
#include "WIFI.h"
#include "webserver.h"
#include "RGB.h"
#include "UART0.h"



static const char *TAG = "example";


//extern esp_timer_handle_t periodic_timer; // this is main controller task timer ;
//extern esp_timer_handle_t running_lights_timer; // this is main controller task timer ;

void app_main(void)
{


    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    //ESP_ERROR_CHECK(esp_event_loop_create_default());
    printf("hello \n");


    wifi_init_softap();
    ESP_ERROR_CHECK(example_mount_storage("/spiffs"));
    ESP_ERROR_CHECK(example_start_file_server("/spiffs"));

    RGB_setup();

    // rgb_params.ramp_up_time = 3000; 
    // strip_color.red = 100;
    // strip_color.blue = 100;
    // strip_color.green = 0;
    // rgb_params.color_ramping = 1;
    // RGB_running_lights(&rgb_params);


    // rgb_params.ramp_up_time = 1000; 
    // strip_color.red = 100;
    // strip_color.blue = 100;
    // strip_color.green = 0;
    // RGB_fade_in_out(&rgb_params);


    //xTaskCreate(RGB_running_rainbow,"RGB_running_rainbow",10000,NULL,5,NULL); // receiving commands from main uart

    //xTaskCreate(RGB_sine_rainbow,"RGB_sine_rainbow",10000,NULL,5,NULL); // receiving commands from main uart


    xTaskCreate(UART0_task,"UART0_task",10000,NULL,5,NULL); // receiving commands from main uart

    //RGB_set_red(255);
    //RGB_set_green(100);
    //RGB_set_blue(50);




    //vTaskDelay(2000/portTICK_PERIOD_MS);



    
}
