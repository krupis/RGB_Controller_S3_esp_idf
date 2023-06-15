
#ifndef RGB_H
#define RGB_H


#include "led_strip.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/queue.h"



#define BUTTON_1                     GPIO_NUM_36

#define GPIO_BUTTON_INPUT_SEL  ( (1ULL<<BUTTON_1))

void RGB_setup();
void RGB_fade_in(uint8_t brigthness);
void RGB_set_red(uint8_t color);
void RGB_set_green(uint8_t color);
void RGB_set_blue(uint8_t color);
void RGB_set_rgb(uint8_t red,uint8_t green,uint8_t blue);
void RGB_turn_index_led(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);


void RGB_fade_in_out_callback(void* arg);
void RGB_running_lights_callback(void* arg);
void RGB_running_rainbow(void *argument);
void RGB_sine_rainbow(void *argument);
void sineLED(uint16_t LED, int angle);
void sineLED_offset(uint16_t LED, int angle,int offset);




struct rgb_color_s{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct rgb_params_s{
    uint16_t ramp_up_time;
    uint8_t red_color;
    uint8_t green_color;
    uint8_t blue_color;
    bool color_ramping;
    bool repeat;
};


void RGB_running_lights(struct rgb_params_s* rgb_parameters);
void RGB_fade_in_out(struct rgb_params_s* rgb_parameters);
void RGB_meet_in_the_middle(void *argument);
void RGB_fading_up();

extern struct rgb_params_s rgb_params;
extern struct rgb_color_s strip_color;


void Button_detection_task(void* arg);

#endif