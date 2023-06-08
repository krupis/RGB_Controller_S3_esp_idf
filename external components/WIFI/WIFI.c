#include "WIFI.h"


static const char *TAG_WIFI = "wifi station";
static const char *TAG_HTTP = "HTTP";
static const char *TAG_WEBSERVER = "webserver";
static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group;

static void ftoa(float n, char* res, int afterpoint);

static void reverse(char* str, int len);

static int intToStr(int x, char str[], int d);


uint8_t wifi_buf_global[800];
uint16_t asset_tracking_length;
httpd_handle_t server_global = NULL;
bool server_status = 0;
esp_netif_t* ap_netif_g;
esp_netif_t* sta_netif_g;

uint8_t wifi_connected_clients = 0;

wifi_status_e wifi_status = WIFI_DISABLED;

extern struct device_info_s device_info;


extern esp_timer_handle_t fading_lights_timer; // this is main controller task timer 
extern esp_timer_handle_t running_lights_timer; // this is main controller task timer 
extern struct rgb_color_s strip_color;





//#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define FILE_PATH_MAX 256
/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (1700*1024) // 200 KB
#define MAX_FILE_SIZE_STR "1700KB"
/* Scratch buffer size */
//#define SCRATCH_BUFSIZE  ė09š
#define SCRATCH_BUFSIZE  4096
#define SEND_DATA   2048 // data is being written from spiffs to ota partition in 100b chunks
//#define SCRATCH_BUFSIZE  100

static esp_err_t download_get_handler(httpd_req_t *req);
static esp_err_t upload_post_handler(httpd_req_t *req);
static esp_err_t delete_post_handler(httpd_req_t *req);
static esp_err_t update_post_handler(httpd_req_t *req);
static esp_err_t upload_and_flash_post_handler(httpd_req_t *req);
static esp_err_t get_image_handler(httpd_req_t *req);
static esp_err_t rollback_handler(httpd_req_t *req);


static esp_err_t handle_brightness_change(httpd_req_t *req);
static esp_err_t handle_animation_change(httpd_req_t *req);
static esp_err_t handle_FW_version(httpd_req_t *req);
static esp_err_t handle_HW_version(httpd_req_t *req);
static esp_err_t handle_BLE_version(httpd_req_t *req);



static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath);
static esp_err_t index_html_get_handler(httpd_req_t *req);
static esp_err_t favicon_get_handler(httpd_req_t *req);
static esp_err_t styles_handler(httpd_req_t *req);
static esp_err_t logo_handler(httpd_req_t *req);
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename);
static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize);



struct file_server_data {
    /* Base path of file storage */
    char base_path[256+1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};





static void wifi_event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data);
static void event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data);



static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG_WIFI, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG_WIFI,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG_WIFI, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}






static void wifi_event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG_WEBSERVER, "station "MACSTR" join, AID=%d",MAC2STR(event->mac), event->aid);
        wifi_connected_clients += 1;
        ESP_LOGI(TAG_WEBSERVER,"connected clients = %u ",wifi_connected_clients);


        if(wifi_connected_clients >0 && server_status == 0){
                ESP_LOGI(TAG_WEBSERVER, "Starting webserver");
                ESP_ERROR_CHECK(start_file_server("/spiffs"));
        }
    } 
    
    

    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG_WEBSERVER, "station "MACSTR" leave, AID=%d",MAC2STR(event->mac), event->aid);
        wifi_connected_clients -= 1;
        ESP_LOGI(TAG_WEBSERVER,"connected clients = %u ",wifi_connected_clients);
        
        if(wifi_connected_clients == 0 && server_status == 1){
            stop_webserver(server_global);

        }
        
    }
}












void WIFI_INIT_AP_SCAN(void)
{

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    sta_netif_g = esp_netif_create_default_wifi_ap();
    assert(sta_netif_g);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    //ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_LOGI(TAG_WIFI, "wifi_init_sta finished.");
    wifi_status = WIFI_AP_SCAN;

}


void WIFI_STOP_AP_SCAN(){
        esp_wifi_disconnect();            // break connection to AP
        esp_wifi_stop();                 // shut down the wifi radio
        esp_wifi_deinit();              // release wifi resources   
        esp_netif_destroy_default_wifi(sta_netif_g);
        esp_event_loop_delete_default();
        esp_netif_deinit();
        wifi_status = WIFI_DISABLED;
}





