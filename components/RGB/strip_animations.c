

#include "strip_animations.h"
#include "RGB.h"

struct rgb_params_s rgb_params; // global variable for passing parameters to timers as arguments

bool handle_timer_stop = 0;

esp_timer_handle_t fading_lights_timer = NULL;  // this is main controller task timer
esp_timer_handle_t running_lights_timer = NULL; // this is main controller task timer
esp_timer_handle_t rainbow_lights_timer = NULL; // this is main controller task timer

// EXTERNS
extern led_strip_handle_t led_strip;
// END OF EXTERNS

void Initialize_animation_timers()
{

    const esp_timer_create_args_t fading_lights = {
        .callback = &RGB_fade_in_out_callback,
        /* name is optional, but may help identify the timer when debugging */
        .arg = NULL,
        .name = "fade_in_out"};
    ESP_ERROR_CHECK(esp_timer_create(&fading_lights, &fading_lights_timer));

    const esp_timer_create_args_t running_lights = {
        .callback = &RGB_running_lights_callback,
        /* name is optional, but may help identify the timer when debugging */
        .arg = &rgb_params,
        .name = "running lights"};
    ESP_ERROR_CHECK(esp_timer_create(&running_lights, &running_lights_timer));

    const esp_timer_create_args_t rainbow_lights = {
        .callback = &RGB_rainbow_lights_callback,
        /* name is optional, but may help identify the timer when debugging */
        .arg = &rgb_params,
        .name = "rainbow lights"};
    ESP_ERROR_CHECK(esp_timer_create(&rainbow_lights, &rainbow_lights_timer));
}

// CALLBACKS

void RGB_fade_in_out_callback()
{
    static bool fade_direction = 0;
    static uint8_t counter = 0;

    if (fade_direction == 0)
    {
        for (int j = 0; j < LED_STRIP_LED_NUMBERS; j++)
        { // GO THROUGH EACH RGB AND SET COLOR
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, j, (((float)strip_color.red / 255.0f) * (float)counter) * strip_color.brightness, (((float)strip_color.green / 255.0f) * (float)counter) * strip_color.brightness, (((float)strip_color.blue / 255.0f) * (float)counter) * strip_color.brightness));
        }
        counter++;

        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        if (counter == 255)
        {
            fade_direction = 1;
        }
    }

    else if (fade_direction == 1)
    {
        counter--;
        for (int j = 0; j < LED_STRIP_LED_NUMBERS; j++)
        {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, j, (((float)strip_color.red / 255.0f) * (float)counter) * strip_color.brightness, (((float)strip_color.green / 255.0f) * (float)counter) * strip_color.brightness, (((float)strip_color.blue / 255.0f) * (float)counter) * strip_color.brightness));
        }

        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        if (counter == 0)
        {
            fade_direction = 0;
        }
    }
    if (handle_timer_stop == 1)
    {
        ESP_ERROR_CHECK(esp_timer_stop(fading_lights_timer));
        ESP_ERROR_CHECK(led_strip_clear(led_strip));
        handle_timer_stop = 0;
    }
}

void RGB_running_lights_callback(void *arg)
{
    const struct rgb_params_s *rgb_params_local = (struct rgb_params_s *)arg;
    static bool fade_direction = 0;
    static uint8_t led_number = 0;
    // TURN ON ALL LEDS ONE BY ONE
    if (fade_direction == 0)
    {
        if (rgb_params_local->color_ramping == 1)
        {
            // calcualte color value increments based on RGB_NUM
            uint8_t red_ramp = (uint8_t)(((float)strip_color.red / (float)LED_STRIP_LED_NUMBERS) * (float)led_number * strip_color.brightness);
            uint8_t green_ramp = (uint8_t)(((float)strip_color.green / (float)LED_STRIP_LED_NUMBERS) * (float)led_number * strip_color.brightness);
            uint8_t blue_ramp = (uint8_t)(((float)strip_color.blue / (float)LED_STRIP_LED_NUMBERS) * (float)led_number * strip_color.brightness);
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_number, red_ramp, green_ramp, blue_ramp));
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        }
        else
        {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_number, strip_color.red, strip_color.green, strip_color.blue));
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        }
        led_number++;
        if (led_number >= LED_STRIP_LED_NUMBERS)
        {
            fade_direction = 1;
        }
    }

    // TURN OFF ALL LEDS ONE BY ONE
    else if (fade_direction == 1)
    {
        led_number--;
        if (rgb_params_local->color_ramping == 1)
        {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_number, 0, 0, 0));
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        }
        else
        {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_number, 0, 0, 0));
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        }

        if (led_number == 0)
        {
            fade_direction = 0;
        }
    }
    if (handle_timer_stop == 1)
    {
        ESP_ERROR_CHECK(esp_timer_stop(running_lights_timer));
        ESP_ERROR_CHECK(led_strip_clear(led_strip));
        handle_timer_stop = 0;
    }
}

