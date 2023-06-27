

#include "RGB.h"


static const char *TAG = "example";



static void IRAM_ATTR IR_sensor_handler(void* arg);
static QueueHandle_t gpio_evt_queue = NULL;
TaskHandle_t rainbow_task;



const uint8_t lights[360]=
{
  0,   0,   0,   0,   0,   1,   1,   2, 
  2,   3,   4,   5,   6,   7,   8,   9, 
 11,  12,  13,  15,  17,  18,  20,  22, 
 24,  26,  28,  30,  32,  35,  37,  39, 
 42,  44,  47,  49,  52,  55,  58,  60, 
 63,  66,  69,  72,  75,  78,  81,  85, 
 88,  91,  94,  97, 101, 104, 107, 111, 
114, 117, 121, 124, 127, 131, 134, 137, 
141, 144, 147, 150, 154, 157, 160, 163, 
167, 170, 173, 176, 179, 182, 185, 188, 
191, 194, 197, 200, 202, 205, 208, 210, 
213, 215, 217, 220, 222, 224, 226, 229, 
231, 232, 234, 236, 238, 239, 241, 242, 
244, 245, 246, 248, 249, 250, 251, 251, 
252, 253, 253, 254, 254, 255, 255, 255, 
255, 255, 255, 255, 254, 254, 253, 253, 
252, 251, 251, 250, 249, 248, 246, 245, 
244, 242, 241, 239, 238, 236, 234, 232, 
231, 229, 226, 224, 222, 220, 217, 215, 
213, 210, 208, 205, 202, 200, 197, 194, 
191, 188, 185, 182, 179, 176, 173, 170, 
167, 163, 160, 157, 154, 150, 147, 144, 
141, 137, 134, 131, 127, 124, 121, 117, 
114, 111, 107, 104, 101,  97,  94,  91, 
 88,  85,  81,  78,  75,  72,  69,  66, 
 63,  60,  58,  55,  52,  49,  47,  44, 
 42,  39,  37,  35,  32,  30,  28,  26, 
 24,  22,  20,  18,  17,  15,  13,  12, 
 11,   9,   8,   7,   6,   5,   4,   3, 
  2,   2,   1,   1,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0};












led_strip_handle_t led_strip;
struct rgb_color_s strip_color;
struct rgb_params_s rgb_params; // global variable for passing parameters to timers as arguments

esp_timer_handle_t fading_lights_timer = NULL; // this is main controller task timer 
esp_timer_handle_t running_lights_timer = NULL; // this is main controller task timer 
esp_timer_handle_t rainbow_lights_timer = NULL; // this is main controller task timer

#define LED_STRIP_BLINK_GPIO  14
//#define LED_STRIP_LED_NUMBERS 117
#define LED_STRIP_LED_NUMBERS 20
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)
















static void IRAM_ATTR button_handler(void* arg)
{
    BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
    uint32_t gpio_num = (uint32_t) arg;

    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
    esp_rom_printf("IR sensor handler toggled \n");
    if( pxHigherPriorityTaskWoken )
    {
        portYIELD_FROM_ISR ();
    }
}






void RGB_setup(){



    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT ;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_BUTTON_INPUT_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(2, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(Button_detection_task, "Button_detection_task", 2048, NULL, 10, NULL);

    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(BUTTON_1, button_handler, (void*) BUTTON_1);


    const esp_timer_create_args_t fading_lights = {
            .callback = &RGB_fade_in_out_callback,
            /* name is optional, but may help identify the timer when debugging */
            .arg = &rgb_params,
            .name = "fade_in_out"
    };
    ESP_ERROR_CHECK(esp_timer_create(&fading_lights, &fading_lights_timer));


    const esp_timer_create_args_t running_lights = {
        .callback = &RGB_running_lights_callback,
        /* name is optional, but may help identify the timer when debugging */
        .arg = &rgb_params,
        .name = "running lights"
    };
    ESP_ERROR_CHECK(esp_timer_create(&running_lights, &running_lights_timer));


    const esp_timer_create_args_t rainbow_lights = {
        .callback = &RGB_rainbow_lights_callback,
        /* name is optional, but may help identify the timer when debugging */
        .arg = &rgb_params,
        .name = "rainbow lights"
    };
    ESP_ERROR_CHECK(esp_timer_create(&rainbow_lights, &rainbow_lights_timer));


    strip_color.red = 0;
    strip_color.blue = 0;
    strip_color.green = 0;

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,   // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_NUMBERS,        // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    bool led_on_off = false;
    ESP_LOGI(TAG, "Start blinking LED strip");
    ESP_ERROR_CHECK(led_strip_clear(led_strip));
}





void RGB_fade_in(uint8_t brigthness){
    ESP_ERROR_CHECK(led_strip_clear(led_strip));
    for(int i = 0; i <LED_STRIP_LED_NUMBERS;i++){
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, brigthness, brigthness, brigthness));
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
    
}