void WIFI_INIT_AP_STA(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif_g = esp_netif_create_default_wifi_sta();
    assert(sta_netif_g);
    ap_netif_g = esp_netif_create_default_wifi_ap();
    assert(ap_netif_g);
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));



    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID_AP,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID_AP),
            .password = EXAMPLE_ESP_WIFI_PASS_AP,
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_OPEN,
        },
    };



    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    ESP_LOGI("WIFI AP", "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID_AP, EXAMPLE_ESP_WIFI_PASS_AP, EXAMPLE_ESP_WIFI_CHANNEL);
    wifi_status = WIFI_AP_STA;

}






void STOP_WIFI_AP_STA(){
        if(wifi_connected_clients > 0){
            stop_webserver(server_global);
        }
        esp_event_handler_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,&wifi_event_handler); // Unregister event handler only when wifi is disabled (60sec when no client or 5min when client is connected)
        esp_wifi_disconnect();            // break connection to AP
        esp_wifi_stop();                 // shut down the wifi radio
        esp_wifi_deinit();              // release wifi resources   
        esp_netif_destroy_default_wifi(sta_netif_g);
        esp_netif_destroy_default_wifi(ap_netif_g);
        esp_event_loop_delete_default();
        esp_netif_deinit();
        wifi_connected_clients = 0;
        wifi_status = WIFI_DISABLED;
}







#define MAC_LENGTH 6
//wifi_init must be called prior to using this function
uint16_t WIFI_scan(uint8_t* buf) {
    char temp_buff_rssi[5];
    char temp_buf_mac1[3];
    char temp_buf_mac2[3];
    char temp_buf_mac3[3];
    char temp_buf_mac4[3];
    char temp_buf_mac5[3];
    char temp_buf_mac6[3];

    memset(buf, 0, 800);

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    uint16_t offset   = 0;


    //ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    if (!ap_count) {
        return offset;
    }
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        if (buf != NULL) {
            uint8_t _len = strlen((char*)ap_info[i].ssid);
            // Store SSID length
            buf[offset] = _len;
            offset++;
            // Store SSID if scanned network has such
            if (_len) {
                memcpy(&buf[offset], ap_info[i].ssid, _len);
                offset += _len;
            }
            // Store MAC
            sprintf(temp_buf_mac1, "%02X", ap_info[i].bssid[0]);
            sprintf(temp_buf_mac2, "%02X", ap_info[i].bssid[1]);
            sprintf(temp_buf_mac3, "%02X", ap_info[i].bssid[2]);
            sprintf(temp_buf_mac4, "%02X", ap_info[i].bssid[3]);
            sprintf(temp_buf_mac5, "%02X", ap_info[i].bssid[4]);
            sprintf(temp_buf_mac6, "%02X", ap_info[i].bssid[5]);
            memcpy(&buf[offset], temp_buf_mac1, 2);
            offset += 2;
            memcpy(&buf[offset], temp_buf_mac2, 2);
            offset += 2;
            memcpy(&buf[offset], temp_buf_mac3, 2);
            offset += 2;
            memcpy(&buf[offset], temp_buf_mac4, 2);
            offset += 2;
            memcpy(&buf[offset], temp_buf_mac5, 2);
            offset += 2;
            memcpy(&buf[offset], temp_buf_mac6, 2);
            offset += 2;
            // Store RSSI
 
            itoa(ap_info[i].rssi,temp_buff_rssi,10);
            memcpy(&buf[offset], temp_buff_rssi, strlen(temp_buff_rssi));
            offset = offset+strlen(temp_buff_rssi);
            char NULL_CHAR = 0x00;
            memcpy(&buf[offset], &NULL_CHAR, 1);
            offset +=1;
        }
        ESP_LOGI("WIFI SCAN", "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI("WIFI SCAN", "RSSI \t\t%d", ap_info[i].rssi);
        ESP_LOGI("WIFI SCAN", "MAC %02X:%02X:%02X:%02X:%02X:%02X",ap_info[i].bssid[0],ap_info[i].bssid[1],ap_info[i].bssid[2],ap_info[i].bssid[3],ap_info[i].bssid[4],ap_info[i].bssid[5]);
        ESP_LOGI("WIFI SCAN", "Channel \t\t%d\n", ap_info[i].primary);
    }

    return offset;
}




