

#include "RGB.h"

static const char *TAG = "example";

static QueueHandle_t gpio_evt_queue = NULL;

const uint8_t lights[360] =
    {
        0, 0, 0, 0, 0, 1, 1, 2,
        2, 3, 4, 5, 6, 7, 8, 9,
        11, 12, 13, 15, 17, 18, 20, 22,
        24, 26, 28, 30, 32, 35, 37, 39,
        42, 44, 47, 49, 52, 55, 58, 60,
        63, 66, 69, 72, 75, 78, 81, 85,
        88, 91, 94, 97, 101, 104, 107, 111,
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
        114, 111, 107, 104, 101, 97, 94, 91,
        88, 85, 81, 78, 75, 72, 69, 66,
        63, 60, 58, 55, 52, 49, 47, 44,
        42, 39, 37, 35, 32, 30, 28, 26,
        24, 22, 20, 18, 17, 15, 13, 12,
        11, 9, 8, 7, 6, 5, 4, 3,
        2, 2, 1, 1, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0};

led_strip_handle_t led_strip;
struct rgb_color_s strip_color; // global color struct

static void IRAM_ATTR button_handler(void *arg)
{
    BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
    uint32_t gpio_num = (uint32_t)arg;

    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
    esp_rom_printf("IR sensor handler toggled \n");
    if (pxHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

void RGB_setup()
{
    gpio_config_t io_conf;
    // disable interrupt
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    // set as output mode
    io_conf.mode = GPIO_MODE_INPUT;
    // bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_BUTTON_INPUT_SEL;
    // disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    // disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    // configure GPIO with the given settings
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(2, sizeof(uint32_t));
    // start gpio task
    xTaskCreate(Button_detection_task, "Button_detection_task", 2048, NULL, 10, NULL);

    gpio_install_isr_service(0);
    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(BUTTON_1, button_handler, (void *)BUTTON_1);

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
    ESP_LOGI(TAG, "Start blinking LED strip");
    ESP_ERROR_CHECK(led_strip_clear(led_strip));

    strip_color.red = 0;
    strip_color.blue = 0;
    strip_color.green = 0;
    strip_color.brightness = 1;
}

void RGB_change_brightness(uint8_t brigthness)
{
    strip_color.red = brigthness;
    strip_color.green = brigthness;
    strip_color.blue = brigthness;
}

void RGB_set_red(uint8_t color)
{
    strip_color.red = color;
}

void RGB_set_green(uint8_t color)
{
    strip_color.green = color;
}

void RGB_set_blue(uint8_t color)
{
    strip_color.blue = color;
}

void RGB_set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    strip_color.red = red;
    strip_color.green = green;
    strip_color.blue = blue;
}

void RGB_clear_strip()
{
    ESP_ERROR_CHECK(led_strip_clear(led_strip));
}

void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = (uint32_t)(v * 2.55f);
    uint32_t rgb_min = (uint32_t)(rgb_max * (100 - s) / 100.0f);

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i)
    {
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
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, LED, lights[(angle + 120) % 360] * strip_color.brightness, lights[(angle + 0) % 360] * strip_color.brightness, lights[(angle + 240) % 360] * strip_color.brightness));
}

void RGB_meet_in_the_middle(void *argument)
{
    uint8_t led_num = 0;
    for (;;)
    {
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_num, 150, 150, 150));                               // violet
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, (LED_STRIP_LED_NUMBERS - 1) - led_num, 150, 150, 150)); // violet
        led_num++;
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        vTaskDelay(50 / portTICK_PERIOD_MS);

        // if value does not divide by 2, turn on the last led
        if (led_num >= (LED_STRIP_LED_NUMBERS / 2))
        {
            if (LED_STRIP_LED_NUMBERS % 2 != 0)
            {
                ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, led_num, 150, 150, 150)); // violet
                ESP_ERROR_CHECK(led_strip_refresh(led_strip));
            }
            vTaskDelete(0);
        }
    }
}

void RGB_turn_index_led(uint8_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, index, red, green, blue)); // violet
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}

void Button_detection_task(void *arg)
{

    uint32_t io_num;
    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            printf("button click detected \n");
        }
    }
}