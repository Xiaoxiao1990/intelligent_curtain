//
// Created by root on 1/4/20.
//

#include <string.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "tcp_bsp.h"
#include "cJSON.h"

#define DEVICE_ID       "LY0123456789"

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

static void message_dispatch(int socket, cJSON *root)
{
    char *msg_ok = "{\"result\":\"ok\",\"code\":0}";
    char *msg_fail = "{\"result\":\"fail\",\"code\":-1}";

    cJSON *rsp_msg = NULL;

    char *json_printed = NULL;

    if (root == NULL) {
        ESP_LOGE(TAG, "No json data input");
        return;
    }

    json_printed = cJSON_Print(root);
    ESP_LOGD(TAG, "Server Msg:\n%s", json_printed);
    if (json_printed)
        free(json_printed);

    if (!cJSON_HasObjectItem(root, "device_id")) {
        ESP_LOGE(TAG, "No device_id field.");
        cJSON_AddStringToObject(rsp_msg, "result", "Device ID is required.");
        cJSON_AddNumberToObject(rsp_msg, "code", -1);
        json_printed = cJSON_Print(rsp_msg);
        send(socket, json_printed, strlen(json_printed), 0);
        if (json_printed)
            free(json_printed);
        cJSON_free(rsp_msg);

        return;
    }

    if (!cJSON_HasObjectItem(root, "message_type")) {
        ESP_LOGE(TAG, "No message_type field.");
        cJSON_AddStringToObject(rsp_msg, "result", "Message type is required.");
        cJSON_AddNumberToObject(rsp_msg, "code", -1);
        json_printed = cJSON_Print(rsp_msg);
        send(socket, json_printed, strlen(json_printed), 0);
        if (json_printed)
            free(json_printed);
        cJSON_free(rsp_msg);
        return;
    }

    char *message_type = cJSON_GetStringValue(cJSON_GetObjectItem(root, "message_type"));
    char *device_params;
    if (strcmp(message_type, "action") == 0) {
        ESP_LOGE(TAG, "Actions do not implement so far.");
        cJSON_AddStringToObject(rsp_msg, "result", "Actions do not implement so far.");
        cJSON_AddNumberToObject(rsp_msg, "code", -1);
        json_printed = cJSON_Print(rsp_msg);
        send(socket, json_printed, strlen(json_printed), 0);
        if (json_printed)
            free(json_printed);
        cJSON_free(rsp_msg);
        return;
    } else if (strcmp(message_type, "get") == 0) {
        if (!cJSON_HasObjectItem(root, "device_params")) {
            ESP_LOGE(TAG, "No device_params field.");
            cJSON_AddStringToObject(rsp_msg, "result", "Device params is required.");
            cJSON_AddNumberToObject(rsp_msg, "code", -1);
            json_printed = cJSON_Print(rsp_msg);
            send(socket, json_printed, strlen(json_printed), 0);
            if (json_printed)
                free(json_printed);
            cJSON_free(rsp_msg);
            return;
        }

        device_params = cJSON_GetStringValue(cJSON_GetObjectItem(root, "device_params"));
        cJSON_AddStringToObject(rsp_msg, "message_type", "update");
        cJSON_AddObjectToObject(rsp_msg, "device_status");
        cJSON *rsp_params = cJSON_GetObjectItem(rsp_msg, "device_status");
        if (strcmp(device_params, "all") == 0) {
            cJSON_AddNumberToObject(rsp_params, "battery", 80);
            cJSON_AddNumberToObject(rsp_params, "optical_sensor_status", 0);
            cJSON_AddNumberToObject(rsp_params, "lumen", 50);
            cJSON_AddNumberToObject(rsp_params, "curtain_position", 50);
        } else {
            cJSON *device_params_item = cJSON_GetObjectItem(root, "device_params");
            if (cJSON_HasObjectItem(device_params_item, "battery")) {
                cJSON_AddNumberToObject(rsp_params, "battery", 80);
            }

            if (cJSON_HasObjectItem(device_params_item, "optical_sensor_status")) {
                cJSON_AddNumberToObject(rsp_params, "optical_sensor_status", 1);
            }

            if (cJSON_HasObjectItem(device_params_item, "lumen")) {
                cJSON_AddNumberToObject(rsp_params, "lumen", 50);
            }

            if (cJSON_HasObjectItem(device_params_item, "curtain_position")) {
                cJSON_AddNumberToObject(rsp_params, "curtain_position", 50);
            }
        }
        cJSON_AddNumberToObject(rsp_params, "report_time", system_get_time());

        json_printed = cJSON_Print(rsp_msg);
        send(socket, json_printed, strlen(json_printed), 0);
        ESP_LOGI(TAG, "Report message:\n%s", json_printed);
        if (json_printed)
            free(json_printed);
        cJSON_free(rsp_msg);
    } else if (strcmp(message_type, "set") == 0) {
        if (!cJSON_HasObjectItem(root, "device_params")) {
            ESP_LOGE(TAG, "No device_params field.");
            cJSON_AddStringToObject(rsp_msg, "result", "Device params is required.");
            cJSON_AddNumberToObject(rsp_msg, "code", -1);
            json_printed = cJSON_Print(rsp_msg);
            send(socket, json_printed, strlen(json_printed), 0);
            if (json_printed)
                free(json_printed);
            cJSON_free(rsp_msg);
            return;
        }

        device_params = cJSON_GetStringValue(cJSON_GetObjectItem(root, "device_params"));
        cJSON *rsp_params = cJSON_GetObjectItem(rsp_msg, "device_status");
        if (strcmp(device_params, "all") == 0) {
            cJSON_AddNumberToObject(rsp_params, "battery", 80);
            cJSON_AddNumberToObject(rsp_params, "optical_sensor_status", 0);
            cJSON_AddNumberToObject(rsp_params, "lumen", 50);
            cJSON_AddNumberToObject(rsp_params, "curtain_position", 50);
        } else {
            cJSON *device_params_item = cJSON_GetObjectItem(root, "device_params");
            if (cJSON_HasObjectItem(device_params_item, "battery")) {
                cJSON_AddNumberToObject(rsp_params, "battery", 80);
            }

            if (cJSON_HasObjectItem(device_params_item, "optical_sensor_status")) {
                cJSON_AddNumberToObject(rsp_params, "optical_sensor_status", 1);
            }

            if (cJSON_HasObjectItem(device_params_item, "lumen")) {
                cJSON_AddNumberToObject(rsp_params, "lumen", 50);
            }

            if (cJSON_HasObjectItem(device_params_item, "curtain_position")) {
                cJSON_AddNumberToObject(rsp_params, "curtain_position", 50);
            }
        }
        cJSON_AddNumberToObject(rsp_params, "report_time", system_get_time());

        json_printed = cJSON_Print(rsp_msg);
        send(socket, json_printed, strlen(json_printed), 0);
        if (json_printed)
            free(json_printed);
        cJSON_free(rsp_msg);
    } else {
        // Undefined operates
        ESP_LOGE(TAG, "Illegal operates.");
        cJSON_AddStringToObject(rsp_msg, "result", "Illegal operates.");
        cJSON_AddNumberToObject(rsp_msg, "code", -1);
        json_printed = cJSON_Print(rsp_msg);
        send(socket, json_printed, strlen(json_printed), 0);
        if (json_printed)
            free(json_printed);
        cJSON_free(rsp_msg);
    }
}