/*
void WIFI_scan_test() {

    wifi_scan_config_t config=
    {
    .show_hidden          = false,
    .scan_type            = WIFI_SCAN_TYPE_ACTIVE,
    .scan_time.active.min = 100,
    .scan_time.active.max = 1000,

    };
    ESP_ERROR_CHECK(esp_wifi_scan_start(&config, true));
}
*/








esp_err_t start_file_server(const char *base_path)
{

    struct file_server_data *server_data = NULL;
    /* Validate file storage base path */
    if (!base_path || strcmp(base_path, "/spiffs") != 0) {
        ESP_LOGE("START_SERVER", "File server presently supports only '/spiffs' as base path");
        return ESP_ERR_INVALID_ARG;
    }

    if (server_data) {
        ESP_LOGE("START_SERVER", "File server already started");
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate memory for server data */
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data) {
        ESP_LOGE("START_SERVER", "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }
    strlcpy(server_data->base_path, base_path,
            sizeof(server_data->base_path));



    //httpd_handle_t server = NULL; //use global instead

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 15;
    config.stack_size = 8096;
    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

    //ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, &server_global));

    ESP_LOGI("START_SERVER", "Starting HTTP Server on port: '%d'", config.server_port);
    if (httpd_start(&server_global, &config) != ESP_OK) {
        ESP_LOGE("START_SERVER", "Failed to start file server!");
        return ESP_FAIL;
    }


  

    //URI handler for getting uploaded files 
    httpd_uri_t file_download = {
        .uri       = "/*",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = download_get_handler,
       .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server_global, &file_download);
    


    // URI handler for uploading files to server 
    httpd_uri_t file_upload = {
        .uri       = "/upload/*",   // Match all URIs of type /upload/path/to/file
        .method    = HTTP_POST,
        .handler   = upload_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server_global, &file_upload);


        httpd_uri_t file_upload_flash = {
        .uri       = "/upload_flash/*",   // Match all URIs of type /upload/path/to/file
        .method    = HTTP_POST,
        .handler   = upload_and_flash_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server_global, &file_upload_flash);



    // URI handler for deleting files from server 
    httpd_uri_t file_delete = {
        .uri       = "/delete/*",   // Match all URIs of type /delete/path/to/file
        .method    = HTTP_POST,
        .handler   = delete_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server_global, &file_delete);




    // URI handler for deleting files from server 
    httpd_uri_t file_update = {
        .uri       = "/update/*",   // Match all URIs of type /delete/path/to/file
        .method    = HTTP_POST,
        .handler   = update_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server_global, &file_update);




    httpd_uri_t uri_post = {
        .uri      = "/rollback",
        .method   = HTTP_POST,
        .handler  = rollback_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server_global, &uri_post);



    httpd_uri_t uri_brightness = {
        .uri      = "/brightness",
        .method   = HTTP_POST,
        .handler  = handle_brightness_change,
        .user_ctx = server_data
    };
    httpd_register_uri_handler(server_global, &uri_brightness);


    httpd_uri_t uri_animation = {
        .uri      = "/animation",
        .method   = HTTP_POST,
        .handler  = handle_animation_change,
        .user_ctx = server_data
    };
    httpd_register_uri_handler(server_global, &uri_animation);





    server_status = 1;
    
    

    return ESP_OK;
}






void stop_webserver(httpd_handle_t server)
{
    
    ESP_LOGI(TAG_WEBSERVER, "Stopping webserver");
    //esp_event_handler_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,&wifi_event_handler);
    httpd_stop(server);
    server_status = 0;


}









static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}



static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath)
{
    //httpd_resp_set_type(req,"text/html");
    char entrypath[FILE_PATH_MAX];
    char entrysize[16];
    const char *entrytype;

    struct dirent *entry;
    struct stat entry_stat;

    DIR *dir = opendir(dirpath);
    const size_t dirpath_len = strlen(dirpath);

    /* Retrieve the base path of file storage to construct the full path */
    strlcpy(entrypath, dirpath, sizeof(entrypath));

    if (!dir) {
        ESP_LOGE("DOWNLOAD", "Failed to stat dir : %s", dirpath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }
    /* Send HTML file header */
    //httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");
    /* Get handle to embedded file upload script */

    extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
    extern const unsigned char upload_script_end[]   asm("_binary_upload_script_html_end");
    const size_t upload_script_size = (upload_script_end - upload_script_start);
    httpd_resp_send_chunk(req, (const char *)upload_script_start, upload_script_size);
/*
    httpd_resp_sendstr_chunk(req, "<h1>");
    httpd_resp_sendstr_chunk(req, "Current device FW version is: ");
    httpd_resp_sendstr_chunk(req, FW_VERSION);
    httpd_resp_sendstr_chunk(req, "</h1>");

    httpd_resp_sendstr_chunk(req, "<h1>");
    httpd_resp_sendstr_chunk(req, "Current device HW version is: ");
    httpd_resp_sendstr_chunk(req, HW_VERSION);
    httpd_resp_sendstr_chunk(req, "</h1>");

    httpd_resp_sendstr_chunk(req, "<h1>");
    httpd_resp_sendstr_chunk(req, "Current device BT version is: ");
    httpd_resp_sendstr_chunk(req, BT_VERSION);
    httpd_resp_sendstr_chunk(req, "</h1>");
    */





    // Send file-list table definition and column labels */
    httpd_resp_sendstr_chunk(req,
        "<table class=\"fixed\" border=\"1\">"
        "<col width=\"500px\" /><col width=\"300px\" /><col width=\"300px\" /><col width=\"100px\" /> <col width=\"100px\" />"
        "<thead><tr><th>Name</th><th>Type</th><th>Size (Bytes)</th><th>Delete</th><th>Update</th></tr></thead>"
        "<tbody>");

    // Iterate over all files / folders and fetch their names and sizes 
    while ((entry = readdir(dir)) != NULL) {
        /*
        if(strcmp(entry->d_name,".gitkeep")==0){
            continue;
        }
        */
        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
        if (stat(entrypath, &entry_stat) == -1) {
            ESP_LOGE("DOWNLOAD", "Failed to stat %s : %s", entrytype, entry->d_name);
            continue;
        }
        sprintf(entrysize, "%ld", entry_stat.st_size);
        ESP_LOGI("DOWNLOAD", "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);

        // Send chunk of HTML file containing table entries with file name and size */
        httpd_resp_sendstr_chunk(req, "<tr><td><a href=\"");
        httpd_resp_sendstr_chunk(req, req->uri);
        httpd_resp_sendstr_chunk(req, entry->d_name);
        if (entry->d_type == DT_DIR) {
            httpd_resp_sendstr_chunk(req, "/");
        }
        httpd_resp_sendstr_chunk(req, "\">");
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "</a></td><td>");
        httpd_resp_sendstr_chunk(req, entrytype);
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, entrysize);
        httpd_resp_sendstr_chunk(req, "</td><td>");
        httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/delete");
        httpd_resp_sendstr_chunk(req, req->uri);
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Delete</button></form>");

        httpd_resp_sendstr_chunk(req, "</td><td>");
        //httpd_resp_sendstr_chunk(req, "<button id=\"update\" type=\"button\" onclick=\"update_device()\">Update</button>");

        //httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/update");
        //httpd_resp_sendstr_chunk(req, req->uri);
        //httpd_resp_sendstr_chunk(req, entry->d_name);
        //httpd_resp_sendstr_chunk(req, "\"><button class=\"button button2\" type=\"submit\">Update</button></form>");

        httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/update");
        httpd_resp_sendstr_chunk(req, req->uri);
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "\"><button class=\"button button2\" type=\"submit\">Update</button></form>");


        httpd_resp_sendstr_chunk(req, "</td></tr>\n");
    }
    closedir(dir);

    /* Finish the file list table */
    httpd_resp_sendstr_chunk(req, "</tbody></table>");

    /* Send remaining chunk of HTML file to complete it */
    httpd_resp_sendstr_chunk(req, "</body></html>");

    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}






