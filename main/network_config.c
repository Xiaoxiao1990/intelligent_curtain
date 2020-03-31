//
// Created by root on 1/4/20.
//

#include <stdlib.h>
#include <esp_err.h>
#include <esp_event_legacy.h>
#include <esp_log.h>
#include <freertos/event_groups.h>
#include <esp_wifi.h>
#include <esp_smartconfig.h>
#include <string.h>
#include <nvs_flash.h>
#include <esp_event_loop.h>
#include <esp_wifi_types.h>
#include "network_config.h"
#include "tcp_client.h"
#include "config.h"

static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const char *TAG = "sc";
static const char *TAG1 = "u_event";

EventGroupHandle_t wifi_event_group;
Network_State_Typedef network_state;

/*smartconfig事件回调
* @param[in]   status  		       :事件状态
* @retval      void                 :无
* @note        修改日志
*               Ver0.0.1:
                    hx-zsj, 2018/08/10, 初始化版本\n
*/
wifi_config_t *new_wifi_config;
static void sc_callback(smartconfig_status_t status, void *pdata) {
    switch (status) {
        case SC_STATUS_WAIT:                    //等待配网
            ESP_LOGI(TAG, "SC_STATUS_WAIT");
            break;
        case SC_STATUS_FIND_CHANNEL:            //扫描信道
            ESP_LOGI(TAG, "SC_STATUS_FINDING_CHANNEL");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:       //获取到ssid和密码
            ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD");
            break;
        case SC_STATUS_LINK:                    //连接获取的ssid和密码
            ESP_LOGI(TAG, "SC_STATUS_LINK");
            wifi_config_t *new_wifi_config = pdata;
            //打印账号密码
            ESP_LOGI(TAG, "SSID:%s", new_wifi_config->sta.ssid);
            ESP_LOGI(TAG, "PASSWORD:%s", new_wifi_config->sta.password);
            memcpy(&Curtain.wifi_config, new_wifi_config, sizeof(wifi_config_t));
            Curtain.is_wifi_config = true;
            if (ESP_OK != params_save()) {
                ESP_LOGE(TAG, "Save wifi config failed.");
                Curtain.is_wifi_config = false;
            } else {
                ESP_LOGI(TAG, "Save wifi config success.");
            }
            //断开默认的
            ESP_ERROR_CHECK(esp_wifi_disconnect());
            //设置获取的ap和密码到寄存器
            ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, new_wifi_config));
            //连接获取的ssid和密码
            ESP_ERROR_CHECK(esp_wifi_connect());
            break;
        case SC_STATUS_LINK_OVER: //连接上配置后，结束
            ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
            //
            if (pdata != NULL) {
                uint8_t phone_ip[4] = {0};
                memcpy(phone_ip, (uint8_t *) pdata, 4);
                ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
            }
            //发送sc结束事件
            xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
            break;
        default:
            break;
    }
}

/*smartconfig任务
* @param[in]   void  		       :wu
* @retval      void                 :无
* @note        修改日志
*               Ver0.0.1:
                    hx-zsj, 2018/08/10, 初始化版本\n
*/
void smart_config_task(void *parm) {
    EventBits_t uxBits;
    //使用ESP-TOUCH配置
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    //开始sc
    ESP_ERROR_CHECK(esp_smartconfig_start(sc_callback));
    while (1) {
        //死等事件组：CONNECTED_BIT | ESPTOUCH_DONE_BIT
        uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);

        //sc结束
        if (uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smart config over");
            esp_smartconfig_stop();

            //xTaskCreate(http_get_task, "http_get_task", 4096, NULL, 3, NULL);
            vTaskDelete(NULL);
        }
        //连上ap
        if (uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected!");
        }
    }
}

/*
* wifi事件
* @param[in]   event  		       :事件
* @retval      esp_err_t           :错误类型
* @note        修改日志
*               Ver0.0.1:
                    hx-zsj, 2018/08/10, 初始化版本\n
*/
static esp_err_t event_handler(void *ctx, system_event_t *event) {
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            if (Curtain.is_wifi_config) {
                ESP_LOGI(TAG1, "Wi-Fi has been configuration.");
                esp_wifi_connect();
            } else {
                ESP_LOGI(TAG1, "Start smart config task.");
                xTaskCreate(smart_config_task, "smart_config_task", 4096, NULL, 3, NULL);
            }
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG1, "got ip:%s\n", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            //sta链接成功，set事件组
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            network_state = NETWORK_CONNECTED;
            xTaskCreate(&tcp_client, "tcp_client", 4096, NULL, 5, NULL);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            network_state = NETWORK_DO_NOT_CONFIG;
            ESP_LOGI(TAG1, "SYSTEM_EVENT_STA_DISCONNECTED");
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

void wifi_service_start(void)
{
    tcpip_adapter_init();

    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    if (Curtain.is_wifi_config) {
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &Curtain.wifi_config));
        network_state = NETWORK_CONNECTTING;
    } else {
        network_state = NETWORK_FIRST_CONFIG;
        memset(&Curtain.curtain_timer, 0, sizeof(work_time_t) * 4);
        memset(&Curtain.server_address.ip, 0, IPv4_STRING_LENGTH);
        memcpy(&Curtain.server_address.ip, CONFIG_TCP_PERF_SERVER_IP, strlen(CONFIG_TCP_PERF_SERVER_IP));
        Curtain.server_address.port = CONFIG_TCP_PERF_SERVER_PORT;
    }

    ESP_ERROR_CHECK(esp_wifi_start());
}

void network_config_unit_test(void)
{
    //ESP_ERROR_CHECK(nvs_flash_init());
    tcpip_adapter_init();

    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    if (Curtain.is_wifi_config) {
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &Curtain.wifi_config));
        network_state = NETWORK_CONNECTTING;
    } else {
        network_state = NETWORK_FIRST_CONFIG;

        memset(&Curtain.curtain_timer, 0, sizeof(work_time_t) * 4);
        memset(&Curtain.server_address.ip, 0, IPv4_STRING_LENGTH);
        memcpy(&Curtain.server_address.ip, CONFIG_TCP_PERF_SERVER_IP, strlen(CONFIG_TCP_PERF_SERVER_IP));
        Curtain.server_address.port = CONFIG_TCP_PERF_SERVER_PORT;
    }

    ESP_ERROR_CHECK(esp_wifi_start());
}