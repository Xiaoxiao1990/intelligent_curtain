//
// Created by root on 3/22/20.
//

#include <esp_log.h>
#include <string.h>
#include "protocol_parser.h"
#include "config.h"
#include "motor_controller.h"

#define PARSER_TAG      "PARSER"

int protocol_parser(protocol_data_block_t *data)
{
    uint8_t *rx, *tx;
    uint16_t rx_len;
    
    rx = data->rx_data;
    tx = data->tx_data;
    rx_len = data->rx_len;

    if (rx[0] != 0x55) {
        ESP_LOGE(PARSER_TAG, "Illegal Header");
        tx[0] = 0x55;
        tx[1] = 0;
        tx[2] = 0xFF;
        data->tx_len = 3;
    } else {
        switch (rx[1]) {
            case 0x00:  // system command
                if (rx[2] == 0x01) {
                    ESP_LOGI(PARSER_TAG, "Start adjust curtain position");
                    motor.state = MOTOR_STATE_ADJUST;
                    // TODO done
                    memcpy(tx, rx, rx_len);
                    data->tx_len = rx_len;
                } else if (rx[2] == 0x02) {
                    ESP_LOGI(PARSER_TAG, "Reboot device");
                    memcpy(tx, rx, rx_len);
                    data->tx_len = rx_len;
                    // TODO actions
                } else if (rx[2] == 0x03) {
                    ESP_LOGI(PARSER_TAG, "Set Server IP & Port");
                    snprintf(Curtain.device_params.server_address.ip, 16, "%d.%d.%d.%d", rx[3], rx[4], rx[5], rx[6]);
                    Curtain.device_params.server_address.port = rx[7];
                    Curtain.device_params.server_address.port <<= 8;
                    Curtain.device_params.server_address.port += rx[8];
                    memcpy(tx, rx, rx_len);
                    data->tx_len = rx_len;
                    // TODO: save to nvs
                    params_save();
                } else if (rx[2] == 0x04) {
                    ESP_LOGI(PARSER_TAG, "Read Server IP & Port");
                    int d1, d2, d3, d4;
                    sscanf(Curtain.device_params.server_address.ip, "%d.%d.%d.%d", &d1, &d2, &d3, &d4);
                    tx[0] = 0x55;
                    tx[1] = 0x00;
                    tx[2] = 0x04;
                    tx[3] = (uint8_t) d1;
                    tx[4] = (uint8_t) d2;
                    tx[5] = (uint8_t) d3;
                    tx[6] = (uint8_t) d4;
                    tx[7] = (uint8_t) (Curtain.device_params.server_address.port >> 8);
                    tx[8] = (uint8_t) (Curtain.device_params.server_address.port & 0xFF);

                    data->tx_len = 9;
                } else {
                    ESP_LOGE(PARSER_TAG, "Undefined operation");
                    tx[0] = 0x55;
                    tx[1] = 0;
                    tx[2] = 0xFE;
                    data->tx_len = 3;
                }
                break;
            case 0x01:  // read
                if (rx[2] == 0x01) {
                    ESP_LOGI(PARSER_TAG, "Read device info");
                    // 55 01 01
                    memcpy(tx, rx, rx_len);
                    // id 6bytes
                    memcpy(&(tx[3]), &Curtain.device_id, 6);
                    // battery
                    tx[10] = Curtain.device_params.battery;
                    // battery temp
                    tx[11] = Curtain.device_params.bat_temp;
                    // battery status, 0:not charge, 1: charging, 2:charged
                    tx[12] = Curtain.device_params.bat_state;
                    // curtain position
                    tx[13] = Curtain.device_params.curtain_position;
                    // optical status
                    tx[14] = Curtain.device_params.optical_sensor_status;
                    // lumen
                    tx[15] = Curtain.device_params.lumen;
                    // work mode, 0:no mode, 1: summer, 2:winter
                    tx[16] = Curtain.device_params.work_mode;
                    // light gate value
                    tx[17] = Curtain.device_params.lumen_gate_value;
                    data->tx_len = 18;
                } else if (rx[2] == 0x03) {
                    ESP_LOGI(PARSER_TAG, "Read switcher time");
                    if (rx[3] > 3) {
                        tx[0] = 0x55;
                        tx[1] = 0;
                        tx[2] = 0xFC;
                        data->tx_len = 3;
                    } else {
                        memcpy(tx, rx, 4);
                        memcpy(&tx[4], &Curtain.device_params.curtain_timer[rx[3]], 5);
                        data->tx_len = 9;
                    }
                } else {
                    ESP_LOGE(PARSER_TAG, "Undefined operation");
                    tx[0] = 0x55;
                    tx[1] = 0;
                    tx[2] = 0xFE;
                    data->tx_len = 3;
                }
                break;
            case 0x02: // set
                if (rx[2] == 0x01) {
                    ESP_LOGI(PARSER_TAG, "Set time");
                    // TODO:Set system time

                    // 55 02 01
                    memcpy(tx, rx, 3);
                    tx[3] = 1;
                    data->tx_len = 4;
                } else if (rx[2] == 0x02) {
                    ESP_LOGI(PARSER_TAG, "Set light work mode");
                    Curtain.device_params.work_mode = rx[3];
                    Curtain.device_params.lumen_gate_value = rx[4];
                    memcpy(tx, rx, 3);
                    tx[3] = Curtain.device_params.optical_sensor_status;
                    data->tx_len = 4;
                } else if (rx[2] == 0x03) {
                    ESP_LOGI(PARSER_TAG, "Set switcher time");
                    if (rx[3] > 3 || rx_len != 9) {
                        tx[0] = 0x55;
                        tx[1] = 0;
                        tx[2] = 0xFC;
                        data->tx_len = 3;
                    } else {
                        memcpy(&Curtain.device_params.curtain_timer[rx[3]], &rx[4], 5);
                        memcpy(tx, rx, rx_len);
                        data->tx_len = rx_len;
                    }
                } else if (rx[2] == 0x04) {
                    ESP_LOGI(PARSER_TAG, "Set curtain ratio");
                    if (rx[3] > 100) {
                        motor_target_position(Curtain.device_params.curtain_width, 1);
                    } else {
                        motor_target_position(Curtain.device_params.curtain_width * rx[3] / 100, 1);
                    }
                    motor.state = MOTOR_POSITION_ADJUST;
                    // TODO: done
                    memcpy(tx, rx, rx_len);
                    data->tx_len = rx_len;
                } else if (rx[2] == 0x05) {
                    ESP_LOGI(PARSER_TAG, "Motor control");
                    if (rx[3]){
                        motor.state = MOTOR_STATE_FORWARD;
                        motor.speed = 70.0;
                        motor.direction = MOTOR_RUN_FORWARD;
                        motor.power_enable = MOTOR_ENABLE;

                        motor_run(motor.direction, motor.speed);
                    } else {
                        motor.state = MOTOR_STATE_BACKWARD;
                        motor.speed = 70.0;
                        motor.direction = MOTOR_RUN_BACKWORK;
                        motor.power_enable = MOTOR_ENABLE;
                        motor_run(motor.direction, motor.speed);
                    }
                    memcpy(tx, rx, rx_len);
                    data->tx_len = rx_len;
                } else {
                    ESP_LOGE(PARSER_TAG, "Undefined operation");
                    tx[0] = 0x55;
                    tx[1] = 0;
                    tx[2] = 0xFE;
                    data->tx_len = 3;
                }
                break;
            default: {
                tx[0] = 0x55;
                tx[1] = 0;
                tx[2] = 0xFD;
                data->tx_len = 3;
            }
                break;
        }
    }

    return 0;
}