static esp_err_t get_image_handler(httpd_req_t *req)
{
    /* Send a simple response */
    //const char resp[] = "URI GET Response";
    //httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    const char resp[] = "URI GET Response";
    extern const unsigned char elstat_test_array_start[] asm("_binary_elstat_test_jpeg_start");
    extern const unsigned char elstat_test_array_end[] asm("_binary_elstat_test_jpeg_end");
    const size_t logo_size_test = (elstat_test_array_end - elstat_test_array_start);
    httpd_resp_set_type(req, "image/jpeg");
    //httpd_resp_send(req, (const char *)elstat_test_array_start, logo_size_test);
    httpd_resp_send_chunk(req, (const char *)elstat_test_array_start, logo_size_test);
    return ESP_OK;
}








/*
static esp_err_t favicon_get_handler(httpd_req_t *req)
{   
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}
*/




static esp_err_t styles_handler(httpd_req_t *req)
{
    extern const unsigned char styles_start[] asm("_binary_styles_css_start");
    extern const unsigned char styles_end[]   asm("_binary_styles_css_end");
    const size_t styles_size = (styles_end - styles_start);
	httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char *)styles_start, styles_size);
	//httpd_resp_send(req, (const char *)styles_start, (styles_end-1) - styles_start);

	return ESP_OK;
}