void RGB_set_red(uint8_t color){
    strip_color.red = color;
    // ESP_ERROR_CHECK(led_strip_clear(led_strip));
    // for(int i = 0; i <LED_STRIP_LED_NUMBERS;i++){
    //     ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, strip_color.red, strip_color.green, strip_color.blue));
    // }
    // ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
}


void RGB_set_green(uint8_t color){
    strip_color.green = color;
    // ESP_ERROR_CHECK(led_strip_clear(led_strip));
    // for(int i = 0; i <LED_STRIP_LED_NUMBERS;i++){
    //     ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, strip_color.red, strip_color.green, strip_color.blue));
    // }
    // ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
}

void RGB_set_blue(uint8_t color){
    strip_color.blue = color;
    // ESP_ERROR_CHECK(led_strip_clear(led_strip));
    // for(int i = 0; i <LED_STRIP_LED_NUMBERS;i++){
    //     ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, strip_color.red, strip_color.green, strip_color.blue));
    // }
    // ESP_ERROR_CHECK(led_strip_refresh(led_strip));  
}




void RGB_set_rgb(uint8_t red,uint8_t green,uint8_t blue){
    strip_color.red = red;
    strip_color.green = green;
    strip_color.blue = blue;
    ESP_ERROR_CHECK(led_strip_clear(led_strip));
    for(int i = 0; i <LED_STRIP_LED_NUMBERS;i++){
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, strip_color.red, strip_color.green, strip_color.blue));
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
    
}

void RGB_clear_strip(){
    ESP_ERROR_CHECK(led_strip_clear(led_strip));
}






void RGB_fade_in_out_callback(void* arg)
{
   // printf("fade in out callback \n");
    struct rgb_params_s* rgb_params_local = (struct rgb_params_s*)arg;
    static bool fade_direction = 0;
    static uint8_t counter = 0;

    if(fade_direction == 0){
        for(int j = 0; j <LED_STRIP_LED_NUMBERS;j++){ // GO THROUGH EACH RGB AND SET COLOR
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, j, ((float)strip_color.red/255.0f)*(float)counter, ((float)strip_color.green/255.0f)*(float)counter, ((float)strip_color.blue/255.0f)*(float)counter));

        }
        counter++;

        ESP_ERROR_CHECK(led_strip_refresh(led_strip));  
        if(counter == 255){
            fade_direction = 1;
        }       
    }

    else if(fade_direction == 1){
        counter --;
        for(int j = 0; j <LED_STRIP_LED_NUMBERS; j++){ // GO THROUGH EACH RGB AND SET COLOR
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, j, ((float)strip_color.red/255.0f)*(float)counter, ((float)strip_color.green/255.0f)*(float)counter, ((float)strip_color.blue/255.0f)*(float)counter)); 

        }

        ESP_ERROR_CHECK(led_strip_refresh(led_strip));   
        if(counter == 0){
            fade_direction = 0;
        }          
    }

}

