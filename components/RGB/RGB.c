

#include "RGB.h"


static const char *TAG = "example";






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

#define LED_STRIP_BLINK_GPIO  14
#define LED_STRIP_LED_NUMBERS 117
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)























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

    for(int i = 0; i< 150;i++){
        for(int j = 0; j <LED_STRIP_LED_NUMBERS;j++)
        {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, j, i, i, i));
        }
        ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
        vTaskDelay(20/portTICK_PERIOD_MS);
    }

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










void RGB_running_rainbow(void *argument)
{   
    uint8_t mode = 3; 
    uint8_t rainbow_segments = 7; //red,ornage,yellow,green,blue,indigo,violet
    uint8_t index_test = 0;
    uint16_t led_index = 0;
    
    // 0 means back and forth
    // 1 means go forward and start over without going backwards
    bool direction = 0;

    uint16_t led_index_red_ro;
    uint16_t led_index_orange_ro;
    uint16_t led_index_yellow_ro;
    uint16_t led_index_green_ro;
    uint16_t led_index_blue_ro;
    uint16_t led_index_indigo_ro;
    uint16_t led_index_violet_ro;


    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    uint16_t hue = 0;
    uint16_t start_rgb = 0;


  	for (;;)
	{	
        //MODE 0
        if (mode == 0){
            if(direction == 0){
                led_index++;
                if(led_index >= LED_STRIP_LED_NUMBERS){
                    direction = 1;
                }
                else{
                    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_index, 100, 0, 0));
                }
            }
            
            else if(direction == 1){
                led_index--;
                if(led_index <= 0){
                    direction = 0;
                }
                else{
                ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_index, 0, 0, 0));
                }
            }
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));   
        }


        //MODE 1
        else if (mode == 1){

            ESP_ERROR_CHECK(led_strip_clear(led_strip));

            led_index_red_ro = (led_index % LED_STRIP_LED_NUMBERS);
            led_index_orange_ro = ( (led_index+1) % LED_STRIP_LED_NUMBERS);
            led_index_yellow_ro = ( (led_index+2) % LED_STRIP_LED_NUMBERS);
            led_index_green_ro = ( (led_index+3) % LED_STRIP_LED_NUMBERS);
            led_index_blue_ro = ( (led_index+4) % LED_STRIP_LED_NUMBERS);
            led_index_indigo_ro = ( (led_index+5) % LED_STRIP_LED_NUMBERS);
            led_index_violet_ro = ( (led_index+6) % LED_STRIP_LED_NUMBERS);


            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_index_red_ro, 255, 0, 0));//red
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_index_orange_ro, 255, 127, 0)); //orange
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_index_yellow_ro, 255, 255, 20)); //yellow
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_index_green_ro, 0, 255, 0)); //green
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_index_blue_ro, 0, 0, 255)); //blue
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_index_indigo_ro, 75, 0, 130)); //indigo
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_index_violet_ro, 148, 0, 211)); //violet
            led_index++;

            ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
        }



        //MODE 1
        else if (mode == 2){
            //divide total leds by the rainbow segments to determine how many LEDS per segment
            ESP_ERROR_CHECK(led_strip_clear(led_strip));
            uint8_t leds_per_segment = LED_STRIP_LED_NUMBERS/rainbow_segments;

            ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
        }





        else if (mode == 3){

                for (int j = 0; j < LED_STRIP_LED_NUMBERS; j ++) {
                    // Build RGB values
                    hue = j * 360 / LED_STRIP_LED_NUMBERS + start_rgb;
                    led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
                    // Write RGB values to strip driver
                    //ESP_ERROR_CHECK(strip->set_pixel(strip, j, red, green, blue));
                    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, j, red, green, blue)); //violet
                }
                // Flush RGB values to LEDs

                ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
                vTaskDelay(pdMS_TO_TICKS(1000));
                ESP_ERROR_CHECK(led_strip_clear(led_strip));
                vTaskDelay(pdMS_TO_TICKS(1000));
            
            start_rgb += 60;
        }
		//vTaskDelay(50/portTICK_PERIOD_MS);
        
    }

}



void RGB_sine_rainbow(void *argument)
{   
    uint16_t offset = 360/LED_STRIP_LED_NUMBERS;

    for (;;)
	{
        //ESP_ERROR_CHECK(led_strip_clear(led_strip));

        for (int k=0; k<360; k++)
        { 
            for(int i = 0; i < LED_STRIP_LED_NUMBERS;i++){
                sineLED(i,k+(offset*i));
            } 
            ESP_ERROR_CHECK(led_strip_refresh(led_strip)); 
            vTaskDelay(30/portTICK_PERIOD_MS);
        }
    }
}


void sineLED(uint16_t LED, int angle)
{
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip,LED, lights[(angle+120)%360], lights[(angle+0)%360],  lights[(angle+240)%360]));
}

