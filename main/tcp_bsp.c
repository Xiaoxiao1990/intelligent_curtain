//
// Created by root on 1/4/20.
//

#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "tcp_bsp.h"
#include "cJSON.h"
#include "esp_types.h"
#include "config.h"
#include "touch_pad.h"
#include "protocol_parser.h"

#define DEVICE_ID       "LY0123456789"

/*typedef enum {
    SET_OK,
    SET_TO_READ_ONLY_FIELD,
    SET_UNSUIABLE_VALUE,
    INVALID_VALUE,
    SET_PARAM_PARSE_FAIL
} SetErrorCode_TypeDef;*/

//socket
static int server_socket = 0;
static struct sockaddr_in server_addr;
static struct sockaddr_in client_addr;
static unsigned int socklen = sizeof(client_addr);
static int connect_socket = 0;
static const char *TAG = "TCP_BSP";
bool g_rxtx_need_restart = false;   //reconnect_flag

static esp_err_t event_handler(void *ctx, system_event_t *event) {
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "got ip:%s\n",
                     ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "station:"
                    MACSTR
                    " join,AID=%d\n",
                     MAC2STR(event->event_info.sta_connected.mac),
                     event->event_info.sta_connected.aid);
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "station:"
                    MACSTR
                    "leave,AID=%d\n",
                     MAC2STR(event->event_info.sta_disconnected.mac),
                     event->event_info.sta_disconnected.aid);
            g_rxtx_need_restart = true;
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

/*
#define CONSTANT_STRING_DEFINE(s)  static const char * s##_CONST_STRING = #s

CONSTANT_STRING_DEFINE(message);
//CONSTANT_STRING_DEFINE(error);
CONSTANT_STRING_DEFINE(code);
CONSTANT_STRING_DEFINE(device_id);
CONSTANT_STRING_DEFINE(message_type);
CONSTANT_STRING_DEFINE(device_params);
CONSTANT_STRING_DEFINE(battery);
CONSTANT_STRING_DEFINE(optical_sensor_status);
CONSTANT_STRING_DEFINE(lumen);
CONSTANT_STRING_DEFINE(curtain_position);
CONSTANT_STRING_DEFINE(report_time);
CONSTANT_STRING_DEFINE(update);
CONSTANT_STRING_DEFINE(result);
CONSTANT_STRING_DEFINE(get);
CONSTANT_STRING_DEFINE(set);
CONSTANT_STRING_DEFINE(action);
CONSTANT_STRING_DEFINE(server_ip);
CONSTANT_STRING_DEFINE(server_port);
CONSTANT_STRING_DEFINE(optical_work_time);
CONSTANT_STRING_DEFINE(curtain_work_time);
CONSTANT_STRING_DEFINE(curtain_repeater);
CONSTANT_STRING_DEFINE(command);
CONSTANT_STRING_DEFINE(args);
CONSTANT_STRING_DEFINE(all);
*/

/*
typedef void cmd_func_t(void *);

typedef struct {
    char cmd[20];
    void *args;
    cmd_func_t *func;
} CMD_TypeDef;

#define CMD_COUNT   3
static void say_hello(void *args);
static void curtain(void *args);
static void led_control(void *args);

static CMD_TypeDef CommandTbl[CMD_COUNT] = {
        {.cmd = "say_hello", .func = say_hello},
        {.cmd = "curtain", .func = curtain},
        {.cmd = "led_control", .func = led_control}
};
*/

/*static void say_hello(void *args)
{
    cJSON *root = (cJSON*)args;
    cJSON *rsp_msg = cJSON_CreateObject();

    char *json_printed = NULL;

    if (root == NULL) {
        ESP_LOGE(TAG, "No json data input");
        return;
    }

    json_printed = cJSON_Print(root);
    ESP_LOGD(TAG, "Server Msg:\n%s", json_printed);
    if (json_printed)
        free(json_printed);

    cJSON_AddStringToObject(rsp_msg, device_id_CONST_STRING, DEVICE_ID);
    char buf[1024] = {0};
    snprintf(buf, 1023, "Hello, %s!", cJSON_GetObjectItem(root, args_CONST_STRING)->valuestring);
    cJSON_AddStringToObject(rsp_msg, message_CONST_STRING, buf);

    json_printed = cJSON_Print(rsp_msg);
    ESP_LOGI(TAG, "RSP_MSG: %s", json_printed);
    send(*((int *)CommandTbl[0].args), json_printed, strlen(json_printed), 0);
    if (json_printed)
        free(json_printed);
    cJSON_free(rsp_msg);

    cJSON_free(rsp_msg);
}*/

