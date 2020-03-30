//
// Created by root on 3/22/20.
//

#include <esp_log.h>
#include "motor_controller.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "freertos/queue.h"
#include "esp_attr.h"
#include "soc/rtc.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"
#include "adc_driver.h"
#include "config.h"

#define  TAG                "MOTOR"

#define  MOTOR_PWR_EN_PIN   GPIO_NUM_19
#define GPIO_PWM0A_OUT      GPIO_NUM_18   //Set GPIO 15 as PWM0A
#define GPIO_PWM0B_OUT      GPIO_NUM_5    //Set GPIO 16 as PWM0B

motor_t motor = {
        .state = MOTOR_STATE_STOP,
        .direction = MOTOR_RUN_FORWARD,
        .power_enable = MOTOR_DISABLE,
        .speed = 0
};

void motor_enable(motor_power_enable_t operate)
{
    if (operate)
        gpio_set_level(MOTOR_PWR_EN_PIN, 1);
    else
        gpio_set_level(MOTOR_PWR_EN_PIN, 0);
}

//static void motor_gpio_config(void) {
//    gpio_config_t io_config;
//    io_config.intr_type = GPIO_INTR_DISABLE;
//    io_config.mode = GPIO_MODE_OUTPUT;
//    io_config.pin_bit_mask = 1 << MOTOR_PWR_EN_PIN;
//    io_config.pull_down_en = GPIO_PULLDOWN_ENABLE;
//    io_config.pull_up_en = GPIO_PULLUP_ENABLE;
//    gpio_config(&io_config);
//    gpio_set_level(MOTOR_PWR_EN_PIN, 1);
//
//    io_config.intr_type = GPIO_INTR_DISABLE;
//    io_config.mode = GPIO_MODE_OUTPUT;
//    io_config.pin_bit_mask = 1 << GPIO_NUM_22;
//    io_config.pull_down_en = GPIO_PULLDOWN_ENABLE;
//    io_config.pull_up_en = GPIO_PULLUP_ENABLE;
//    gpio_config(&io_config);
//    gpio_set_level(GPIO_NUM_22, 1);
//
//    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_NUM_18);
//    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, GPIO_NUM_5);
//}

static void mcpwm_example_gpio_initialize()
{
    gpio_config_t io_config;
    io_config.intr_type = GPIO_INTR_DISABLE;
    io_config.mode = GPIO_MODE_OUTPUT;
    io_config.pin_bit_mask = 1 << MOTOR_PWR_EN_PIN;
    io_config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_config.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_config);
    motor_enable(MOTOR_DISABLE);
    ESP_LOGI(TAG, "motor enable pin disable");
    //gpio_set_level(MOTOR_PWR_EN_PIN, 1);

    printf("initializing mcpwm gpio...\n");
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_PWM0A_OUT);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, GPIO_PWM0B_OUT);
}

/**
 * @brief motor moves in forward direction, with duty cycle = duty %
 */
static void brushed_motor_forward(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num , float duty_cycle)
{
    mcpwm_set_signal_low(mcpwm_num, timer_num, MCPWM_OPR_B);
    mcpwm_set_duty(mcpwm_num, timer_num, MCPWM_OPR_A, duty_cycle);
    mcpwm_set_duty_type(mcpwm_num, timer_num, MCPWM_OPR_A, MCPWM_DUTY_MODE_0); //call this each time, if operator was previously in low/high state
}

/**
 * @brief motor moves in backward direction, with duty cycle = duty %
 */
static void brushed_motor_backward(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num , float duty_cycle)
{
    mcpwm_set_signal_low(mcpwm_num, timer_num, MCPWM_OPR_A);
    mcpwm_set_duty(mcpwm_num, timer_num, MCPWM_OPR_B, duty_cycle);
    mcpwm_set_duty_type(mcpwm_num, timer_num, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);  //call this each time, if operator was previously in low/high state
}

/**
 * @brief motor stop
 */
static void brushed_motor_stop(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num)
{
    mcpwm_set_signal_low(mcpwm_num, timer_num, MCPWM_OPR_A);
    mcpwm_set_signal_low(mcpwm_num, timer_num, MCPWM_OPR_B);
}