#define BUF_SIZE 1024

void recv_data(void *pvParameters) {
    int len = 0;
    char databuff[BUF_SIZE];
    cJSON *json_data = NULL;
    char *msg_err = "{\"message\":\"error\",\"code\":-1}";
    char *first_report_fmt = "{\n"
                             "    \"device_id\": \"%s\",\n"
                             "    \"message_type\": \"%s\",\n"
                             "    \"device_status\": {\n"
                             "        \"battery\": %d,\n"
                             "        \"optical_sensor_status\": %d,\n"
                             "        \"lumen\": %d,\n"
                             "        \"curtain_position\": %d\n"
                             "    },\n"
                             "    \"report_time\": %d\n"
                             "}";
    ESP_LOGI(TAG, "start received data...");

    snprintf(databuff, BUF_SIZE - 1, first_report_fmt, DEVICE_ID, "update", 88, 1, 50, 50, system_get_time());
    if (send(connect_socket, databuff, strlen(databuff), 0) < 0) {
        ESP_LOGE(TAG, "Send first update failure.");
    } else {
        ESP_LOGI(TAG, "Send first update success!");
    }

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

esp_err_t create_tcp_server(bool isCreatServer) {

    if (isCreatServer) {
        ESP_LOGI(TAG, "server socket....,port=%d", TCP_PORT);
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0) {
            show_socket_error_reason("create_server", server_socket);
            close(server_socket);
            return ESP_FAIL;
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(TCP_PORT);
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

esp_err_t create_tcp_client() {
    ESP_LOGI(TAG, "will connect gateway ssid : %s port:%d",
             TCP_SERVER_ADRESS, TCP_PORT);
    connect_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (connect_socket < 0) {
        show_socket_error_reason("create client", connect_socket);
        close(connect_socket);
        return ESP_FAIL;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = inet_addr(TCP_SERVER_ADRESS);
    ESP_LOGI(TAG, "connectting server...");

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
        ESP_LOGE(TAG, "socket error code:%d", err);
        ESP_LOGE(TAG, "socket error code:%s", strerror(err));
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