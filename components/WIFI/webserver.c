

#include "webserver.h"





static esp_err_t handle_brightness_change(httpd_req_t *req);
static esp_err_t handle_animation_change(httpd_req_t *req);


/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (1500*1024) // 200 KB
#define MAX_FILE_SIZE_STR "1500KB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  4096
#define SEND_DATA   2048 // data is being written from spiffs to ota partition in 2048b chunks

struct file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};

static const char *TAG = "file_server";

/* Handler to redirect incoming GET request for /index.html to /
 * This can be overridden by uploading file with same name */
static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);  // Response body can be empty
    return ESP_OK;
}

/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico.
 * This can be overridden by uploading file with same name */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

/* Send HTTP response with a run-time generated html consisting of
 * a list of all files and folders under the requested path.
 * In case of SPIFFS this returns empty list when path is any
 * string other than '/', since SPIFFS doesn't support directories */
static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath)
{
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
        ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }

    /* Send HTML file header */
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");

    /* Get handle to embedded file upload script */
    extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
    extern const unsigned char upload_script_end[]   asm("_binary_upload_script_html_end");
    const size_t upload_script_size = (upload_script_end - upload_script_start);

    /* Add file upload form and script which on execution sends a POST request to /upload */
    httpd_resp_send_chunk(req, (const char *)upload_script_start, upload_script_size);

    /* Send file-list table definition and column labels */
    httpd_resp_sendstr_chunk(req,
        "<table class=\"fixed\" border=\"1\">"
        "<col width=\"800px\" /><col width=\"300px\" /><col width=\"300px\" /><col width=\"100px\" />"
        "<thead><tr><th>Name</th><th>Type</th><th>Size (Bytes)</th><th>Delete</th></tr></thead>"
        "<tbody>");

    /* Iterate over all files / folders and fetch their names and sizes */
    while ((entry = readdir(dir)) != NULL) {
        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
        if (stat(entrypath, &entry_stat) == -1) {
            ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
            continue;
        }
        sprintf(entrysize, "%ld", entry_stat.st_size);
        ESP_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);

        /* Send chunk of HTML file containing table entries with file name and size */
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

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
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










/* Handler to download a file kept on the server */
static esp_err_t download_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri, sizeof(filepath));
    if (!filename) {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* If name has trailing '/', respond with directory contents */
    if (filename[strlen(filename) - 1] == '/') {
        return http_resp_dir_html(req, filepath);
    }

    if (stat(filepath, &file_stat) == -1) {
        /* If file not present on SPIFFS check if URI
         * corresponds to one of the hardcoded paths */
        if (strcmp(filename, "/index.html") == 0) {
            return index_html_get_handler(req);
        } 
        else if (strcmp(filename, "/favicon.ico") == 0) {
            return favicon_get_handler(req);
        }
        else if(strcmp(filename,"/styles.css")== 0){
            return styles_handler(req);
        }
        
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
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
                ESP_LOGE(TAG, "File sending failed!");
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
    ESP_LOGI(TAG, "File sending complete");

    /* Respond with an empty chunk to signal HTTP response completion */
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_send_chunk(req, NULL, 0);
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
        // ESP_LOGW("UPDATE", "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
        //                 configured->address, running->address);
        // ESP_LOGW("UPDATE", "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)"
        //                 );
    }
    // ESP_LOGI("UPDATE", "Running partition type %d subtype %d (offset 0x%08x)",
    //                 running->type, running->subtype, running->address);

    // ESP_LOGI("UPDATE", "Starting OTA example");
    // It finds the partition where it should write the firmware
    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI("UPDATE", "Writing to partition subtype %d at offset 0x%lu",
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
















/* Handler to upload a file onto the server */
static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    /* Skip leading "/upload" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri + sizeof("/upload") - 1, sizeof(filepath));
    if (!filename) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/') {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == 0) {
        ESP_LOGE(TAG, "File already exists : %s", filepath);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        return ESP_FAIL;
    }

    /* File cannot be larger than a limit */
    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than "
                            MAX_FILE_SIZE_STR "!");
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to create file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Receiving file : %s...", filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;

    while (remaining > 0) {

        ESP_LOGI(TAG, "Remaining size : %d", remaining);
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error,
             * close and delete the unfinished file*/
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File reception failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        /* Write buffer content to file on storage */
        if (received && (received != fwrite(buf, 1, received, fd))) {
            /* Couldn't write everything to file!
             * Storage may be full? */
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File write failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    /* Close file upon upload completion */
    fclose(fd);
    ESP_LOGI(TAG, "File reception complete");

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}

/* Handler to delete a file from the server */
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
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "File does not exist : %s", filename);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Deleting file : %s", filename);
    /* Delete file */
    unlink(filepath);

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
}

