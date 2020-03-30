//
// Created by root on 3/24/20.
//

#ifndef INTELLIGENT_CURTAIN_ADC_DRIVER_H
#define INTELLIGENT_CURTAIN_ADC_DRIVER_H

#include <stdint.h>

typedef struct {
    uint32_t motor_fwd_val, motor_bwd_val, motor_cur_val, optical_val, bat_chr_val, bat_tmp_val;
    uint32_t motor_fwd_vtg, motor_bwd_vtg, motor_cur_vtg, optical_vtg, bat_chr_vtg, bat_tmp_vtg;
    int battery_temp;
    uint32_t battery_voltage;
    uint8_t optical_ratio;
} adc_value_t;

extern adc_value_t g_adcs_vals;

void adc_unit_init(void);
void get_adcs_values(void);
void adc_driver_unit_test(void);

#endif //INTELLIGENT_CURTAIN_ADC_DRIVER_H
