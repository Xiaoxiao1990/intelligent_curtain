//
// Created by root on 1/4/20.
//

#include "sensor.h"
#include "esp_system.h"
#include "config.h"

#define OPTICAL_SENSOR_ENABLE_GPIO              CONFIG_OPTICAL_SENSOR_ENABLE_GPIO

typedef struct {
    uint8_t state;
    uint8_t lumen;
} optical_t;

void optical_enable(enable_t enable)
{
    gpio_pad_select_gpio(OPTICAL_SENSOR_ENABLE_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(OPTICAL_SENSOR_ENABLE_GPIO, GPIO_MODE_OUTPUT);
    // enable optical sensor
    gpio_set_level(OPTICAL_SENSOR_ENABLE_GPIO, enable);
}

