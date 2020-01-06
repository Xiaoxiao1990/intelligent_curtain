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

typedef struct Config_TAG {
    bool is_wifi_configed;
    wifi_config_t wifi_config;

    IPv4_Address_TypeDef server_address;
} Curtain_TypeDef;

extern Curtain_TypeDef Curtain;

esp_err_t params_init(void);
esp_err_t params_save(void);
void config_unit_test(void);

#endif //INTELLIGENT_CURTAIN_CONFIG_H