void RGB_running_lights_callback(void* arg)
{
    //printf("hello \n");
    struct rgb_params_s* rgb_params_local = (struct rgb_params_s*)arg;
    static bool fade_direction = 0;
    static uint8_t color_value = 0; // only used with color ramping
    static uint8_t led_number = 0;

        
    
    //TURN ON ALL LEDS ONE BY ONE
    if(fade_direction == 0){
 
        if(rgb_params_local->color_ramping > 0){
            // calcualte color value increments based on RGB_NUM
            uint8_t red_ramp = ((float)strip_color.red/(float)LED_STRIP_LED_NUMBERS)*(float)led_number;
            uint8_t green_ramp = ((float)strip_color.green/(float)LED_STRIP_LED_NUMBERS)*(float)led_number;
            uint8_t blue_ramp =  ((float)strip_color.blue/(float)LED_STRIP_LED_NUMBERS)*(float)led_number;
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_number, red_ramp, green_ramp, blue_ramp)); 
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));  
        }
        else{
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_number, strip_color.red, strip_color.green, strip_color.blue)); 
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));   
        }
        led_number ++;
        if(led_number >= LED_STRIP_LED_NUMBERS){
            fade_direction = 1;
        }       
    }

    // TURN OFF ALL LEDS ONE BY ONE
    else if(fade_direction == 1){
        led_number --;
        if(rgb_params_local->color_ramping > 0){
            uint8_t red_ramp = ((float)strip_color.red/(float)LED_STRIP_LED_NUMBERS)*(float)led_number;
            uint8_t green_ramp = ((float)strip_color.green/(float)LED_STRIP_LED_NUMBERS)*(float)led_number;
            uint8_t blue_ramp =  ((float)strip_color.blue/(float)LED_STRIP_LED_NUMBERS)*(float)led_number;
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_number, 0, 0, 0)); 
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));   
        }
        else{
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_number, 0, 0, 0)); 
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));  
        }

        if(led_number == 0){
            fade_direction = 0;
        }       
    }

}







    // uint16_t offset = 360/LED_STRIP_LED_NUMBERS;
    // for (;;)
	// {
    //     for (int k=0; k<360; k++)
    //     { 
    //         for(int i = 0; i < LED_STRIP_LED_NUMBERS;i++){
    //             sineLED(i,k+(offset*i));
    //         } 
    //         ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
    //         vTaskDelay(30/portTICK_PERIOD_MS);
    //     }
    // }


void RGB_rainbow_lights_callback(void* arg)
{
    static uint16_t offset = 360/LED_STRIP_LED_NUMBERS;
    static uint8_t led_number = 0;
    static uint16_t angle = 0;


    for(int i = 0; i < LED_STRIP_LED_NUMBERS;i++){
        sineLED(i,angle+(offset*i));
    }
    angle++;
    if(angle == 359){
        angle = 0;
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
    
    
}





// /**
//  * @brief 
//  * RGB_fade_in_out is simillar tu running lights except that it controls all leds at once whereas running lights conrol LEDS one by one
//  * @param rgb_parameters 
//  * The parameters that you must pass to fade in out function:
//  * color_red -> the target red color
//  * color_green -> the target green color
//  * color_blue -> the target_blue color
//  * ramp_up_time -> time it takes to reach target color and time to reach 0 0 0. 
//  * Example usage:
//  * 
//     rgb_params.ramp_up_time = 3000; takes 3 seconds to reach target and another 3 seconds to fade down
//     rgb_params.red_color = 204;
//     rgb_params.green_color = 51;
//     rgb_params.blue_color = 255;
//     RGB_fade_in_out(&rgb_params);
//  * 
//  */
void RGB_fade_in_out(struct rgb_params_s* rgb_parameters){
    if(esp_timer_is_active(fading_lights_timer) == 0){
        ESP_ERROR_CHECK(esp_timer_start_periodic(fading_lights_timer, ((rgb_parameters->ramp_up_time/255)*1000))); //
    }
}



//  * @brief 
//  *  RGB_running_lights Control leds one by one
//  * @param rgb_parameters 
//  * The parameters that you must pass to fade in out function:
//  * color_red -> the target red color
//  * color_green -> the target green color
//  * color_blue -> the target_blue color
//  * ramp_up_time -> time it takes to reach target color and time to reach 0 0 0. 
//  * color_ramping 1/0 
//  * Example usage:
//  * 
//     rgb_params.ramp_up_time = 3000; takes 3 seconds to reach target and another 3 seconds to fade down
//     rgb_params.red_color = 204;
//     rgb_params.green_color = 51;
//     rgb_params.blue_color = 0;
//     rgb_params.color_ramping = 1;
//     RGB_running_lights(&rgb_params);
//  * 
//  */
// //this function sets the required timer interval for ramp up/ ramp down time
void RGB_running_lights(struct rgb_params_s* rgb_parameters){
// if you want 44 LEDS to turn ON in 3 seconds, each LED will have:
// 3000/44 = 68 ms time
    if(esp_timer_is_active(running_lights_timer) == 0){
        ESP_ERROR_CHECK(esp_timer_start_periodic(running_lights_timer, ((rgb_parameters->ramp_up_time/LED_STRIP_LED_NUMBERS)*1000))); //
        printf("running lights timer started \n");
    }
}


void RGB_rainbow_lights(struct rgb_params_s* rgb_parameters){

    if(esp_timer_is_active(rainbow_lights_timer) == 0){
        ESP_ERROR_CHECK(esp_timer_start_periodic(rainbow_lights_timer, rgb_parameters->ramp_up_time*10)); //
        printf("rainbow lights timer started \n");
    }
}





void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

void sineLED(uint16_t LED, int angle)
{
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip,LED, lights[(angle+120)%360], lights[(angle+0)%360],  lights[(angle+240)%360]));
}