/*
static void curtain(void *args)
{
    if (strcmp(args, "on") == 0) {
        green_led(0);
    } else if (strcmp(args, "off") == 0) {
        green_led(1);
    } else {
        ESP_LOGE(TAG, "Illegal args");
    }
}

static void led_control(void *args)
{
    if (strcmp(args, "on") == 0) {
        red_led(0);
    } else if (strcmp(args, "off") == 0) {
        red_led(1);
    } else {
        ESP_LOGE(TAG, "Illegal args");
    }
}
*/

/*
static void command_dispatch(int socket, cJSON *command)
{
    char *cmd = cJSON_GetStringValue(command);

    for (int i = 0; i < CMD_COUNT; ++i) {
        if (strcmp(cmd, CommandTbl[i].cmd) == 0) {
            CommandTbl[i].args = (void *)&socket;
            CommandTbl[i].func(cJSON_GetObjectItem(command, args_CONST_STRING)->valuestring);
        }
    }
}

static void message_dispatch(int socket, cJSON *root)
{
    cJSON *rsp_msg = cJSON_CreateObject();
    char *json_printed = NULL;

    if (root == NULL) {
        ESP_LOGE(TAG, "No json data input");
        return;
    }

    json_printed = cJSON_Print(root);
    ESP_LOGD(TAG, "Server Msg:\n%s", json_printed);
    if (json_printed)
        free(json_printed);

    if (!cJSON_HasObjectItem(root, device_id_CONST_STRING)) {
        ESP_LOGE(TAG, "No device_id field.");
        cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "Device ID is required.");
        cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, -1);
        json_printed = cJSON_Print(rsp_msg);
        ESP_LOGI(TAG, "RSP_MSG: %s", json_printed);
        send(socket, json_printed, strlen(json_printed), 0);
        if (json_printed)
            free(json_printed);
        cJSON_free(rsp_msg);

        return;
    }

    if (!cJSON_HasObjectItem(root, message_type_CONST_STRING)) {
        ESP_LOGE(TAG, "No message_type field.");
        cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "Message type is required.");
        cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, -1);
        json_printed = cJSON_Print(rsp_msg);
        send(socket, json_printed, strlen(json_printed), 0);
        if (json_printed)
            free(json_printed);
        cJSON_free(rsp_msg);
        return;
    }

    char *message_type = cJSON_GetStringValue(cJSON_GetObjectItem(root, message_type_CONST_STRING));

    if (strcmp(message_type, action_CONST_STRING) == 0) {
        if (!cJSON_HasObjectItem(root, command_CONST_STRING)) {
            ESP_LOGE(TAG, "No %s field.", command_CONST_STRING);
            cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "Command required.");
            cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, -1);
            json_printed = cJSON_Print(rsp_msg);
            send(socket, json_printed, strlen(json_printed), 0);
            if (json_printed)
                free(json_printed);
            cJSON_free(rsp_msg);
            return;
        }

        char *cmd = cJSON_GetObjectItem(root, command_CONST_STRING)->valuestring;
        cJSON_AddStringToObject(rsp_msg, device_id_CONST_STRING, DEVICE_ID);
        if (strcmp(cmd, CommandTbl[0].cmd) == 0) {
            if (cJSON_HasObjectItem(root, args_CONST_STRING)) {
                char buf[1023] = {0};
                snprintf(buf, 1023, "Hello, %s", cJSON_GetObjectItem(root, args_CONST_STRING)->valuestring);
                cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "success");
                cJSON_AddStringToObject(rsp_msg, message_CONST_STRING, buf);
                cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, 0);
            } else {
                cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "fail");
                cJSON_AddStringToObject(rsp_msg, message_CONST_STRING, "args required");
                cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, -1);
            }
        } else if (strcmp(cmd, CommandTbl[1].cmd) == 0) {
            if (cJSON_HasObjectItem(root, args_CONST_STRING)) {
                cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "success");
                cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, 0);
                char *args = cJSON_GetObjectItem(root, args_CONST_STRING)->valuestring;
                if (strcmp(args, "on") == 0) {
                    red_led(0);
                } else if (strcmp(args, "off") == 0) {
                    red_led(1);
                }
            } else {
                cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "fail");
                cJSON_AddStringToObject(rsp_msg, message_CONST_STRING, "args required");
                cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, -1);
            }
        } else if (strcmp(cmd, CommandTbl[2].cmd) == 0) {
            if (cJSON_HasObjectItem(root, args_CONST_STRING)) {
                cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "success");
                cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, 0);
                char *args = cJSON_GetObjectItem(root, args_CONST_STRING)->valuestring;
                if (strcmp(args, "on") == 0) {
                    green_led(0);
                } else if (strcmp(args, "off") == 0) {
                    green_led(1);
                }
            } else {
                cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "fail");
                cJSON_AddStringToObject(rsp_msg, message_CONST_STRING, "args required");
                cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, -1);
            }
        } else {
            cJSON_AddStringToObject(rsp_msg, message_CONST_STRING, "Un support command.");
        }

        json_printed = cJSON_Print(rsp_msg);
        send(socket, json_printed, strlen(json_printed), 0);
        if (json_printed)
            free(json_printed);
        cJSON_free(rsp_msg);
    } else if (strcmp(message_type, get_CONST_STRING) == 0) {
        char *device_params;
        if (!cJSON_HasObjectItem(root, device_params_CONST_STRING)) {
            ESP_LOGE(TAG, "No device_params field.");
            cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "Device params is required.");
            cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, -1);
            json_printed = cJSON_Print(rsp_msg);
            send(socket, json_printed, strlen(json_printed), 0);
            if (json_printed)
                free(json_printed);
            cJSON_free(rsp_msg);
            return;
        }

        device_params = cJSON_GetStringValue(cJSON_GetObjectItem(root, device_params_CONST_STRING));
        cJSON_AddStringToObject(rsp_msg, device_id_CONST_STRING, DEVICE_ID);
        cJSON_AddStringToObject(rsp_msg, message_type_CONST_STRING, update_CONST_STRING);
        cJSON_AddObjectToObject(rsp_msg, device_params_CONST_STRING);
        cJSON *rsp_params = cJSON_GetObjectItem(rsp_msg, device_params_CONST_STRING);
        if (strstr(device_params, all_CONST_STRING) != NULL) {
            cJSON_AddNumberToObject(rsp_params, battery_CONST_STRING, Curtain.device_params.battery);
            cJSON_AddNumberToObject(rsp_params, optical_sensor_status_CONST_STRING,
                                    Curtain.device_params.optical_sensor_status);
            cJSON_AddNumberToObject(rsp_params, lumen_CONST_STRING, Curtain.device_params.lumen);
            cJSON_AddNumberToObject(rsp_params, curtain_position_CONST_STRING, Curtain.device_params.curtain_position);
            cJSON_AddStringToObject(rsp_params, server_ip_CONST_STRING, Curtain.device_params.server_address.ip);
            cJSON_AddNumberToObject(rsp_params, server_port_CONST_STRING, Curtain.device_params.server_address.port);
            cJSON_AddItemToObject(rsp_params, optical_work_time_CONST_STRING,
                                  cJSON_CreateIntArray(Curtain.device_params.optical_work_time, 6));
            cJSON_AddItemToObject(rsp_params, curtain_work_time_CONST_STRING,
                                  cJSON_CreateIntArray(Curtain.device_params.curtain_work_time, 6));
            cJSON_AddNumberToObject(rsp_params, curtain_repeater_CONST_STRING, Curtain.device_params.curtain_repeater);
        } else {
            if (strstr(device_params, battery_CONST_STRING) != NULL) {
                cJSON_AddNumberToObject(rsp_params, battery_CONST_STRING, Curtain.device_params.battery);
            }

            if (strstr(device_params, optical_sensor_status_CONST_STRING) != NULL) {
                cJSON_AddNumberToObject(rsp_params, optical_sensor_status_CONST_STRING,
                                        Curtain.device_params.optical_sensor_status);
            }

            if (strstr(device_params, lumen_CONST_STRING) != NULL) {
                cJSON_AddNumberToObject(rsp_params, lumen_CONST_STRING, Curtain.device_params.lumen);
            }

            if (strstr(device_params, curtain_position_CONST_STRING) != NULL) {
                cJSON_AddNumberToObject(rsp_params, curtain_position_CONST_STRING,
                                        Curtain.device_params.curtain_position);
            }

            if (strstr(device_params, server_ip_CONST_STRING) != NULL) {
                cJSON_AddStringToObject(rsp_params, server_ip_CONST_STRING, Curtain.device_params.server_address.ip);
            }

            if (strstr(device_params, server_port_CONST_STRING) != NULL) {
                cJSON_AddNumberToObject(rsp_params, server_port_CONST_STRING, Curtain.device_params.curtain_position);
            }

            if (strstr(device_params, optical_work_time_CONST_STRING) != NULL) {
                cJSON_AddItemToObject(rsp_params, optical_work_time_CONST_STRING,
                                      cJSON_CreateIntArray(Curtain.device_params.optical_work_time, 6));
            }

            if (strstr(device_params, curtain_work_time_CONST_STRING) != NULL) {
                cJSON_AddItemToObject(rsp_params, curtain_work_time_CONST_STRING,
                                      cJSON_CreateIntArray(Curtain.device_params.curtain_work_time, 6));
            }

            if (strstr(device_params, curtain_repeater_CONST_STRING) != NULL) {
                cJSON_AddNumberToObject(rsp_params, curtain_repeater_CONST_STRING,
                                        Curtain.device_params.curtain_repeater);
            }
        }
        cJSON_AddNumberToObject(rsp_msg, report_time_CONST_STRING, esp_timer_get_time());

        json_printed = cJSON_Print(rsp_msg);
        send(socket, json_printed, strlen(json_printed), 0);
        ESP_LOGI(TAG, "Report message:\n%s", json_printed);
        if (json_printed)
            free(json_printed);
        cJSON_free(rsp_msg);
    } else if (strcmp(message_type, set_CONST_STRING) == 0) {
        if (!cJSON_HasObjectItem(root, device_params_CONST_STRING)) {
            ESP_LOGE(TAG, "No device_params field.");
            cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "Device params is required.");
            cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, -1);
            json_printed = cJSON_Print(rsp_msg);
            send(socket, json_printed, strlen(json_printed), 0);
            if (json_printed)
                free(json_printed);
            cJSON_free(rsp_msg);
            return;
        }

        cJSON *params = cJSON_GetObjectItem(root, device_params_CONST_STRING);
        cJSON *item;
        SetErrorCode_TypeDef error_code = SET_PARAM_PARSE_FAIL;
        int data;

        if (cJSON_HasObjectItem(params, battery_CONST_STRING) || cJSON_HasObjectItem(params, lumen_CONST_STRING) ||
            cJSON_HasObjectItem(params, server_ip_CONST_STRING) || cJSON_HasObjectItem(params, server_port_CONST_STRING)) {
            error_code = SET_TO_READ_ONLY_FIELD;
            goto error_process;
        }

        if (cJSON_HasObjectItem(params, optical_sensor_status_CONST_STRING)) {
            item = cJSON_GetObjectItem(params, optical_sensor_status_CONST_STRING);
            if (item->type != cJSON_Number) {
                error_code = SET_UNSUIABLE_VALUE;
                goto error_process;
            }

            data = item->valueint;

            if (data > 1) {
                error_code = INVALID_VALUE;
                goto error_process;
            }

            Curtain.device_params.optical_sensor_status = (uint8_t)data;
            error_code = SET_OK;
        }

        if (cJSON_HasObjectItem(params, curtain_position_CONST_STRING)) {
            item = cJSON_GetObjectItem(params, optical_sensor_status_CONST_STRING);
            if (item->type != cJSON_Number) {
                error_code = SET_UNSUIABLE_VALUE;
                goto error_process;
            }

            data = item->valueint;
            if (data > 100) {
                error_code = INVALID_VALUE;
                goto error_process;
            }

            Curtain.device_params.curtain_position = (uint8_t)data;
            error_code = SET_OK;
        }

        if (cJSON_HasObjectItem(params, optical_work_time_CONST_STRING)) {
            item = cJSON_GetObjectItem(params, optical_sensor_status_CONST_STRING);
            if (item->type != cJSON_Array) {
                error_code = SET_UNSUIABLE_VALUE;
                goto error_process;
            }

            cJSON *time = item;
            if (cJSON_GetArraySize(time) != 6) {
                error_code = INVALID_VALUE;
                goto error_process;
            }

            if (cJSON_GetArrayItem(time, 0)->valueint > 59 || cJSON_GetArrayItem(time, 1)->valueint > 59 ||
                cJSON_GetArrayItem(time, 3)->valueint > 59 || cJSON_GetArrayItem(time, 4)->valueint > 59) {
                error_code = INVALID_VALUE;
                goto error_process;
            }

            if (cJSON_GetArrayItem(time, 2)->valueint > 23 || cJSON_GetArrayItem(time, 5)->valueint > 23) {
                error_code = INVALID_VALUE;
                goto error_process;
            }

            cJSON *element;
            uint8_t i = 0;
            cJSON_ArrayForEach(element, time) {
                Curtain.device_params.optical_work_time[i] = element->valueint;
                ++i;
            }

            error_code = SET_OK;
        }

        if (cJSON_HasObjectItem(params, curtain_work_time_CONST_STRING)) {
            item = cJSON_GetObjectItem(params, optical_sensor_status_CONST_STRING);
            if (item->type != cJSON_Array) {
                error_code = SET_UNSUIABLE_VALUE;
                goto error_process;
            }

            cJSON *time = item;
            if (cJSON_GetArraySize(time) != 6) {
                error_code = INVALID_VALUE;
                goto error_process;
            }

            if (cJSON_GetArrayItem(time, 0)->valueint > 59 || cJSON_GetArrayItem(time, 1)->valueint > 59 ||
                cJSON_GetArrayItem(time, 3)->valueint > 59 || cJSON_GetArrayItem(time, 4)->valueint > 59) {
                error_code = INVALID_VALUE;
                goto error_process;
            }

            if (cJSON_GetArrayItem(time, 2)->valueint > 23 || cJSON_GetArrayItem(time, 5)->valueint > 23) {
                error_code = INVALID_VALUE;
                goto error_process;
            }

            cJSON *element;
            uint8_t i = 0;
            cJSON_ArrayForEach(element, time) {
                Curtain.device_params.curtain_work_time[i] = element->valueint;
                ++i;
            }

            error_code = SET_OK;
        }

        if (cJSON_HasObjectItem(params, curtain_repeater_CONST_STRING)) {
            item = cJSON_GetObjectItem(params, optical_sensor_status_CONST_STRING);
            if (item->type != cJSON_Number) {
                error_code = SET_UNSUIABLE_VALUE;
                goto error_process;
            }
            data = item->valueint;
            if (data > 0x7F) {
                error_code = INVALID_VALUE;
                goto error_process;
            }

            Curtain.device_params.curtain_repeater = (uint8_t)data;
            error_code = SET_OK;
        }

    error_process:
        switch (error_code) {
            case SET_OK:
                cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "OK");
                cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, 0);
                // TODO save to flash
                params_save();
                break;
            case SET_TO_READ_ONLY_FIELD:
                cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "Can not set read-only field.");
                cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, -1);
                break;
            case SET_UNSUIABLE_VALUE:
                cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "Incompatible value types");
                cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, -1);
                break;
            case INVALID_VALUE:
                cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "Invalid value.");
                cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, -1);
                break;
            case SET_PARAM_PARSE_FAIL:
                cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "Cannot parse any params.");
                cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, -1);
                break;
        }
        json_printed = cJSON_Print(rsp_msg);
        send(socket, json_printed, strlen(json_printed), 0);
        ESP_LOGI(TAG, "Report message:\n%s", json_printed);
        if (json_printed)
            free(json_printed);
        cJSON_free(rsp_msg);
    } else {
        // Undefined operates
        ESP_LOGE(TAG, "Illegal operates.");
        cJSON_AddStringToObject(rsp_msg, result_CONST_STRING, "Illegal operates.");
        cJSON_AddNumberToObject(rsp_msg, code_CONST_STRING, -1);
        json_printed = cJSON_Print(rsp_msg);
        send(socket, json_printed, strlen(json_printed), 0);
        if (json_printed)
            free(json_printed);
        cJSON_free(rsp_msg);
    }
}
*/

