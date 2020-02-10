/* ADC2 Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <esp_adc_cal.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "network_config.h"
#include "config.h"
#include "touch_pad.h"
#include "ble_gatt_server.h"

void app_main(void)
{
    //optical_sensor_init();
    //config_unit_test();
    //network_config_unit_test();

    //touch_pad_unit_test();
    ble_gatt_server_unit_test();
    //while(1) {
        //opitcal_unit_test();
    //}
}