void RGB_fading_up(){
    for(int i = 0; i< 150;i++){
        for(int j = 0; j <LED_STRIP_LED_NUMBERS;j++)
        {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, j, i, i, i));
        }
        ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
        vTaskDelay(20/portTICK_PERIOD_MS);
    }
    vTaskDelete(0);
}

void RGB_meet_in_the_middle(void *argument){
    uint8_t led_num = 0;
    for (;;)
	{
        //printf("led_num = %u \n",led_num);
        //printf("(LED_STRIP_LED_NUMBERS-1)-led_num = %u \n",(LED_STRIP_LED_NUMBERS-1)-led_num);
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_num, 150, 150, 150)); //violet
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, (LED_STRIP_LED_NUMBERS-1)-led_num, 150, 150, 150)); //violet
        led_num ++;
        ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
        vTaskDelay(50/portTICK_PERIOD_MS);

        //if value does not divide by 2, turn on the last led
        if(led_num >= (LED_STRIP_LED_NUMBERS/2)){
            if(LED_STRIP_LED_NUMBERS %2 != 0){
                ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_num, 150, 150, 150)); //violet
                ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
            }
            vTaskDelete(0);
        }
        
    }
}


void RGB_turn_index_led(uint8_t index, uint8_t red, uint8_t green, uint8_t blue){
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, index, red, green, blue)); //violet
    ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
}




void Stop_current_animation(){
    if(fading_lights_timer != NULL){
        if(esp_timer_is_active(fading_lights_timer) == 1){
            ESP_LOGW("RGB","STOPPING fading lights timer");
            ESP_ERROR_CHECK(esp_timer_stop(fading_lights_timer));
            ESP_ERROR_CHECK(led_strip_clear(led_strip));
        }
    }
    else{
        printf("fading lights timer is null \n");
    }


    if(running_lights_timer != NULL){
        if(esp_timer_is_active(running_lights_timer) == 1){
            ESP_LOGW("RGB","STOPPING running lights timer");
            ESP_ERROR_CHECK(esp_timer_stop(running_lights_timer));
            ESP_ERROR_CHECK(led_strip_clear(led_strip));
        }
    }
    else{
        printf("running lights timer is null \n");
    }

    if(rainbow_lights_timer != NULL){
        if(esp_timer_is_active(rainbow_lights_timer) == 1){
            ESP_LOGW("RGB","STOPPING rainbow lights timer");
            ESP_ERROR_CHECK(esp_timer_stop(rainbow_lights_timer));
            ESP_ERROR_CHECK(led_strip_clear(led_strip));
        }
    }
    else{
        printf("rainbow lights timer is null \n");
    }

}


void Button_detection_task(void* arg)
{
    
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) { 

            printf("button click detected \n");
            //vTaskDelay(100/portTICK_PERIOD_MS);
        }
    }
}