#define BUF_SIZE 1024

/*
void recv_data(void *pvParameters) {
    int len = 0;
    char databuff[BUF_SIZE];
    cJSON *json_data = NULL;
    char *msg_err = "{\"message\":\"error\",\"code\":-1}";
    cJSON *rsp_msg = cJSON_CreateObject();
    char *json_printed = NULL;

    ESP_LOGI(TAG, "start received data...");

    cJSON_AddStringToObject(rsp_msg, device_id_CONST_STRING, DEVICE_ID);
    cJSON_AddStringToObject(rsp_msg, message_type_CONST_STRING, update_CONST_STRING);
    cJSON_AddObjectToObject(rsp_msg, device_params_CONST_STRING);
    cJSON *rsp_params = cJSON_GetObjectItem(rsp_msg, device_params_CONST_STRING);
    cJSON_AddNumberToObject(rsp_params, battery_CONST_STRING, Curtain.device_params.battery);
    cJSON_AddNumberToObject(rsp_params, optical_sensor_status_CONST_STRING,
                            Curtain.device_params.optical_sensor_status);
    cJSON_AddNumberToObject(rsp_params, lumen_CONST_STRING, Curtain.device_params.lumen);
    cJSON_AddNumberToObject(rsp_params, curtain_position_CONST_STRING, Curtain.device_params.curtain_position);
    cJSON_AddStringToObject(rsp_params, server_ip_CONST_STRING, Curtain.device_params.server_address.ip);
    cJSON_AddNumberToObject(rsp_params, server_port_CONST_STRING, Curtain.device_params.server_address.port);
    cJSON_AddItemToObject(rsp_params, optical_work_time_CONST_STRING,
                          cJSON_CreateIntArray(Curtain.device_params.optical_work_time, 6));
    cJSON_AddItemToObject(rsp_params, curtain_work_time_CONST_STRING,
                          cJSON_CreateIntArray(Curtain.device_params.curtain_work_time, 6));
    cJSON_AddNumberToObject(rsp_params, curtain_repeater_CONST_STRING, Curtain.device_params.curtain_repeater);

    cJSON_AddNumberToObject(rsp_msg, report_time_CONST_STRING, esp_timer_get_time());

    json_printed = cJSON_Print(rsp_msg);
    send(connect_socket, json_printed, strlen(json_printed), 0);
    ESP_LOGI(TAG, "First update message:\n%s", json_printed);
    if (json_printed)
        free(json_printed);
    cJSON_free(rsp_msg);

    while (1) {
        memset(databuff, 0x00, sizeof(databuff));
        len = recv(connect_socket, databuff, sizeof(databuff), 0);
        g_rxtx_need_restart = false;
        if (len > 0) {
            ESP_LOGI(TAG, "recvData: %s", databuff);
            databuff[len] = 0;
            json_data = cJSON_Parse(databuff);
            message_dispatch(connect_socket, json_data);
            if (json_data)
                cJSON_free(json_data);
            else
                send(connect_socket, msg_err, strlen(msg_err), 0);
            json_data = NULL;
            //send(connect_socket, databuff, strlen(databuff), 0);
            //sendto(connect_socket, databuff , sizeof(databuff), 0, (struct sockaddr *) &remote_addr,sizeof(remote_addr));
        } else {
            show_socket_error_reason("recv_data", connect_socket);
            g_rxtx_need_restart = true;

#if TCP_SERVER_CLIENT_OPTION
            //服务器接收异常，不用break后close socket,因为有其他client
            //break;
            vTaskDelete(NULL);
#else
            //client
            break;
#endif
        }
    }
    close_socket();
    g_rxtx_need_restart = true;
    vTaskDelete(NULL);
}
*/
void heart_beat(void)
{
    uint8_t tx[200] = { 0 };
    tx[0] = 0x55;
    tx[1] = 0x01;
    tx[2] = 0x01;
    // id 6bytes
    esp_read_mac(Curtain.device_id, ESP_MAC_BT);
    esp_log_buffer_hex("MAC", Curtain.device_id, 6);
    memcpy(&(tx[3]), &Curtain.device_id, 6);
    // battery
    tx[9] = Curtain.battery;
    // battery temp
    tx[10] = Curtain.bat_temp;
    // battery status, 0:not charge, 1: charging, 2:charged
    tx[11] = Curtain.bat_state;
    // curtain position
    tx[12] = (uint8_t)Curtain.curtain_ratio;
    // optical status
    tx[13] = Curtain.optical_sensor_status;
    // lumen
    tx[14] = Curtain.lumen;
    // work mode, 0:no mode, 1: summer, 2:winter
    tx[15] = Curtain.work_mode;
    // light gate value
    tx[16] = (uint8_t)Curtain.lumen_gate_value;
    send(connect_socket, tx, 17, 0);
}
void recv_data(void *pvParameters) {
    int len = 0;
    //char databuff[BUF_SIZE];
    protocol_data_block_t data;

    ESP_LOGI(TAG, "start received data...");

    while (1) {
        memset(data.rx_data, 0x00, PROTOCOL_BUFFER_LEN);
        len = recv(connect_socket, data.rx_data, PROTOCOL_BUFFER_LEN, 0);
        g_rxtx_need_restart = false;
        if (len > 0) {
            data.rx_len = (uint16_t)len;
            esp_log_buffer_hex(TAG, data.rx_data, data.rx_len);
            data.rx_data[data.rx_len] = 0;
            protocol_parser(&data);
            send(connect_socket, data.tx_data, data.tx_len, 0);
            //sendto(connect_socket, databuff , sizeof(databuff), 0, (struct sockaddr *) &remote_addr,sizeof(remote_addr));
        } else {
            show_socket_error_reason("recv_data", connect_socket);
            g_rxtx_need_restart = true;

#if TCP_SERVER_CLIENT_OPTION
            //服务器接收异常，不用break后close socket,因为有其他client
            //break;
            vTaskDelete(NULL);
#else
            //client
            break;
#endif
        }
    }
    close_socket();
    g_rxtx_need_restart = true;
    vTaskDelete(NULL);
}

