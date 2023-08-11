
#ifndef WIFI_H
#define WIFI_H


#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_event.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>
#include "esp_mac.h"




#define DEFAULT_SCAN_LIST_SIZE 10
#define EXAMPLE_ESP_WIFI_SSID_AP      "Smart_led_control_lukas"
#define EXAMPLE_ESP_WIFI_PASS_AP     "test12345"
#define EXAMPLE_ESP_WIFI_CHANNEL   1
#define EXAMPLE_MAX_STA_CONN       10

//
#define EXAMPLE_ESP_MAXIMUM_RETRY  2

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


void wifi_init_softap(void);



#endif


