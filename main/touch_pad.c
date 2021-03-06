/* Touch Pad Interrupt Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/touch_pad.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "touch_pad.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "network_config.h"
#include "config.h"

/* Can run 'make menuconfig' to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO0 22
#define BLINK_GPIO1 23

static const char* TAG = "Touch pad";
#define TOUCH_THRESH_NO_USE   (0)
#define TOUCH_THRESH_PERCENT  (80)
#define TOUCHPAD_FILTER_TOUCH_PERIOD (10)

#define TOUCH_PAD   TOUCH_PAD_NUM7

static bool s_pad_activated;
//static uint32_t s_pad_init_val;

/*
  Read values sensed at all available touch pads.
  Use 2 / 3 of read value as the threshold
  to trigger interrupt when the pad is touched.
  Note: this routine demonstrates a simple way
  to configure activation threshold for the touch pads.
  Do not touch any pads when this routine
  is running (on application start).
 */
static void tp_set_thresholds(void)
{
    uint16_t touch_value;

    //read filtered value
    touch_pad_read_filtered(TOUCH_PAD, &touch_value);
    ESP_LOGI(TAG, "test init: touch pad [%d] val is %d", TOUCH_PAD, touch_value);

    //set interrupt threshold.
    ESP_ERROR_CHECK(touch_pad_set_thresh(TOUCH_PAD, touch_value * 2 / 3));
}

/*
  Check if any of touch pads has been activated
  by reading a table updated by rtc_intr()
  If so, then print it out on a serial monitor.
  Clear related entry in the table afterwards

  In interrupt mode, the table is updated in touch ISR.

  In filter mode, we will compare the current filtered value with the initial one.
  If the current filtered value is less than 80% of the initial value, we can
  regard it as a 'touched' event.
  When calling touch_pad_init, a timer will be started to run the filter.
  This mode is designed for the situation that the pad is covered
  by a 2-or-3-mm-thick medium, usually glass or plastic.
  The difference caused by a 'touch' action could be very small, but we can still use
  filter mode to detect a 'touch' event.
 */
static void tp_read_task(void *pvParameter)
{
    int press_time_cnt = 0, release_time_cnt = 0;
    uint8_t state = 0;
    int led_cnt = 0;

    //interrupt mode, enable touch interrupt
    touch_pad_intr_enable();
    gpio_set_level(BLINK_GPIO0, 1);
    gpio_set_level(BLINK_GPIO1, 1);

    network_state = NETWORK_DO_NOT_CONFIG;

    while (1) {
        switch (network_state) {
            case NETWORK_DO_NOT_CONFIG:
                //gpio_set_level(BLINK_GPIO1, 0);
                break;
            case NETWORK_CONNECTTING:
                if (led_cnt++ < 5) {
                    gpio_set_level(BLINK_GPIO1, 0);
                } else if (led_cnt < 10) {
                    gpio_set_level(BLINK_GPIO1, 1);
                } else {
                    led_cnt = 0;
                }
                break;
            case NETWORK_CONNECTED:
                gpio_set_level(BLINK_GPIO1, 1);
                break;
            case NETWORK_FIRST_CONFIG:
                if (led_cnt++ < 3) {
                    gpio_set_level(BLINK_GPIO1, 0);
                } else if (led_cnt < 6) {
                    gpio_set_level(BLINK_GPIO1, 1);
                } else {
                    led_cnt = 0;
                }
                break;
            case NETWORK_ERROR:
                gpio_set_level(BLINK_GPIO1, 1);
                break;
            default:
                gpio_set_level(BLINK_GPIO1, 1);
                break;
        }

        if (s_pad_activated == true) {
            s_pad_activated = false;
            // Wait a while for the pad being released
            //vTaskDelay(20 / portTICK_PERIOD_MS);
            press_time_cnt++;
            release_time_cnt = 0;
        } else {
            release_time_cnt++;
            press_time_cnt = 0;
        }

        switch (state) {
            case 0:
                if (press_time_cnt > 120) {
                    state++;
                    release_time_cnt = 0;
                    ESP_LOGI(TAG, "It's time to re-config WiFi...");
                    network_state = NETWORK_FIRST_CONFIG;
                    Curtain.is_wifi_config = false;
                }
                break;
            case 1:
                if (release_time_cnt > 2) {
                    ESP_LOGI(TAG, "Re-config WiFi now...");
                    network_state = NETWORK_FIRST_CONFIG;
                    Curtain.is_wifi_config = false;
                    params_save();
                    esp_restart();
                }
                break;
            default:
                state = 0;
                break;
        }

        vTaskDelay(40 / portTICK_PERIOD_MS);
    }
}