static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    
    extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
    extern const unsigned char upload_script_end[]   asm("_binary_upload_script_html_end");
    const size_t upload_script_size = (upload_script_end - upload_script_start);
    httpd_resp_send(req, (const char *)upload_script_start, upload_script_size);
    return ESP_OK;
    

}

/*
   static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);  // Response body can be empty
    return ESP_OK;
}
*/












static esp_err_t download_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri, sizeof(filepath));

    if (!filename) {
        ESP_LOGE("DOWNLOAD", "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }
    
    // If name has trailing '/', respond with directory contents 
    
    if (filename[strlen(filename) - 1] == '/') {
        return index_html_get_handler(req);
    }
    
    /*
    if (filename[strlen(filename) - 1] == '/') {
        return http_resp_dir_html(req, filepath);
    }
    */
    
    if (stat(filepath, &file_stat) == -1) {
        /* If file not present on SPIFFS check if URI
         * corresponds to one of the hardcoded paths */
        
        if (strcmp(filename, "/index.html") == 0) {
            return index_html_get_handler(req);
        } 
        
/*
        if (strcmp(filename, "/favicon.ico") == 0) {
            return favicon_get_handler(req);
        }
        */
        
        else if(strcmp(filename,"/styles.css")== 0){
            return styles_handler(req);
        }
        
        /*
         else if(strcmp(filename,"/image2.png")== 0){
            return logo_handler(req);
        }
        */


        ESP_LOGE("DOWNLOAD", "Failed to stat file : %s", filepath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE("DOWNLOAD", "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    ESP_LOGI("DOWNLOAD", "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    set_content_type_from_file(req, filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(fd);
                ESP_LOGE("DOWNLOAD", "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
               return ESP_FAIL;
           }
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    /* Close file after sending complete */
    fclose(fd);
    ESP_LOGI("DOWNLOAD", "File sending complete");

    /* Respond with an empty chunk to signal HTTP response completion */
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}








// Handler to upload a file onto the server 
static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    // Skip leading "/upload" from URI to get filename 
    // Note sizeof() counts NULL termination hence the -1 
    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri + sizeof("/upload") - 1, sizeof(filepath));
    if (!filename) {
        // Respond with 500 Internal Server Error 
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    // Filename cannot have a trailing '/' 
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE("UPLOAD", "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == 0) {
        ESP_LOGE("UPLOAD", "File already exists : %s", filepath);
        // Respond with 400 Bad Request 
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        return ESP_FAIL;
    }

    // File cannot be larger than a limit 
    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE("UPLOAD", "File too large : %d bytes", req->content_len);
        // Respond with 400 Bad Request 
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than "
                            MAX_FILE_SIZE_STR "!");
        //Return failure to close underlying connection else the
        // incoming file content will keep the socket busy 
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd) {
        ESP_LOGE("UPLOAD", "Failed to create file : %s", filepath);
        // Respond with 500 Internal Server Error //
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    ESP_LOGI("UPLOAD", "Receiving file : %s...", filename);

    //Retrieve the pointer to scratch buffer for temporary storage 
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int received;
    // Content length of the request gives
    // the size of the file being uploaded 
    int remaining = req->content_len;
    while (remaining > 0) {
        ESP_LOGI("UPLOAD", "Remaining size : %d", remaining);
        // Receive the file part by part into a buffer 
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                // Retry if timeout occurred 
                continue;
            }

            // In case of unrecoverable error,
            // close and delete the unfinished file
            fclose(fd);
            unlink(filepath);

            ESP_LOGE("UPLOAD", "File reception failed!");
            // Respond with 500 Internal Server Error *
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        // Write buffer content to file on storage 
        if (received && (received != fwrite(buf, 1, received, fd))) {
            // Couldn't write everything to file!
            // Storage may be full? 
            fclose(fd);
            unlink(filepath);

            ESP_LOGE("UPLOAD", "File write failed!");
            // Respond with 500 Internal Server Error //
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        // Keep track of remaining size of
        // the file left to be uploaded 
        remaining -= received;
    }

    // Close file upon upload completion 
    fclose(fd);
    ESP_LOGI("UPLOAD", "File reception complete");

    // Redirect onto root to see the updated file list
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File uploaded successfully");

 
    return ESP_OK;
}













// Handler to upload a file onto the server 
static esp_err_t upload_and_flash_post_handler(httpd_req_t *req){
    
    printf("upload and flash handler \n");
    char filepath[FILE_PATH_MAX];
    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri + sizeof("/upload") - 1, sizeof(filepath));

    // File cannot be larger than a limit 
    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE("UPLOAD", "File too large : %d bytes", req->content_len);
        // Respond with 400 Bad Request 
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than "
                            MAX_FILE_SIZE_STR "!");
        //Return failure to close underlying connection else the
        // incoming file content will keep the socket busy 
        return ESP_FAIL;
    }



    ESP_LOGI("UPLOAD", "Receiving file : %s...", filename);

    //Retrieve the pointer to scratch buffer for temporary storage 
    
    int received;


    esp_err_t ret;
    
    const esp_partition_t *update_partition;
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running  = esp_ota_get_running_partition();
    esp_ota_handle_t well_done_handle = 0;  // Handler for OTA update. 
    if (configured != running) {
        ESP_LOGW("UPDATE", "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                        configured->address, running->address);
        ESP_LOGW("UPDATE", "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)"
                        );
    }
    ESP_LOGI("UPDATE", "Running partition type %d subtype %d (offset 0x%08x)",
                    running->type, running->subtype, running->address);

    ESP_LOGI("UPDATE", "Starting OTA example");
    // It finds the partition where it should write the firmware
    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI("UPDATE", "Writing to partition subtype %d at offset 0x%x",
                    update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);
    
    // Reset of this partition
    ESP_ERROR_CHECK(esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &well_done_handle));

   
    char buf[SEND_DATA];
    // Init of the buffer
    memset(buf, 0, sizeof(buf));
    // Content length of the request gives
    // the size of the file being uploaded 
    int remaining = req->content_len;
    while (remaining > 0) {
        uint16_t read_chunk_length = MIN(remaining, SEND_DATA);
        vTaskDelay(20/portTICK_PERIOD_MS);
        ESP_LOGI("UPLOAD", "Remaining size : %d", remaining);
        // Receive the file part by part into a buffer 
        if ((received = httpd_req_recv(req, buf, read_chunk_length)) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE("UPLOAD", "File reception failed!");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }
        //printf("data actually received received=%u \n",received);
        //printf("read_chunk_length=%u \n",read_chunk_length);
        ret = esp_ota_write(well_done_handle, buf, received);
        if(ret != ESP_OK){
            ESP_LOGE("UPDATE", "Firmware upgrade failed");
            while (1) {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            return ESP_FAIL;
        }
        
    remaining -= received;
    }


    ESP_LOGI("UPLOAD", "File reception complete");

    // Redirect onto root to see the updated file list
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File uploaded successfully");

    // If you are here it means there are no problems!
    ESP_ERROR_CHECK(esp_ota_end(well_done_handle));

    // OTA partition configuration
    ESP_ERROR_CHECK(esp_ota_set_boot_partition(update_partition));
    ESP_LOGI("UPDATE", "Restarting...");
    // REBOOT!!!!!
    esp_restart();
    return ESP_OK;
}











static esp_err_t delete_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    /* Skip leading "/delete" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri  + sizeof("/delete") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE("DELETE", "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE("DELETE", "File does not exist : %s", filename);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }

    ESP_LOGI("DELETE", "Deleting file : %s", filename);
    /* Delete file */
    unlink(filepath);
    //const size_t file_size = 1024  * 1024;
    //esp_spiffs_gc(NULL, file_size);   

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
}
