esp_err_t create_tcp_server(bool isCreatServer) {

    if (isCreatServer) {
        ESP_LOGI(TAG, "server socket....,port=%d", CONFIG_TCP_PERF_SERVER_PORT);
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0) {
            show_socket_error_reason("create_server", server_socket);
            close(server_socket);
            return ESP_FAIL;
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(CONFIG_TCP_PERF_SERVER_PORT);
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
            show_socket_error_reason("bind_server", server_socket);
            close(server_socket);
            return ESP_FAIL;
        }
    }

    if (listen(server_socket, 5) < 0) {
        show_socket_error_reason("listen_server", server_socket);
        close(server_socket);
        return ESP_FAIL;
    }

    connect_socket = accept(server_socket, (struct sockaddr *) &client_addr, &socklen);
    if (connect_socket < 0) {
        show_socket_error_reason("accept_server", connect_socket);
        close(server_socket);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "tcp connection established!");
    return ESP_OK;
}

static struct addrinfo *resolve_host_name(const char *host, size_t hostlen)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char *use_host = strndup(host, hostlen);
    if (!use_host) {
        return NULL;
    }

    ESP_LOGD(TAG, "host:%s: strlen %lu", use_host, (unsigned long)hostlen);
    struct addrinfo *res;
    if (getaddrinfo(use_host, NULL, &hints, &res)) {
        ESP_LOGE(TAG, "couldn't get hostname for :%s:", use_host);
        free(use_host);
        return NULL;
    }
    free(use_host);
    return res;
}

