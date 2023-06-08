
#ifndef WIFI_H
#define WIFI_H


#include <dirent.h>
#include "esp_https_ota.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include <string.h> /* memset */
#include "esp_spiffs.h"
#include "esp_ota_ops.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"


#include "lwip/err.h"
#include "lwip/sys.h"
#include <sys/param.h>
#include "esp_netif.h"
#include <esp_http_server.h>

#include "math.h"
#include <stdio.h>
#include "RGB.h"







/* The examples use WiFi configuration that you can set via project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/




//#include <unistd.h> /* close */




#define EXAMPLE_ESP_WIFI_SSID          "test123"
#define EXAMPLE_ESP_WIFI_PASS          "test123"
#define DEFAULT_SCAN_LIST_SIZE 10
#define EXAMPLE_ESP_WIFI_SSID_AP      "Smart_led_control"
#define EXAMPLE_ESP_WIFI_PASS_AP     "test12345"
#define EXAMPLE_ESP_WIFI_CHANNEL   1
#define EXAMPLE_MAX_STA_CONN       10

//
#define EXAMPLE_ESP_MAXIMUM_RETRY  2

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

typedef enum{
   WIFI_DISABLED = 0,
   WIFI_AP = 1,
   WIFI_STA = 2,
   WIFI_AP_STA = 3,
   WIFI_AP_SCAN = 4,
}wifi_status_e;



extern uint8_t wifi_connected_clients;
extern wifi_status_e wifi_status;









void WIFI_INIT_AP_SCAN(void);
void WIFI_STOP_AP_SCAN();

void WIFI_INIT_AP();
void WIFI_INIT_STA();
void WIFI_INIT_AP_STA();

void STOP_WIFI_AP_STA();
void STOP_WIFI_STA();
void STOP_WIFI_AP();

uint16_t WIFI_scan(uint8_t* buf);
void WIFI_scan_test();




esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err);
void connect_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data);
void disconnect_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data);


esp_err_t start_file_server(const char *base_path);
void stop_webserver(httpd_handle_t server);



esp_err_t update_handler(char* file_name);

// for the fota update in STA mode
esp_err_t do_firmware_upgrade();
esp_err_t do_firmware_upgrade_http();





#endif


