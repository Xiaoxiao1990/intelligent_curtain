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
        .speed = 70.0
};

void motor_enable(motor_power_enable_t operate)
{
    if (operate == MOTOR_ENABLE) {
        gpio_set_level(MOTOR_PWR_EN_PIN, 1);
        motor.power_enable = MOTOR_ENABLE;
    } else {
        gpio_set_level(MOTOR_PWR_EN_PIN, 0);
        motor.power_enable = MOTOR_DISABLE;
    }
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

void motor_backward(void)
{
    if (motor.power_enable == MOTOR_DISABLE) {
        motor_enable(MOTOR_ENABLE);
        vTaskDelay(pdMS_TO_TICKS(50)); // slow start
    }

    if (motor.state != MOTOR_STATE_BACKWARD)
        motor.state = MOTOR_STATE_BACKWARD;

    brushed_motor_backward(MCPWM_UNIT_0, MCPWM_TIMER_0, motor.speed);
}

void motor_forward(void)
{
    if (motor.power_enable == MOTOR_DISABLE) {
        motor_enable(MOTOR_ENABLE);
        vTaskDelay(pdMS_TO_TICKS(500)); // slow start
    }

    if (motor.direction != MOTOR_RUN_FORWARD)
        motor.direction = MOTOR_RUN_FORWARD;

    motor.state = MOTOR_STATE_FORWARD;
    brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_0, motor.speed);
}

void motor_stop(void)
{
    if (motor.power_enable == MOTOR_ENABLE) {
        motor_enable(MOTOR_DISABLE);
    }

    if (motor.state != MOTOR_STATE_PRE_STOP)
        motor.state = MOTOR_STATE_PRE_STOP;

    brushed_motor_stop(MCPWM_UNIT_0, MCPWM_TIMER_0);
}

void motor_run(direction_t direction)
{
    //float speed = 70.0;//motor.speed;
    if (direction == MOTOR_RUN_FORWARD) {
        motor_forward();
    } else if (direction == MOTOR_RUN_BACKWARD) {
        motor_backward();
    }
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
        //motor_controller_task();
        get_adcs_values();
        vTaskDelay(50 / portTICK_RATE_MS);
    }
}


void motor_controller_state_machine(void)
{
    adc_value_t *adc_vals = &g_adcs_vals;

    switch (motor.state) {
        case MOTOR_STATE_STOP:
            // Nothing to do
            break;
        case MOTOR_STATE_PRE_STOP:
            if (adc_vals->motor_cur_vtg < 120 && adc_vals->motor_fwd_vtg < 120 && adc_vals->motor_bwd_vtg < 120) {
                ESP_LOGI(TAG, "motor stopped");
                motor.state = MOTOR_STATE_STOP;
            }
            break;
        case MOTOR_STATE_FORWARD:
            if (adc_vals->motor_cur_val > 900) {
                ESP_LOGI(TAG, "motor braking!!!");
                motor_stop();
            }
            break;
        case MOTOR_STATE_BACKWARD:
            if (adc_vals->motor_cur_val > 900) {
                ESP_LOGI(TAG, "motor braking!!!");
                motor_stop();
            }
            break;
        default:
            motor.state = MOTOR_STATE_PRE_STOP;
            Curtain.state = CURTAIN_IDLE;
            break;
    }
}

void motor_controller_unit_test(void)
{
    motor_init();
    adc_unit_init();

    mcpwm_example_brushed_motor_control();
}