void motor_backward(float speed)
{
    brushed_motor_backward(MCPWM_UNIT_0, MCPWM_TIMER_0, speed);
}

void motor_forward(float speed)
{
    brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_0, speed);
}

void motor_stop(void)
{
    brushed_motor_stop(MCPWM_UNIT_0, MCPWM_TIMER_0);
}

void motor_run(direction_t direction, float speed)
{
    if (direction == MOTOR_RUN_FORWARD) {
        motor_forward(speed);
    } else if (direction == MOTOR_RUN_BACKWORK) {
        motor_backward(speed);
    }

    if (speed == 0.0)
        motor_stop();
}

void motor_controller_task(void)
{
    static uint32_t width = 0;
    adc_value_t *adc_vals = &g_adcs_vals;

    switch (motor.state) {
        case MOTOR_STATE_STOP:
            //ESP_LOGI(TAG, "motor stop.");
            if (adc_vals->motor_fwd_vtg > 200) {
                ESP_LOGI(TAG, "motor start run forward by manual.");
                motor.power_enable = MOTOR_ENABLE;
                motor.direction = MOTOR_RUN_FORWARD;
                motor.speed = 70.0;
                motor.state = MOTOR_STATE_FORWARD;

                motor_enable(MOTOR_ENABLE);
                motor_run(motor.direction, motor.speed);
            }

            if (adc_vals->motor_bwd_vtg > 200) {
                ESP_LOGI(TAG, "motor start run backward by manual.");
                motor.power_enable = MOTOR_ENABLE;
                motor.direction = MOTOR_RUN_BACKWORK;
                motor.speed = 70.0;
                motor.state = MOTOR_STATE_BACKWARD;

                motor_enable(motor.power_enable);
                motor_run(motor.direction, motor.speed);
            }
            break;
        case MOTOR_STATE_FORWARD:
            //ESP_LOGI(TAG, "motor start run forward.");
            if (adc_vals->motor_cur_val > 900) {
                ESP_LOGI(TAG, "motor braking!!!");
                motor.power_enable = MOTOR_DISABLE;
                motor.speed = 0.0;

                motor_enable(motor.power_enable);
                motor_stop();
            }

            if (adc_vals->motor_fwd_vtg < 200) {
                motor.state = MOTOR_STATE_STOP;
            }
            break;
        case MOTOR_STATE_BACKWARD:
            //ESP_LOGI(TAG, "motor start run backward.");
            if (adc_vals->motor_cur_val > 900) {
                ESP_LOGI(TAG, "motor braking!!!");
                motor.power_enable = MOTOR_DISABLE;
                motor.speed = 0;

                motor_enable(motor.power_enable);
                motor_stop();
            }

            if (adc_vals->motor_bwd_vtg < 200) {
                motor.state = MOTOR_STATE_STOP;
            }
            break;
        case MOTOR_STATE_ADJUST:
            ESP_LOGI(TAG, "curtain start to adjust!");
            motor.power_enable = MOTOR_ENABLE;
            motor.speed = 70.0;
            motor.direction = MOTOR_RUN_BACKWORK;

            motor_enable(motor.power_enable);
            motor_run(motor.direction, motor.speed);
            motor.state = MOTOR_ADJUST_BACK_TO_ORIGIN;
            width = 0;
            break;
        case MOTOR_ADJUST_BACK_TO_ORIGIN:
            if (adc_vals->motor_cur_val > 900) {
                ESP_LOGI(TAG, "motor back to origin....");
                motor.power_enable = MOTOR_DISABLE;
                motor.speed = 0;

                motor_enable(motor.power_enable);
                motor_stop();
            }

            if (adc_vals->motor_bwd_vtg < 200) {
                ESP_LOGI(TAG, "start to count width eclipse time");
                motor.power_enable = MOTOR_ENABLE;
                motor.direction = MOTOR_RUN_FORWARD;
                motor.speed = 70.0;
                motor.state = MOTOR_STATE_FORWARD;

                motor_enable(MOTOR_ENABLE);
                motor_run(motor.direction, motor.speed);

                motor.state = MOTOR_ADJUST_ARRIVE_END;
                width = 0;
            }
            break;
        case MOTOR_ADJUST_ARRIVE_END:
            if (adc_vals->motor_cur_val > 900) {
                ESP_LOGI(TAG, "motor arrived the end of other side.");
                motor.power_enable = MOTOR_DISABLE;
                motor.speed = 0;

                motor_enable(motor.power_enable);
                motor_stop();
            }

            if (adc_vals->motor_fwd_vtg < 200) {
                Curtain.device_params.curtain_position = 100;
                Curtain.device_params.curtain_width = width;

                width = 0;
                params_save();
                motor.state = MOTOR_STATE_STOP;
            } else {
                if (width < 0xFFFFFFFF)
                    width++;
            }
            break;
        case MOTOR_POSITION_ADJUST:
            if (Curtain.device_params.curtain_position > motor_target_position(0, 0)) {
                motor.power_enable = MOTOR_ENABLE;
                motor.direction = MOTOR_RUN_BACKWORK;
                motor.speed = 70.0;
                motor.state = MOTOR_POSITION_ADJUST_LEFT;

                motor_enable(motor.power_enable);
                motor_run(motor.direction, motor.speed);
            }

            if (Curtain.device_params.curtain_position < motor_target_position(0, 0)) {
                motor.power_enable = MOTOR_ENABLE;
                motor.direction = MOTOR_RUN_FORWARD;
                motor.speed = 70.0;
                motor.state = MOTOR_POSITION_ADJUST_RIGHT;

                motor_enable(MOTOR_ENABLE);
                motor_run(motor.direction, motor.speed);
            }
            break;
        case MOTOR_POSITION_ADJUST_LEFT:
            if (Curtain.device_params.curtain_position)
                Curtain.device_params.curtain_position--;
            if (Curtain.device_params.curtain_position < motor_target_position(0, 0)) {
                ESP_LOGI(TAG, "motor arrived the target left position.");
                motor.power_enable = MOTOR_DISABLE;
                motor.speed = 0;

                motor_enable(motor.power_enable);
                motor_stop();
                motor.state = MOTOR_STATE_STOP;
            }
            break;
        case MOTOR_POSITION_ADJUST_RIGHT:
            if (Curtain.device_params.curtain_position < Curtain.device_params.curtain_width)
                Curtain.device_params.curtain_position++;
            if (Curtain.device_params.curtain_position > motor_target_position(0, 0)) {
                ESP_LOGI(TAG, "motor arrived the target right position.");
                motor.power_enable = MOTOR_DISABLE;
                motor.speed = 0;

                motor_enable(motor.power_enable);
                motor_stop();
                motor.state = MOTOR_STATE_STOP;
            }
            break;
        default:
            break;
    }
}

