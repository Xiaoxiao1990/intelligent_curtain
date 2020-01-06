//
// Created by root on 1/4/20.
//

#ifndef INTELLIGENT_CURTAIN_ADC_H
#define INTELLIGENT_CURTAIN_ADC_H

void optical_sensor_init(void);
esp_err_t get_optical_adc_value(int *read_raw);
uint32_t get_optical_voltage(int adc_value);
void opitcal_unit_test(void);

#endif //INTELLIGENT_CURTAIN_ADC_H
