
void Initialize_animation_timers(){
        const esp_timer_create_args_t fading_lights = {
        .callback = &RGB_fade_in_out_callback,
        /* name is optional, but may help identify the timer when debugging */
        .arg = &rgb_params,
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