static esp_err_t update_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri  + sizeof("/delete") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE("DELETE", "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE("DELETE", "File does not exist : %s", filename);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }

//FILE* f = fopen("/spiffs/ELSTAT_ESP_IDF.bin", "r");
    ESP_LOGI("UPDATE", "Updating firmware file : %s", filename);
    char full_name_buf[64];
    memset(&full_name_buf, 0, sizeof(full_name_buf));

    sprintf(full_name_buf, "%s", "/spiffs");
    sprintf(full_name_buf + strlen(full_name_buf),filename);
    update_handler(full_name_buf);

    /* Delete file */


    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
}





// update handler updates the firmware from the file saved in spiffs.
esp_err_t update_handler(char* file_name){
    int data_read = 0;
    esp_err_t ret;
    const esp_partition_t *update_partition;
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running  = esp_ota_get_running_partition();
    esp_ota_handle_t well_done_handle = 0;  /* Handler for OTA update. */
    if (configured != running) {
        ESP_LOGW("UPDATE", "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                        configured->address, running->address);
        ESP_LOGW("UPDATE", "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)"
                        );
    }
    ESP_LOGI("UPDATE", "Running partition type %d subtype %d (offset 0x%08x)",
                    running->type, running->subtype, running->address);

    // Open for reading firmware.bin
    FILE* f = fopen(file_name, "r"); 
    //FILE* f = fopen("/spiffs/ELSTAT_ESP_IDF.bin", "r");
    if (f == NULL) {
        ESP_LOGE("UPDATE", "Failed to open ELSTAT_ESP_IDF.bin");
        return ESP_FAIL;
    }
    ESP_LOGI("UPDATE", "Starting OTA example");
    // It finds the partition where it should write the firmware
    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI("UPDATE", "Writing to partition subtype %d at offset 0x%x",
                    update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);
    // Reset of this partition
    ESP_ERROR_CHECK(esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &well_done_handle));
    // Temporary buffer where I write the chunks of file read from the file firmware.bin.
    char buf[SEND_DATA];
    // Init of the buffer
    memset(buf, 0, sizeof(buf));
    /* Loop */
    do{
        // Put the data read from the file inside buf.
        data_read = fread(buf, 1, SEND_DATA, f);
        // I write data from buffer to the partition
        ret = esp_ota_write(well_done_handle, buf, data_read);
        // In case of failure it sends a log and exits.
        if(ret != ESP_OK){
            ESP_LOGE("UPDATE", "Firmware upgrade failed");
            fclose(f);
            while (1) {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            return ESP_FAIL;
        }
    } while(data_read == SEND_DATA);
    // If you are here it means there are no problems!
    ESP_ERROR_CHECK(esp_ota_end(well_done_handle));
    fclose(f);
    // OTA partition configuration
    ESP_ERROR_CHECK(esp_ota_set_boot_partition(update_partition));
    ESP_LOGI("UPDATE", "Restarting...");
    // REBOOT!!!!!
    esp_restart();
    return ESP_OK; // Not so useful :/
}







static esp_err_t rollback_handler(httpd_req_t *req)
{

    char content[100];
    /* Truncate if content length larger than the buffer */
    size_t recv_size = MIN(req->content_len, sizeof(content));

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }
    ESP_LOGI("POST_HANDLER","post request received = %s \n",req->uri);
    /*
    esp_err_t error = esp_ota_mark_app_invalid_rollback_and_reboot();
    if(error == -1){
        const char resp[] = "Already running factory fimware";
        httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        //httpd_resp_sendstr(req, "Already running factory fimware");
    }
    */
    //printf("erro integer = %i \n",error);

    /* Send a simple response */
    return ESP_OK;
}





