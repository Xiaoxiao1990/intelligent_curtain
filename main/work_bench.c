//
// Created by root on 3/30/20.
//

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "work_bench.h"
#include "config.h"
#include "network_config.h"
#include "ble_spp_server.h"
#include "motor_controller.h"
#include "adc_driver.h"
#include "touch_pad.h"

#define   TAG       "WORK_BENCH"

void work_bench(void)
{
    config_init();
    touch_pad_initialize();
    motor_init();
    adc_unit_init();

    //wifi_service_start();
    ble_server_start();

    int a = 0;
    while (1) {
        if (a++ > 30) {
            a = 0;
            ESP_LOGI(TAG, "motor state, speed, enable: %d, %f, %d", motor.state, motor.speed, motor.power_enable);
        }
        motor_controller_task();
        get_adcs_values();
        vTaskDelay(50 / portTICK_RATE_MS);
    }
}