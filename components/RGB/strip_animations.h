
#ifndef STRIP_ANIMATIONS_H
#define STRIP_ANIMATIONS_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "RGB.h"


struct rgb_params_s{
    uint16_t ramp_up_time;
    bool color_ramping;
    bool repeat;
};


void Initialize_animation_timers();


void RGB_fade_in_out_callback();
void RGB_running_lights_callback(void* arg);
void RGB_rainbow_lights_callback(void* arg);

void RGB_running_lights(struct rgb_params_s* rgb_parameters);
void RGB_fade_in_out();
void RGB_rainbow_lights(struct rgb_params_s* rgb_parameters);

void Start_animation_by_index(enum animation_index_e animation_index);
enum animation_index_e Stop_current_animation();
void Get_current_animation_speed();

#endif