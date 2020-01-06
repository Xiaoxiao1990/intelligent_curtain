//
// Created by root on 1/4/20.
//

#include "freertos/FreeRTOS.h"
#include <driver/adc.h>
#include <freertos/task.h>
#include <lwipopts.h>
#include <esp_adc_cal.h>
#include "optical_sensor.h"

#include "esp_system.h"

#define OPTICAL_SENSOR_ADC2_CHANNEL             CONFIG_OPTICAL_SENSOR_ADC2_CHANNEL
#define OPTICAL_SENSOR_ENABLE_GPIO              CONFIG_OPTICAL_SENSOR_ENABLE_GPIO

#define DEFAULT_VREF    1100

void optical_sensor_init(void)
{
    gpio_num_t adc_gpio_num;
    esp_err_t r;
    //Characterize ADC at particular atten

//    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
//    //Check type of calibration value used to characterize ADC
//    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
//        printf("eFuse Vref");
//    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
//        printf("Two Point");
//    } else {
//        printf("Default");
//    }

    gpio_pad_select_gpio(OPTICAL_SENSOR_ENABLE_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(OPTICAL_SENSOR_ENABLE_GPIO, GPIO_MODE_OUTPUT);
    // enable optical sensor
    gpio_set_level(OPTICAL_SENSOR_ENABLE_GPIO, 1);

    r = adc2_pad_get_io_num(OPTICAL_SENSOR_ADC2_CHANNEL, &adc_gpio_num );
    assert( r == ESP_OK );
    adc2_config_channel_atten(OPTICAL_SENSOR_ADC2_CHANNEL, ADC_ATTEN_11db);

    vTaskDelay(2 * portTICK_PERIOD_MS);
}

esp_err_t get_optical_adc_value(int *read_raw)
{
    return adc2_get_raw(OPTICAL_SENSOR_ADC2_CHANNEL, ADC_WIDTH_12Bit, read_raw);
}

uint32_t get_optical_voltage(int adc_value)
{
    esp_adc_cal_characteristics_t *adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    //esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_2, ADC_ATTEN_11db, ADC_WIDTH_12Bit, DEFAULT_VREF, adc_chars);
    esp_adc_cal_characterize(ADC_UNIT_2, ADC_ATTEN_11db, ADC_WIDTH_12Bit, DEFAULT_VREF, adc_chars);

    return esp_adc_cal_raw_to_voltage(adc_value, adc_chars);
}

void opitcal_unit_test(void)
{
    uint8_t output_data=0;
    int read_raw;
    esp_err_t r;

    output_data++;
    r = get_optical_adc_value(&read_raw);
    if ( r == ESP_OK ) {
        printf("[%d] adc value:%d, voltage: %dmv\n", output_data, read_raw, get_optical_voltage(read_raw));
    } else if ( r == ESP_ERR_INVALID_STATE ) {
        printf("%s: ADC2 not initialized yet.\n", esp_err_to_name(r));
    } else if ( r == ESP_ERR_TIMEOUT ) {
        //This can not happen in this example. But if WiFi is in use, such error code could be returned.
        printf("%s: ADC2 is in use by Wi-Fi.\n", esp_err_to_name(r));
    } else {
        printf("%s\n", esp_err_to_name(r));
    }

    vTaskDelay( 2 * portTICK_PERIOD_MS );
}