

#include "RGB.h"


static const char *TAG = "example";


led_strip_handle_t led_strip;
struct rgb_color_s strip_color;
struct rgb_params_s rgb_params; // global variable for passing parameters to timers as arguments

esp_timer_handle_t fading_lights_timer = NULL; // this is main controller task timer 
esp_timer_handle_t running_lights_timer = NULL; // this is main controller task timer 

#define LED_STRIP_BLINK_GPIO  14
#define LED_STRIP_LED_NUMBERS 50
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)



// // esp_timer_handle_t fading_lights_timer = NULL; // this is main controller task timer 
// // esp_timer_handle_t running_lights_timer = NULL; // this is main controller task timer 


void RGB_setup(){

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



    // rgb_params.ramp_up_time = 10000; //takes 3 seconds to reach target and another 3 seconds to fade down
    // rgb_params.red_color = 204;
    // rgb_params.green_color = 51;
    // rgb_params.blue_color = 255;

    
    // RGB_fade_in_out(&rgb_params);

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
    ESP_ERROR_CHECK(led_strip_clear(led_strip));
    for(int i = 0; i <LED_STRIP_LED_NUMBERS;i++){
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, strip_color.red, strip_color.green, strip_color.blue));
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
}


void RGB_set_green(uint8_t color){
    strip_color.green = color;
    ESP_ERROR_CHECK(led_strip_clear(led_strip));
    for(int i = 0; i <LED_STRIP_LED_NUMBERS;i++){
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, strip_color.red, strip_color.green, strip_color.blue));
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
}

void RGB_set_blue(uint8_t color){
    strip_color.blue = color;
    ESP_ERROR_CHECK(led_strip_clear(led_strip));
    for(int i = 0; i <LED_STRIP_LED_NUMBERS;i++){
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, strip_color.red, strip_color.green, strip_color.blue));
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));  
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
    }
}


