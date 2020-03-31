//
// Created by root on 3/30/20.
//

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "work_bench.h"
#include "config.h"
#include "network_config.h"
#include "ble_spp_server.h"
#include "motor_controller.h"
#include "adc_driver.h"
#include "touch_pad.h"

#define TAG       "WORK_BENCH"

void curtain_idle(void);
void curtain_manual_adjust(void);
void curtain_width_adjust(void);
void curtain_set_position();

static uint8_t curtain_set_position_state = 0, curtain_width_adjust_state = 0;

void curtain_set_position()
{
    switch (curtain_set_position_state) {
        case 0:
            if (Curtain.target_position > Curtain.curtain_position) {
                ESP_LOGI(TAG, "curtain move forward.");
                motor_forward();
                curtain_set_position_state++;
            }

            if (Curtain.target_position < Curtain.curtain_position) {
                ESP_LOGI(TAG, "curtain move backward.");
                motor_backward();
                curtain_set_position_state += 2;
            }
            break;
        case 1:
            if (Curtain.target_position <= Curtain.curtain_position) {
                ESP_LOGI(TAG, "curtain move forward arrived target position.");
                motor_stop();
                curtain_set_position_state = 0;
                Curtain.state = CURTAIN_IDLE;
            }
            break;
        case 2:
            if (Curtain.target_position >= Curtain.curtain_position) {
                ESP_LOGI(TAG, "curtain move backward arrived target position.");
                motor_stop();
                curtain_set_position_state = 0;
                Curtain.state = CURTAIN_IDLE;
            }
            break;
        default:
            motor.state = MOTOR_STATE_PRE_STOP;
            Curtain.state = CURTAIN_IDLE;
            curtain_set_position_state = 0;
            break;
    }

    if (motor.state == MOTOR_STATE_PRE_STOP) {
        Curtain.state = CURTAIN_IDLE;
        curtain_set_position_state = 0;
    }
}

void curtain_manual_adjust(void)
{
    if (motor.state == MOTOR_STATE_STOP) {
        Curtain.state = CURTAIN_IDLE;
    }
}

void curtain_width_adjust(void)
{
    static uint32_t width = 0;

    switch (curtain_width_adjust_state) {
        case 0:
            ESP_LOGI(TAG, "start to back to origin.");
            motor_backward();
            curtain_width_adjust_state++;
            break;
        case 1:
            if (motor.state == MOTOR_STATE_STOP) {
                ESP_LOGI(TAG, "motor arrived origin, start counter width.");
                motor_forward();
                curtain_width_adjust_state++;
                width = 0;
            }
            break;
        case 2:
            if (width < 0xffffffff)
                width++;

            if (motor.state == MOTOR_STATE_STOP) {
                ESP_LOGI(TAG, "motor arrived the end side.");
                motor_stop();
                curtain_width_adjust_state++;
            }
            break;
        case 3:
            Curtain.curtain_width = width;
            Curtain.curtain_position = width;
            Curtain.curtain_ratio = 100;
            // TODO save
            //params_save();
            Curtain.state = CURTAIN_IDLE;
            curtain_width_adjust_state = 0;
            break;
        default:
            motor.state = MOTOR_STATE_PRE_STOP;
            Curtain.state = CURTAIN_IDLE;
            curtain_width_adjust_state = 0;
            break;
    }
}

void curtain_idle(void)
{
    adc_value_t *adc_vals = &g_adcs_vals;

    switch (motor.state) {
        case MOTOR_STATE_STOP:
            if (adc_vals->motor_fwd_vtg > 200) {
                ESP_LOGI(TAG, "motor start run forward by manual.");
                motor_forward();
            }

            if (adc_vals->motor_bwd_vtg > 200) {
                ESP_LOGI(TAG, "motor start run backward by manual.");
                motor_backward();
            }
            break;
        case MOTOR_STATE_PRE_STOP:
            break;
        case MOTOR_STATE_FORWARD:
            break;
        case MOTOR_STATE_BACKWARD:
            break;
        default:
            motor.state = MOTOR_STATE_PRE_STOP;
            Curtain.state = CURTAIN_IDLE;
            break;
    }
}

void curtain_state_machine(void)
{
    switch (Curtain.state) {
        case CURTAIN_IDLE:
            curtain_idle();
            break;
        case CURTAIN_WIDTH_ADJUST:
            curtain_width_adjust();
            break;
        case CURTAIN_MANUAL_ADJUST:
            curtain_manual_adjust();
            break;
        case CURTAIN_SET_POSITION:
            curtain_set_position();
            break;
        default:
            Curtain.state = CURTAIN_IDLE;
            motor_stop();
            break;
    }

    if (motor.state == MOTOR_STATE_FORWARD) {
        if (Curtain.curtain_position < Curtain.curtain_width) {
            Curtain.curtain_position++;
            Curtain.curtain_ratio = (Curtain.curtain_position * 100) / Curtain.curtain_width;
        }
    }

    if (motor.state == MOTOR_STATE_BACKWARD) {
        if (Curtain.curtain_position) {
            Curtain.curtain_position--;
            Curtain.curtain_ratio = (Curtain.curtain_position * 100) / Curtain.curtain_width;
        }
    }
}

void optical_timer_check_per_minute(void)
{
    uint8_t weekday = 0, hour = 0, min = 0;

    if (Curtain.optical_sensor_status) {
        if (g_adcs_vals.optical_val > Curtain.lumen_gate_value) {

        }
    } else {
        for (int i = 0; i < 4; ++i) {
            if (Curtain.curtain_timer[i].hour == hour && Curtain.curtain_timer[i].min == min) {
                // create a task
                if (Curtain.curtain_timer[i].repeater & (uint8_t)0x80) {
                    Curtain.curtain_timer[i].repeater = 0;
                    if (Curtain.curtain_timer[i].action)
                        motor_backward();
                    else
                        motor_forward();
                } else if (Curtain.curtain_timer[i].repeater & weekday) {
                    if (Curtain.curtain_timer[i].action)
                        motor_backward();
                    else
                        motor_forward();
                }
            }
        }
    }
}

void work_bench(void)
{
    config_init();
    touch_pad_initialize();
    motor_init();
    adc_unit_init();

    //wifi_service_start();
    ble_server_start();

    Curtain.state = CURTAIN_WIDTH_ADJUST;

    int a = 0;
    while (1) {
        if (a++ > 30) {
            a = 0;
            ESP_LOGI(TAG, "Curtain state: %x, Motor state: %x, cur_pos: %d, width: %d, open ratio: %d%%", Curtain.state,
                     motor.state, Curtain.curtain_position, Curtain.curtain_width, Curtain.curtain_ratio);
        }
        motor_controller_state_machine();
        curtain_state_machine();
        get_adcs_values();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}