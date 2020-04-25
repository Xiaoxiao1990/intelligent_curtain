//
// Created by root on 3/30/20.
//

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <time.h>
#include <lwipopts.h>
#include "work_bench.h"
#include "config.h"
#include "network_config.h"
#include "ble_spp_server.h"
#include "motor_controller.h"
#include "adc_driver.h"
#include "touch_pad.h"
#include "network_time_sntp.h"

#define TAG       "WORK_BENCH"

#define CHARGE_STATE_DET_PIN        GPIO_NUM_21

void curtain_idle(void);
void curtain_manual_adjust(void);
void curtain_width_adjust(void);
void curtain_set_position();

static uint8_t curtain_set_position_state = 0, curtain_width_adjust_state = 0;

void entrace_curtain_set_position(uint32_t target)
{
    curtain_set_position_state = 0;
    Curtain.state = CURTAIN_SET_POSITION;
    Curtain.target_position = target;
}

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
            params_save();
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
    time_t now = 0;
    struct tm time_info = { 0 };

    time(&now);
    localtime_r(&now, &time_info);
    ESP_LOGI(TAG, "time now: %d:%d:%d", time_info.tm_hour, time_info.tm_min, time_info.tm_sec);

    if (Curtain.optical_sensor_status) {

        if (g_adcs_vals.optical_val > Curtain.lumen_gate_value) {
            ESP_LOGE(TAG, "optical gate value too big");
            motor_forward();
        }

        if (g_adcs_vals.optical_val < Curtain.lumen_gate_value - 200) {
            ESP_LOGE(TAG, "optical gate value too small");
            motor_backward();
        }
    } else {
        for (int i = 0; i < 4; ++i) {
            if (Curtain.curtain_timer[i].hour == time_info.tm_hour && Curtain.curtain_timer[i].min == time_info.tm_min) {
                // create a task
                if (Curtain.curtain_timer[i].repeater & (uint8_t)((uint8_t)0x01 << (uint8_t)time_info.tm_wday)) {
                    if (Curtain.curtain_timer[i].repeater & (uint8_t)0x80) {
                        Curtain.curtain_timer[i].repeater = 0;
                    }

                    if (Curtain.curtain_timer[i].action) {
                        ESP_LOGE(TAG, "timer %d start backward.", time_info.tm_wday);
                        motor_backward();
                    } else {
                        ESP_LOGE(TAG, "timer %d start forward", time_info.tm_wday);
                        motor_forward();
                    }
                }
            }
        }
    }
}

void charge_state_detect_init(void)
{
    //第一种方式配置
    gpio_pad_select_gpio(CHARGE_STATE_DET_PIN);
    gpio_set_direction(CHARGE_STATE_DET_PIN, GPIO_MODE_INPUT);
}

int charge_state()
{
    return gpio_get_level(CHARGE_STATE_DET_PIN);
}

void timer_task(void *arg)
{
    int daily = 0;
    int minutely = 0;

    while (1) {
        if (Curtain.state == CURTAIN_IDLE) {

            if (daily++ > 3600 * 24) {
                daily = 0;
                if (network_state == NETWORK_CONNECTED) {
                    esp_wait_sntp_sync();
                }
            }

            if (minutely++ > 60) {
                minutely = 0;

                optical_timer_check_per_minute();

            }

            Curtain.bat_state = charge_state(); // 1 charged, 0 charging
        }
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

void work_bench(void)
{
    config_init();
    touch_pad_initialize();
    motor_init();
    adc_unit_init();

    wifi_service_start();
    ble_server_start();
    charge_state_detect_init();

    Curtain.state = CURTAIN_IDLE;
    memset(&g_adcs_vals, 0, sizeof(adc_value_t));
    xTaskCreate(&timer_task, "timer_task", 4096, NULL, 6, NULL);
    int a = 0;

    while (a++ < 100) {
        get_adcs_values();

        //if (g_adcs_vals.motor_fwd_vtg < 100 && g_adcs_vals.motor_bwd_vtg < 100)
            //break;

        vTaskDelay(pdMS_TO_TICKS(50));
    }

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