static esp_err_t handle_brightness_change(httpd_req_t *req) {

    char content[100];
    char brightness_value[10];
    uint8_t brightness_value_num;
    size_t recv_size = MIN(req->content_len, sizeof(content));

    int ret = httpd_req_recv(req, content, recv_size);
    if (httpd_query_key_value((char *) content, "name", brightness_value, (recv_size-4)) != ESP_ERR_NOT_FOUND) {
        ESP_LOGI("POST_HANDLER","key value = %s \n",brightness_value);
        brightness_value_num = strtoul(brightness_value, NULL, 10);
        ESP_LOGI("POST_HANDLER","key value number= %u \n",brightness_value_num);
        RGB_fade_in(brightness_value_num);
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    }

    else if (httpd_query_key_value((char *) content, "red", brightness_value, (recv_size-3)) != ESP_ERR_NOT_FOUND) {
        ESP_LOGI("POST_HANDLER","key value = %s \n",brightness_value);
        uint8_t red = strtoul(brightness_value, NULL, 10);
        ESP_LOGI("POST_HANDLER","key value number= %u \n",red);
        RGB_set_red(red);
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    }

    else if (httpd_query_key_value((char *) content, "green", brightness_value, (recv_size-5)) != ESP_ERR_NOT_FOUND) {
        ESP_LOGI("POST_HANDLER","key value = %s \n",brightness_value);
        uint8_t green = strtoul(brightness_value, NULL, 10);
        ESP_LOGI("POST_HANDLER","key value number= %u \n",green);
        RGB_set_green(green);
        httpd_resp_sendstr(req, "OK");


        return ESP_OK;
    }

    else if (httpd_query_key_value((char *) content, "blue", brightness_value, (recv_size-4)) != ESP_ERR_NOT_FOUND) {
        ESP_LOGI("POST_HANDLER","key value = %s \n",brightness_value);
        uint8_t blue = strtoul(brightness_value, NULL, 10);
        ESP_LOGI("POST_HANDLER","key value number= %u \n",blue);
        RGB_set_blue(blue);
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    }

    else if (httpd_query_key_value((char *) content, "rgb", brightness_value, (recv_size-3)) != ESP_ERR_NOT_FOUND) {
        //ESP_LOGI("POST_HANDLER","key value = %s \n",brightness_value);


        char red_buf[10];
        char green_buf[10];
        char blue_buf[10];
        
        memcpy(red_buf,(brightness_value)+1,2); //SET NEW ASSET ID
        red_buf[2] = 0;

        memcpy(green_buf,(brightness_value)+3,2); //SET NEW ASSET ID
        green_buf[2] = 0;

        memcpy(blue_buf,(brightness_value)+5,2); //SET NEW ASSET ID
        blue_buf[2] = 0;


        uint8_t red = strtoul(red_buf, NULL, 16);
        uint8_t green = strtoul(green_buf, NULL, 16);
        uint8_t blue = strtoul(blue_buf, NULL, 16);

        RGB_set_rgb(red,green,blue);

        //RGB_fade_in(brightness_value_num);\watch
        httpd_resp_sendstr(req, "OK");

        return ESP_OK;
    }
    
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }

    
    /* Send a simple response */
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}