/* Function to start the file server */
esp_err_t example_start_file_server(const char *base_path)
{
    static struct file_server_data *server_data = NULL;

    if (server_data) {
        ESP_LOGE(TAG, "File server already started");
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate memory for server data */
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }
    strlcpy(server_data->base_path, base_path,
            sizeof(server_data->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8096;

    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start file server!");
        return ESP_FAIL;
    }

    /* URI handler for getting uploaded files */
    httpd_uri_t file_download = {
        .uri       = "/*",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = download_get_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_download);


        httpd_uri_t uri_brightness = {
        .uri      = "/brightness",
        .method   = HTTP_POST,
        .handler  = handle_brightness_change,
        .user_ctx = server_data
    };
    httpd_register_uri_handler(server, &uri_brightness);

    httpd_uri_t uri_animation = {
        .uri      = "/animation",
        .method   = HTTP_POST,
        .handler  = handle_animation_change,
        .user_ctx = server_data
    };
    httpd_register_uri_handler(server, &uri_animation);




    httpd_uri_t file_upload_flash = {
        .uri       = "/upload_flash/*",   // Match all URIs of type /upload/path/to/file
        .method    = HTTP_POST,
        .handler   = upload_and_flash_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_upload_flash);


    /* URI handler for uploading files to server */
    httpd_uri_t file_upload = {
        .uri       = "/upload/*",   // Match all URIs of type /upload/path/to/file
        .method    = HTTP_POST,
        .handler   = upload_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_upload);

    /* URI handler for deleting files from server */
    httpd_uri_t file_delete = {
        .uri       = "/delete/*",   // Match all URIs of type /delete/path/to/file
        .method    = HTTP_POST,
        .handler   = delete_post_handler,
        .user_ctx  = server_data    // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_delete);

    return ESP_OK;
}




esp_err_t example_mount_storage(const char* base_path)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = base_path,
        .partition_label = NULL,
        .max_files = 5,   // This sets the maximum number of files that can be open at the same time
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    return ESP_OK;
}







static esp_err_t handle_brightness_change(httpd_req_t *req) {

    char content[100];
    char brightness_value[10];
    uint8_t brightness_value_num;
    size_t recv_size = MIN(req->content_len, sizeof(content));

    int ret = httpd_req_recv(req, content, recv_size);
    if (httpd_query_key_value((char *) content, "name", brightness_value, (recv_size-4)) != ESP_ERR_NOT_FOUND) {
        //ESP_LOGI("POST_HANDLER","key value = %s \n",brightness_value);
        brightness_value_num = strtoul(brightness_value, NULL, 10);
        //ESP_LOGI("POST_HANDLER","key value number= %u \n",brightness_value_num);
        RGB_fade_in(brightness_value_num);
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    }

    else if (httpd_query_key_value((char *) content, "red", brightness_value, (recv_size-3)) != ESP_ERR_NOT_FOUND) {
        //ESP_LOGI("POST_HANDLER","RED");
        uint8_t red = strtoul(brightness_value, NULL, 10);
        //ESP_LOGI("POST_HANDLER","key value number= %u",red);
        RGB_set_red(red);
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    }

    else if (httpd_query_key_value((char *) content, "green", brightness_value, (recv_size-5)) != ESP_ERR_NOT_FOUND) {
        //ESP_LOGI("POST_HANDLER","GREEN");
        uint8_t green = strtoul(brightness_value, NULL, 10);
        //ESP_LOGI("POST_HANDLER","key value number= %u ",green);
        RGB_set_green(green);
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    }

    else if (httpd_query_key_value((char *) content, "blue", brightness_value, (recv_size-4)) != ESP_ERR_NOT_FOUND) {
        //ESP_LOGI("POST_HANDLER","BLUE");
        uint8_t blue = strtoul(brightness_value, NULL, 10);
        //ESP_LOGI("POST_HANDLER","key value number= %u ",blue);
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


extern esp_timer_handle_t fading_lights_timer; // this is main controller task timer 
extern esp_timer_handle_t running_lights_timer; // this is main controller task timer 
extern struct rgb_color_s strip_color;

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