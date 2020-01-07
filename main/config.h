//
// Created by root on 1/5/20.
//

#ifndef INTELLIGENT_CURTAIN_CONFIG_H
#define INTELLIGENT_CURTAIN_CONFIG_H

#include <esp_wifi_types.h>

typedef struct {
    char ip[17];
    unsigned short port;
} IPv4_Address_TypeDef;

#define WEEKLY_MASK_SUNDAY          0x01
#define WEEKLY_MASK_MONDAY          0x02
#define WEEKLY_MASK_TUESDAY         0x04
#define WEEKLY_MASK_WEDNSDAY        0x08
#define WEEKLY_MASK_THUSDAY         0x10
#define WEEKLY_MASK_FRIDAY          0x20
#define WEEKLY_MASK_SATURDAY        0x40

typedef struct {
    uint8_t s_hour;
    uint8_t s_min;
    uint8_t s_second;
    uint8_t e_hour;
    uint8_t e_min;
    uint8_t e_second;
} WorkTime_TypeDef;

typedef struct {
    uint8_t battery;
    uint8_t optical_sensor_status;
    uint8_t lumen;
    uint8_t curtain_position;
    WorkTime_TypeDef optical_work_time;
    WorkTime_TypeDef curtain_work_time;
    uint8_t curtain_repeater;
    IPv4_Address_TypeDef server_address;
} Device_Params_TypeDef;

typedef struct Config_TAG {
    char device_id[10];
    Device_Params_TypeDef device_params;
    bool is_wifi_config;
    wifi_config_t wifi_config;
} Curtain_TypeDef;

extern Curtain_TypeDef Curtain;

esp_err_t params_init(void);
esp_err_t params_save(void);
void config_unit_test(void);

#endif //INTELLIGENT_CURTAIN_CONFIG_H