static int tcp_client_task(char *ip, size_t len)
{
//    char rx_buffer[128];
//    char addr_str[128];
//    int addr_family;
//    int ip_protocol;

    struct addrinfo *cur;
    // 域名解析得到ip地址
    struct addrinfo *res = resolve_host_name(SERVER_DOMAIN, strlen(SERVER_DOMAIN));
    if (!res) {
        ESP_LOGI(TAG, "DNS resolve host name failed!");
        return -1;
    }

    for (cur = res; cur != NULL; cur = cur->ai_next) {
        inet_ntop(AF_INET, &(((struct sockaddr_in *)cur->ai_addr)->sin_addr), ip, len);
        ESP_LOGI(TAG, "DNS IP:[%s]", ip);
        if (strlen(ip) != 0) {
            ESP_LOGI(TAG, "Server IP: %s Server Port:%d", ip, SERVER_PORT);
            return 0;
        }
    }

    return -1;
}


esp_err_t create_tcp_client(void) {
    char ip[128] = { 0 };
    while (1) {
        if (!tcp_client_task(ip, 128))
            break;

        vTaskDelay(10000 / portTICK_RATE_MS);
    }

    //ESP_LOGI(TAG, "will connect gateway IP: %s port:%d", Curtain.server_address.ip, Curtain.server_address.port);
    ESP_LOGI(TAG, "will connect ip: %s port:%d", ip, SERVER_PORT); //客户要求写死IP及端口
    connect_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (connect_socket < 0) {
        show_socket_error_reason("create client", connect_socket);
        close(connect_socket);
        return ESP_FAIL;
    }

    server_addr.sin_family = AF_INET;
//    server_addr.sin_port = htons(Curtain.server_address.port);
//    server_addr.sin_addr.s_addr = inet_addr(Curtain.server_address.ip);
    server_addr.sin_port = htons(SERVER_PORT);//客户要求写死IP及端口
    server_addr.sin_addr.s_addr = inet_addr(ip);//客户要求写死IP及端口
    ESP_LOGI(TAG, "connecting server...");

    if (connect(connect_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        show_socket_error_reason("client connect", connect_socket);
        ESP_LOGE(TAG, "connect failed!");
        close(connect_socket);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "connect success!");
    return ESP_OK;
}

void wifi_init_sta() {
    wifi_event_group = xEventGroupCreate();
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
            .sta = {
                    .ssid = GATEWAY_SSID,
                    .password = GATEWAY_PAS},
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s \n",
             GATEWAY_SSID, GATEWAY_PAS);
}