void RGB_rainbow_lights_callback(void *arg)
{
    static uint16_t offset = 360 / LED_STRIP_LED_NUMBERS;
    static uint16_t angle = 0;
    for (uint16_t i = 0; i < LED_STRIP_LED_NUMBERS; i++)
    {
        sineLED(i, angle + (offset * i));
    }
    angle++;
    if (angle == 359)
    {
        angle = 0;
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    if (handle_timer_stop == 1)
    {
        ESP_ERROR_CHECK(esp_timer_stop(rainbow_lights_timer));
        ESP_ERROR_CHECK(led_strip_clear(led_strip));
        handle_timer_stop = 0;
    }
}

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
void RGB_fade_in_out()
{
    if (esp_timer_is_active(fading_lights_timer) == 0)
    {
        ESP_ERROR_CHECK(esp_timer_start_periodic(fading_lights_timer, (uint64_t)((float)((float)rgb_params.ramp_up_time / 255.0f) * 1000.0f))); //
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
void RGB_running_lights(struct rgb_params_s *rgb_parameters)
{
    if (esp_timer_is_active(running_lights_timer) == 0)
    {
        ESP_ERROR_CHECK(esp_timer_start_periodic(running_lights_timer, ((rgb_parameters->ramp_up_time / LED_STRIP_LED_NUMBERS) * 1000))); //
        printf("running lights timer started \n");
    }
}

void RGB_rainbow_lights(struct rgb_params_s *rgb_parameters)
{
    if (esp_timer_is_active(rainbow_lights_timer) == 0)
    {
        ESP_ERROR_CHECK(esp_timer_start_periodic(rainbow_lights_timer, rgb_parameters->ramp_up_time * 10)); //
        printf("rainbow lights timer started \n");
    }
}

// TODO: start animation by index
void Start_animation_by_index(enum animation_index_e animation_index)
{
    // only allowed if handle timer stop is not set
    if (handle_timer_stop == 0)
    {
        switch (animation_index)
        {
        case FADING:
        {
            RGB_fade_in_out();
        }
        break;
        case RUNNING:
        {
            RGB_running_lights(&rgb_params);
        }
        break;
        case RAINBOW:
        {
            RGB_rainbow_lights(&rgb_params);
        }
        break;
        default:
        {
            printf("unknown \n");
        }
        }
    }
    else
    {
        printf("handle timer stop is set \n");
    }
    // will be done
}

// return index based on which animation has been stopped
enum animation_index_e Stop_current_animation()
{
    if (fading_lights_timer != NULL)
    {
        if (esp_timer_is_active(fading_lights_timer) == 1)
        {
            ESP_LOGW("RGB", "STOPPING fading lights timer");
            handle_timer_stop = 1;
            return FADING;
        }
    }
    else
    {
        printf("fading lights timer is null \n");
    }

    if (running_lights_timer != NULL)
    {
        if (esp_timer_is_active(running_lights_timer) == 1)
        {
            ESP_LOGW("RGB", "STOPPING running lights timer");
            handle_timer_stop = 1;
            return RUNNING;
        }
    }
    else
    {
        printf("running lights timer is null \n");
    }

    if (rainbow_lights_timer != NULL)
    {
        if (esp_timer_is_active(rainbow_lights_timer) == 1)
        {
            ESP_LOGW("RGB", "STOPPING rainbow lights timer");
            handle_timer_stop = 1;
            return RAINBOW;
        }
    }
    else
    {
        printf("rainbow lights timer is null \n");
    }

    return 255;
}

void Get_current_animation_speed()
{
    if (fading_lights_timer != NULL)
    {
        if (esp_timer_is_active(fading_lights_timer) == 1)
        {
            ESP_LOGW("RGB", "fading lights is active");
            uint64_t period;
            ESP_ERROR_CHECK(esp_timer_get_period(fading_lights_timer, &period));
            printf("Period = %llu \n", period);
        }
    }
    else
    {
        printf("fading lights timer is null \n");
    }

    if (running_lights_timer != NULL)
    {
        if (esp_timer_is_active(running_lights_timer) == 1)
        {
            ESP_LOGW("RGB", " running lights timer is active");
            uint64_t period;
            ESP_ERROR_CHECK(esp_timer_get_period(running_lights_timer, &period));
            printf("Period = %llu \n", period);
        }
    }
    else
    {
        printf("running lights timer is null \n");
    }

    if (rainbow_lights_timer != NULL)
    {
        if (esp_timer_is_active(rainbow_lights_timer) == 1)
        {
            ESP_LOGW("RGB", "rainbow lights timer is active");
            uint64_t period;
            ESP_ERROR_CHECK(esp_timer_get_period(rainbow_lights_timer, &period));
            printf("Period = %llu \n", period);
        }
    }
    else
    {
        printf("rainbow lights timer is null \n");
    }
}