static esp_err_t handle_animation_change(httpd_req_t *req) {
    char content[200];
    char brightness_value[200];

    size_t recv_size = MIN(req->content_len, sizeof(content));

    int ret = httpd_req_recv(req, content, recv_size);
    printf("recv_size = %u \n",recv_size);
    printf("content = %s \n",content);












    if (httpd_query_key_value((char *) content, "name", brightness_value,(recv_size-4)) != ESP_ERR_NOT_FOUND) {
        if(strncmp(brightness_value,"fading",6) == 0){

            
            //check if timer is even created
            if(running_lights_timer != NULL){
                if(esp_timer_is_active(running_lights_timer) == 1){
                    ESP_LOGW("POST_HANDLER","STOPPING running lights timer");
                    ESP_ERROR_CHECK(esp_timer_stop(running_lights_timer));
                }
            }
            else{
                printf("running lights timer does not exist \n");
            }
            
            if(fading_lights_timer != NULL){
                printf("deleting timer and creating a new one \n");
                if(esp_timer_is_active(fading_lights_timer) == 1){
                    ESP_LOGW("POST_HANDLER","STOPPING fading lights timer");
                    ESP_ERROR_CHECK(esp_timer_stop(fading_lights_timer));
                }
            }
            rgb_params.ramp_up_time = 3000;
            rgb_params.color_ramping = 1;
            rgb_params.repeat = 1;
            RGB_fade_in_out(&rgb_params);

        }


        else if(strncmp(brightness_value,"running",7) == 0){
            printf("running word matched \n");
            
            if(fading_lights_timer != NULL){
                if(esp_timer_is_active(fading_lights_timer) == 1){
                    ESP_LOGW("POST_HANDLER","STOPPING fading lights timer");
                    ESP_ERROR_CHECK(esp_timer_stop(fading_lights_timer));
                }
            }
            else{
                printf("fading lights timer does not exist \n");
            }

            if(running_lights_timer != NULL){
                printf("deleting timer and creating a new one \n");
                if(esp_timer_is_active(running_lights_timer) == 1){
                    ESP_LOGW("POST_HANDLER","STOPPING running lights timer");
                    ESP_ERROR_CHECK(esp_timer_stop(running_lights_timer));
                }
            }
            
            
            rgb_params.ramp_up_time = 3000;
            rgb_params.color_ramping = 1;
            rgb_params.repeat = 1;
            RGB_running_lights(&rgb_params);

        }

        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    }



    if (ret <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }

    
    /* Send a simple response */
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}
