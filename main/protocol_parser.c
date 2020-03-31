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
                    snprintf(Curtain.server_address.ip, 16, "%d.%d.%d.%d", rx[3], rx[4], rx[5], rx[6]);
                    Curtain.server_address.port = rx[7];
                    Curtain.server_address.port <<= 8;
                    Curtain.server_address.port += rx[8];
                    memcpy(tx, rx, rx_len);
                    data->tx_len = rx_len;
                    // TODO: save to nvs
                    params_save();
                } else if (rx[2] == 0x04) {
                    ESP_LOGI(PARSER_TAG, "Read Server IP & Port");
                    int d1, d2, d3, d4;
                    sscanf(Curtain.server_address.ip, "%d.%d.%d.%d", &d1, &d2, &d3, &d4);
                    tx[0] = 0x55;
                    tx[1] = 0x00;
                    tx[2] = 0x04;
                    tx[3] = (uint8_t) d1;
                    tx[4] = (uint8_t) d2;
                    tx[5] = (uint8_t) d3;
                    tx[6] = (uint8_t) d4;
                    tx[7] = (uint8_t) (Curtain.server_address.port >> 8);
                    tx[8] = (uint8_t) (Curtain.server_address.port & 0xFF);

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
                    tx[10] = Curtain.battery;
                    // battery temp
                    tx[11] = Curtain.bat_temp;
                    // battery status, 0:not charge, 1: charging, 2:charged
                    tx[12] = Curtain.bat_state;
                    // curtain position
                    tx[13] = Curtain.curtain_position;
                    // optical status
                    tx[14] = Curtain.optical_sensor_status;
                    // lumen
                    tx[15] = Curtain.lumen;
                    // work mode, 0:no mode, 1: summer, 2:winter
                    tx[16] = Curtain.work_mode;
                    // light gate value
                    tx[17] = Curtain.lumen_gate_value;
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
                        memcpy(&tx[4], &Curtain.curtain_timer[rx[3]], 5);
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
                    Curtain.work_mode = rx[3];
                    Curtain.lumen_gate_value = rx[4];
                    memcpy(tx, rx, 3);
                    tx[3] = Curtain.optical_sensor_status;
                    data->tx_len = 4;
                } else if (rx[2] == 0x03) {
                    ESP_LOGI(PARSER_TAG, "Set switcher time");
                    if (rx[3] > 3 || rx_len != 9) {
                        tx[0] = 0x55;
                        tx[1] = 0;
                        tx[2] = 0xFC;
                        data->tx_len = 3;
                    } else {
                        memcpy(&Curtain.curtain_timer[rx[3]], &rx[4], 5);
                        memcpy(tx, rx, rx_len);
                        data->tx_len = rx_len;
                    }
                } else if (rx[2] == 0x04) {
                    ESP_LOGI(PARSER_TAG, "Set curtain ratio: %d%%", rx[3]);
                    if (rx[3] > 100) {
                        // motor_target_position(Curtain.curtain_width, 1);
                        Curtain.target_position = Curtain.curtain_width;
                        Curtain.state = CURTAIN_SET_POSITION;
                    } else {
                        // motor_target_position(Curtain.curtain_width * rx[3] / 100, 1);
                        Curtain.target_position = Curtain.curtain_width * rx[3] / 100;
                        Curtain.state = CURTAIN_SET_POSITION;
                    }

                    // TODO: done
                    memcpy(tx, rx, rx_len);
                    data->tx_len = rx_len;
                } else if (rx[2] == 0x05) {
                    ESP_LOGI(PARSER_TAG, "Motor control");
                    if (rx[3]){
                        motor_stop();
                        Curtain.state = CURTAIN_IDLE;
                    } else {
                        if (rx[4])
                            motor_backward();
                        else
                            motor_forward();
                        Curtain.state = CURTAIN_MANUAL_ADJUST;
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