void wifi_init_softap() {
    wifi_event_group = xEventGroupCreate();
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
            .ap = {
                    .ssid = SOFT_AP_SSID,
                    .password = SOFT_AP_PAS,
                    .ssid_len = 0,
                    .max_connection = SOFT_AP_MAX_CONNECT,
                    .authmode = WIFI_AUTH_WPA_WPA2_PSK
            }
    };
    if (strlen(SOFT_AP_PAS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "SoftAP set finish,SSID:%s password:%s \n",
             wifi_config.ap.ssid, wifi_config.ap.password);
}

int get_socket_error_code(int socket) {
    int result;
    u32_t optlen = sizeof(int);
    int err = getsockopt(socket, SOL_SOCKET, SO_ERROR, &result, &optlen);
    if (err == -1) {
        //WSAGetLastError();
        //ESP_LOGE(TAG, "socket error code:%d", err);
        ESP_LOGE(TAG, "socket error code[%d]:%s", err, strerror(err));
        return -1;
    }
    return result;
}

int show_socket_error_reason(const char *str, int socket) {
    int err = get_socket_error_code(socket);

    if (err != 0) {
        ESP_LOGW(TAG, "%s socket error reason %d %s", str, err, strerror(err));
    }

    return err;
}

int check_working_socket() {
    int ret;
#if EXAMPLE_ESP_TCP_MODE_SERVER
    ESP_LOGD(TAG, "check server_socket");
    ret = get_socket_error_code(server_socket);
    if (ret != 0)
    {
        ESP_LOGW(TAG, "server socket error %d %s", ret, strerror(ret));
    }
    if (ret == ECONNRESET)
    {
        return ret;
    }
#endif
    ESP_LOGD(TAG, "check connect_socket");
    ret = get_socket_error_code(connect_socket);
    if (ret != 0) {
        ESP_LOGW(TAG, "connect socket error %d %s", ret, strerror(ret));
    }
    if (ret != 0) {
        return ret;
    }
    return 0;
}

void close_socket() {
    close(connect_socket);
    close(server_socket);
}