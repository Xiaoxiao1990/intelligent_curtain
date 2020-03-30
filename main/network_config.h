//
// Created by root on 1/4/20.
//

#ifndef INTELLIGENT_CURTAIN_NETWORK_CONFIG_H
#define INTELLIGENT_CURTAIN_NETWORK_CONFIG_H

typedef enum {
    NETWORK_DO_NOT_CONFIG = 0,
    NETWORK_CONNECTTING,
    NETWORK_CONNECTED,
    NETWORK_FIRST_CONFIG,
    NETWORK_ERROR
} Network_State_Typedef;

extern Network_State_Typedef network_state;

void network_config_unit_test(void);
void wifi_service_start(void);

#endif //INTELLIGENT_CURTAIN_NETWORK_CONFIG_H