/*
  Handle an interrupt triggered when a pad is touched.
  Recognize what pad has been touched and save it in a table.
 */

static tp_callback_func_t *callback_func = NULL;

static void tp_rtc_intr(void * arg)
{
    uint32_t pad_intr = touch_pad_get_status();
    //clear interrupt
    touch_pad_clear_status();
    if ((pad_intr >> TOUCH_PAD) & 0x01) {
        s_pad_activated = true;
    }
}

void touch_pad_initial_unuse(tp_callback_func_t func)
{
    callback_func = func;
    gpio_pad_select_gpio(BLINK_GPIO0);
    gpio_pad_select_gpio(BLINK_GPIO1);
/* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO0, GPIO_MODE_OUTPUT);
    gpio_set_direction(BLINK_GPIO1, GPIO_MODE_OUTPUT);
}

void touch_pad_initialize(void)
{
    gpio_pad_select_gpio(BLINK_GPIO0);
    gpio_pad_select_gpio(BLINK_GPIO1);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO0, GPIO_MODE_OUTPUT);
    gpio_set_direction(BLINK_GPIO1, GPIO_MODE_OUTPUT);

    // Initialize touch pad peripheral, it will start a timer to run a filter
    ESP_LOGI(TAG, "Initializing touch pad");
    touch_pad_init();
    // If use interrupt trigger mode, should set touch sensor FSM mode at 'TOUCH_FSM_MODE_TIMER'.
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    // Set reference voltage for charging/discharging
    // For most usage scenarios, we recommend using the following combination:
    // the high reference valtage will be 2.7V - 1V = 1.7V, The low reference voltage will be 0.5V.
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    // Init touch pad IO
    //init RTC IO and mode for touch pad.
    touch_pad_config(TOUCH_PAD, TOUCH_THRESH_NO_USE);
    // Initialize and start a software filter to detect slight change of capacitance.
    touch_pad_filter_start(TOUCHPAD_FILTER_TOUCH_PERIOD);
    // Set thresh hold
    tp_set_thresholds();
    // Register touch interrupt ISR
    touch_pad_isr_register(tp_rtc_intr, NULL);
    // Start a task to show what pads have been touched
    xTaskCreate(&tp_read_task, "touch_pad_read_task", 2048, NULL, 5, NULL);
}

void touch_pad_unit_test(void)
{
    gpio_pad_select_gpio(BLINK_GPIO0);
    gpio_pad_select_gpio(BLINK_GPIO1);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO0, GPIO_MODE_OUTPUT);
    gpio_set_direction(BLINK_GPIO1, GPIO_MODE_OUTPUT);

    // Initialize touch pad peripheral, it will start a timer to run a filter
    ESP_LOGI(TAG, "Initializing touch pad");
    touch_pad_init();
    // If use interrupt trigger mode, should set touch sensor FSM mode at 'TOUCH_FSM_MODE_TIMER'.
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    // Set reference voltage for charging/discharging
    // For most usage scenarios, we recommend using the following combination:
    // the high reference valtage will be 2.7V - 1V = 1.7V, The low reference voltage will be 0.5V.
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    // Init touch pad IO
    //init RTC IO and mode for touch pad.
    touch_pad_config(TOUCH_PAD, TOUCH_THRESH_NO_USE);
    // Initialize and start a software filter to detect slight change of capacitance.
    touch_pad_filter_start(TOUCHPAD_FILTER_TOUCH_PERIOD);
    // Set thresh hold
    tp_set_thresholds();
    // Register touch interrupt ISR
    touch_pad_isr_register(tp_rtc_intr, NULL);
    // Start a task to show what pads have been touched
    xTaskCreate(&tp_read_task, "touch_pad_read_task", 2048, NULL, 5, NULL);
}

void green_led(int state)
{
    gpio_set_level(BLINK_GPIO0, state);
}

void red_led(int state)
{
    gpio_set_level(BLINK_GPIO1, state);
}