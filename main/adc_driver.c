//
// Created by root on 3/24/20.
//

#include "adc_driver.h"
/* ADC1 Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   16          //Multisampling

#define CHANNEL_NUMS    6

adc_value_t g_adcs_vals;

static esp_adc_cal_characteristics_t *adc_chars;

//GPIO34 if ADC1
typedef enum {
    CH_MOTOR_FORWARD, // ch0
    CH_MOTOR_BACKWARD, // ch3
    CH_OPTICAL, // ch4
    CH_BAT_TEMP, // ch5
    CH_MOTOR_BRAK, // ch6
    CH_BAT_VOLTAGE // ch7
} adc_ch_t;

static const adc1_channel_t channel[CHANNEL_NUMS] = {ADC1_CHANNEL_0, ADC1_CHANNEL_3, ADC1_CHANNEL_4, ADC1_CHANNEL_5,
                                                    ADC1_CHANNEL_6, ADC1_CHANNEL_7 };
static const adc_atten_t atten = ADC_ATTEN_DB_0;
static const adc_unit_t unit = ADC_UNIT_1;

static void check_efuse()
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

void adc_unit_init(void)
{
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();

    for (int j = 0; j < CHANNEL_NUMS; ++j) {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(channel[j], atten);
        adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
        esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
        print_char_val_type(val_type);
    }
}

void get_adcs_values(void)
{
    static uint32_t adc_reading[CHANNEL_NUMS] = { 0, }, adc_voltage[CHANNEL_NUMS] = { 0, };
    static uint8_t sample_times = 0, print_times = 0;

    for (int j = 0; j < CHANNEL_NUMS; ++j) {
        adc_reading[j] += adc1_get_raw(channel[j]);
    }

    if (++sample_times >= NO_OF_SAMPLES) {
        sample_times = 0;
        for (int i = 0; i < CHANNEL_NUMS; ++i) {
            adc_reading[i] >>= 4;
            //Convert adc_reading to voltage in mV
            adc_voltage[i] = esp_adc_cal_raw_to_voltage(adc_reading[i], adc_chars);

        }

        g_adcs_vals.motor_fwd_val = adc_reading[CH_MOTOR_FORWARD];
        g_adcs_vals.motor_bwd_val = adc_reading[CH_MOTOR_BACKWARD];
        g_adcs_vals.motor_cur_val = adc_reading[CH_MOTOR_BRAK];
        g_adcs_vals.optical_val = adc_reading[CH_OPTICAL];
        g_adcs_vals.bat_chr_val = adc_reading[CH_BAT_VOLTAGE];
        g_adcs_vals.bat_tmp_val = adc_reading[CH_BAT_TEMP];

        g_adcs_vals.motor_fwd_vtg = adc_voltage[CH_MOTOR_FORWARD];
        g_adcs_vals.motor_bwd_vtg = adc_voltage[CH_MOTOR_BACKWARD];
        g_adcs_vals.motor_cur_vtg = adc_voltage[CH_MOTOR_BRAK];
        g_adcs_vals.optical_vtg = adc_voltage[CH_OPTICAL];
        g_adcs_vals.bat_chr_vtg = adc_voltage[CH_BAT_VOLTAGE];
        g_adcs_vals.bat_tmp_vtg = adc_voltage[CH_BAT_TEMP];

        g_adcs_vals.battery_temp = (adc_voltage[CH_BAT_TEMP] - 500) / 10;
        g_adcs_vals.battery_voltage = (adc_voltage[CH_BAT_VOLTAGE] >> 1) * 3;
//
//        if (print_times++ > 2) {
//            print_times = 0;
//            printf("ADCs: %d, %d, %d, %d, %d, %d\n", adc_reading[0], adc_reading[1], adc_reading[2], adc_reading[3],
//                   adc_reading[4], adc_reading[5]);
//            printf("Voltages %dmV, %dmV, %dmV, %dmV, %dmV, %dmV\n", adc_voltage[0], adc_voltage[1], adc_voltage[2],
//                   adc_voltage[3], adc_voltage[4], adc_voltage[5]);
//        }
    }

}

void adc_driver_unit_test(void)
{
    static uint32_t adc_reading[CHANNEL_NUMS] = { 0, }, adc_voltage[CHANNEL_NUMS] = { 0, };

    adc_unit_init();

    //Continuously sample ADC1

    uint8_t sample_times = 0;
    uint8_t print_times = 0;
    while (1) {
        for (int j = 0; j < CHANNEL_NUMS; ++j) {
            adc_reading[j] += adc1_get_raw(channel[j]);
        }

        if (++sample_times >= NO_OF_SAMPLES) {
            sample_times = 0;
            for (int i = 0; i < CHANNEL_NUMS; ++i) {
                adc_reading[i] >>= 4;
                //Convert adc_reading to voltage in mV
                adc_voltage[i] = esp_adc_cal_raw_to_voltage(adc_reading[i], adc_chars);

            }

            g_adcs_vals.motor_fwd_val = adc_reading[CH_MOTOR_FORWARD];
            g_adcs_vals.motor_bwd_val = adc_reading[CH_MOTOR_BACKWARD];
            g_adcs_vals.motor_cur_val = adc_reading[CH_MOTOR_BRAK];
            g_adcs_vals.optical_val = adc_reading[CH_OPTICAL];
            g_adcs_vals.bat_chr_val = adc_reading[CH_BAT_VOLTAGE];
            g_adcs_vals.bat_tmp_val = adc_reading[CH_BAT_TEMP];

            g_adcs_vals.motor_fwd_vtg = adc_voltage[CH_MOTOR_FORWARD];
            g_adcs_vals.motor_bwd_vtg = adc_voltage[CH_MOTOR_BACKWARD];
            g_adcs_vals.motor_cur_vtg = adc_voltage[CH_MOTOR_BRAK];
            g_adcs_vals.optical_vtg = adc_voltage[CH_OPTICAL];
            g_adcs_vals.bat_chr_vtg = adc_voltage[CH_BAT_VOLTAGE];
            g_adcs_vals.bat_tmp_vtg = adc_voltage[CH_BAT_TEMP];

            g_adcs_vals.battery_temp = (adc_voltage[CH_BAT_TEMP] - 500) / 10;
            g_adcs_vals.battery_voltage = (adc_voltage[CH_BAT_VOLTAGE] >> 1) * 3;

            if (print_times++ > 2) {
                print_times = 0;
                printf("ADCs: %d, %d, %d, %d, %d, %d\n", adc_reading[0], adc_reading[1], adc_reading[2], adc_reading[3],
                       adc_reading[4], adc_reading[5]);
                printf("Voltages %dmV, %dmV, %dmV, %dmV, %dmV, %dmV\n", adc_voltage[0], adc_voltage[1], adc_voltage[2],
                       adc_voltage[3], adc_voltage[4], adc_voltage[5]);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