uint32_t motor_target_position(uint32_t position, bool set_or_get)
{
    static uint32_t target = 0;

    if (set_or_get) {
        target = position;
    }

    return target;
}

void motor_init(void)
{
    //1. mcpwm gpio initialization
    mcpwm_example_gpio_initialize();
    motor_enable(MOTOR_ENABLE);
    //2. initial mcpwm configuration
    printf("Configuring Initial Parameters of mcpwm...\n");
    mcpwm_config_t pwm_config;
    pwm_config.frequency = 20000;    //frequency = 500Hz,
    pwm_config.cmpr_a = 0.0;    //duty cycle of PWMxA = 0
    pwm_config.cmpr_b = 0.0;    //duty cycle of PWMxb = 0
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);    //Configure PWM0A & PWM0B with above settings
}


/**
 * @brief Configure MCPWM module for brushed dc motor
 */
static void mcpwm_example_brushed_motor_control()
{
    int a = 0;
    while (1) {
        if (a++ > 20) {
            a = 0;
            ESP_LOGI(TAG, "motor run...");
        }
//        brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_0, 60.0);
//        vTaskDelay(2000 / portTICK_RATE_MS);
//        brushed_motor_backward(MCPWM_UNIT_0, MCPWM_TIMER_0, 60.0);
//        vTaskDelay(2000 / portTICK_RATE_MS);
//        brushed_motor_stop(MCPWM_UNIT_0, MCPWM_TIMER_0);
        motor_controller_task();
        get_adcs_values();
        vTaskDelay(50 / portTICK_RATE_MS);
    }
}

void motor_controller_unit_test(void)
{
    motor_init();
    adc_unit_init();

    mcpwm_example_brushed_motor_control();
}