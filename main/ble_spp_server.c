/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#include <esp_gatts_api.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "driver/uart.h"
#include "string.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "ble_spp_server.h"
#include "config.h"

#define GATTS_TABLE_TAG  "BLE_BLE"

#define SPP_PROFILE_NUM             1
#define SPP_PROFILE_APP_IDX         0
#define ESP_SPP_APP_ID              0x56
#define SAMPLE_DEVICE_NAME          "Curtain"
#define SPP_SVC_INST_ID	            0

/// SPP Service
static const uint16_t spp_service_uuid = 0xABF0;
/// Characteristic UUID
#define ESP_GATT_UUID_SPP_DATA_RECEIVE      0xABF1
#define ESP_GATT_UUID_SPP_DATA_NOTIFY       0xABF2
//#define ESP_GATT_UUID_SPP_COMMAND_RECEIVE   0xABF3
//#define ESP_GATT_UUID_SPP_COMMAND_NOTIFY    0xABF4

#ifdef SUPPORT_HEARTBEAT
#define ESP_GATT_UUID_SPP_HEARTBEAT         0xABF5
#endif

//static const uint8_t spp_adv_data[23] = {
//    0x02,0x01,0x06,
//    0x03,0x03,0xF0,0xAB,
//    0x0F,0x09,0x45,0x53,0x50,0x5f,0x53,0x50,0x50,0x5f,0x53,0x45,0x52,0x56,0x45,0x52
//};

//static const uint8_t spp_adv_data[] = {
//        0x02,0x01,0x06,
//        0x03,0x03,0xF0,0xAB,
//        0x0F,0x09,0x45,0x53,0x50,0x5f,0x53,0x50,0x50,0x5f,0x53,0x45,0x52,0x56,0x45,0x52
//};

#define TEST_MANUFACTURER_DATA_LEN  17

static uint8_t adv_config_done = 0;
#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

#ifdef CONFIG_SET_RAW_ADV_DATA
static uint8_t raw_adv_data[] = {
        0x02, 0x01, 0x06,
        0x02, 0x0a, 0xeb, 0x03, 0x03, 0xab, 0xcd
};
static uint8_t raw_scan_rsp_data[] = {
        0x0f, 0x09, 0x45, 0x53, 0x50, 0x5f, 0x47, 0x41, 0x54, 0x54, 0x53, 0x5f, 0x44,
        0x45, 0x4d, 0x4f
};
#else

static uint8_t adv_service_uuid128[16] = {
        /* LSB <--------------------------------------------------------------------------------> MSB */
        //first uuid, 16bit, [12],[13] is the value
        0xaf, 0xdb, 0xcd, 0x35, 0x1f, 0xf6, 0xba, 0xf1, 0xa3, 0x14, 0xb4, 0xcf, 0x01, 0x00, 0x40, 0x8f
};

// The length of adv data must be less than 31 bytes
static uint8_t test_manufacturer[TEST_MANUFACTURER_DATA_LEN] =  {'L', 'Y', 'K', 'J', '-', 't', 'e', 'c', 'h', '.'};
//adv data
static esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = true,
        .include_txpower = true,
        .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
        .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
        .appearance = 0x00,
        .manufacturer_len = TEST_MANUFACTURER_DATA_LEN,
        .p_manufacturer_data = test_manufacturer,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = sizeof(adv_service_uuid128),
        .p_service_uuid = adv_service_uuid128,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};
// scan response data
static esp_ble_adv_data_t scan_rsp_data = {
        .set_scan_rsp = true,
        .include_name = true,
        .include_txpower = true,
        .min_interval = 0x0006,
        .max_interval = 0x0010,
        .appearance = 0x00,
        .manufacturer_len = TEST_MANUFACTURER_DATA_LEN,
        .p_manufacturer_data = test_manufacturer,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = sizeof(adv_service_uuid128),
        .p_service_uuid = adv_service_uuid128,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

#endif /* CONFIG_SET_RAW_ADV_DATA */

static esp_ble_adv_params_t adv_params = {
        .adv_int_min        = 0x20,
        .adv_int_max        = 0x40,
        .adv_type           = ADV_TYPE_IND,
        .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
        //.peer_addr            =
        //.peer_addr_type       =
        .channel_map        = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static uint16_t spp_mtu_size = 23;
static uint16_t spp_conn_id = 0xffff;
static esp_gatt_if_t spp_gatts_if = 0xff;
//QueueHandle_t spp_uart_queue = NULL;
//static xQueueHandle cmd_cmd_queue = NULL;

#ifdef SUPPORT_HEARTBEAT
static xQueueHandle cmd_heartbeat_queue = NULL;
static uint8_t  heartbeat_s[9] = {'E','s','p','r','e','s','s','i','f'};
static bool enable_heart_ntf = false;
static uint8_t heartbeat_count_num = 0;
#endif

static bool enable_data_ntf = false;
static bool is_connected = false;
static esp_bd_addr_t spp_remote_bda = {0x0,};

static uint16_t spp_handle_table[SPP_IDX_NB];

static esp_ble_adv_params_t spp_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

typedef struct spp_receive_data_node{
    int32_t len;
    uint8_t * node_buff;
    struct spp_receive_data_node * next_node;
}spp_receive_data_node_t;

static spp_receive_data_node_t * temp_spp_recv_data_node_p1 = NULL;
static spp_receive_data_node_t * temp_spp_recv_data_node_p2 = NULL;

typedef struct spp_receive_data_buff{
    int32_t node_num;
    int32_t buff_size;
    spp_receive_data_node_t * first_node;
}spp_receive_data_buff_t;

static spp_receive_data_buff_t SppRecvDataBuff = {
    .node_num   = 0,
    .buff_size  = 0,
    .first_node = NULL
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst spp_profile_tab[SPP_PROFILE_NUM] = {
    [SPP_PROFILE_APP_IDX] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

/*
 *  SPP PROFILE ATTRIBUTES
 ****************************************************************************************
 */

#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

static const uint8_t char_prop_read_notify = ESP_GATT_CHAR_PROP_BIT_READ|ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_WRITE_NR|ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
static const uint8_t char_prop_notify = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
#ifdef SUPPORT_HEARTBEAT
static const uint8_t char_prop_read_write_notify = ESP_GATT_CHAR_PROP_BIT_READ|ESP_GATT_CHAR_PROP_BIT_WRITE_NR|ESP_GATT_CHAR_PROP_BIT_NOTIFY;
#endif

///SPP Service - data receive characteristic, read&write without response
static const uint16_t spp_data_receive_uuid = ESP_GATT_UUID_SPP_DATA_RECEIVE;
static const uint8_t  spp_data_receive_val[20] = {0x00};

///SPP Service - data notify characteristic, notify&read
static const uint16_t spp_data_notify_uuid = ESP_GATT_UUID_SPP_DATA_NOTIFY;
static const uint8_t  spp_data_notify_val[20] = {0x00};
static const uint8_t  spp_data_notify_ccc[2] = {0x00, 0x00};

/////SPP Service - command characteristic, read&write without response
//static const uint16_t spp_command_uuid = ESP_GATT_UUID_SPP_COMMAND_RECEIVE;
//static const uint8_t  spp_command_val[10] = {0x00};
//
/////SPP Service - status characteristic, notify&read
//static const uint16_t spp_status_uuid = ESP_GATT_UUID_SPP_COMMAND_NOTIFY;
//static const uint8_t  spp_status_val[10] = {0x00};
//static const uint8_t  spp_status_ccc[2] = {0x00, 0x00};

#ifdef SUPPORT_HEARTBEAT
///SPP Server - Heart beat characteristic, notify&write&read
static const uint16_t spp_heart_beat_uuid = ESP_GATT_UUID_SPP_HEARTBEAT;
static const uint8_t  spp_heart_beat_val[2] = {0x00, 0x00};
static const uint8_t  spp_heart_beat_ccc[2] = {0x00, 0x00};
#endif

///Full HRS Database Description - Used to add attributes into the database
static const esp_gatts_attr_db_t spp_gatt_db[SPP_IDX_NB] =
{
    //SPP -  Service Declaration
    [SPP_IDX_SVC]                      	=
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
    sizeof(spp_service_uuid), sizeof(spp_service_uuid), (uint8_t *)&spp_service_uuid}},

    //SPP -  data receive characteristic Declaration
    [SPP_IDX_SPP_DATA_RECV_CHAR]            =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
    CHAR_DECLARATION_SIZE,CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}},

    //SPP -  data receive characteristic Value
    [SPP_IDX_SPP_DATA_RECV_VAL]             	=
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&spp_data_receive_uuid, ESP_GATT_PERM_WRITE,
    SPP_DATA_MAX_LEN,sizeof(spp_data_receive_val), (uint8_t *)spp_data_receive_val}},

    //SPP -  data notify characteristic Declaration
    [SPP_IDX_SPP_DATA_NOTIFY_CHAR]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
    CHAR_DECLARATION_SIZE,CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_notify}},

    //SPP -  data notify characteristic Value
    [SPP_IDX_SPP_DATA_NTY_VAL]   =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&spp_data_notify_uuid, ESP_GATT_PERM_READ,
    SPP_DATA_MAX_LEN, sizeof(spp_data_notify_val), (uint8_t *)spp_data_notify_val}},

    //SPP -  data notify characteristic - Client Characteristic Configuration Descriptor
    [SPP_IDX_SPP_DATA_NTF_CFG]         =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
    sizeof(uint16_t),sizeof(spp_data_notify_ccc), (uint8_t *)spp_data_notify_ccc}},

//    //SPP -  command characteristic Declaration
//    [SPP_IDX_SPP_COMMAND_CHAR]            =
//    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
//    CHAR_DECLARATION_SIZE,CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},
//
//    //SPP -  command characteristic Value
//    [SPP_IDX_SPP_COMMAND_VAL]                 =
//    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&spp_command_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
//    SPP_CMD_MAX_LEN,sizeof(spp_command_val), (uint8_t *)spp_command_val}},
//
//    //SPP -  status characteristic Declaration
//    [SPP_IDX_SPP_STATUS_CHAR]            =
//    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
//    CHAR_DECLARATION_SIZE,CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_notify}},
//
//    //SPP -  status characteristic Value
//    [SPP_IDX_SPP_STATUS_VAL]                 =
//    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&spp_status_uuid, ESP_GATT_PERM_READ,
//    SPP_STATUS_MAX_LEN,sizeof(spp_status_val), (uint8_t *)spp_status_val}},
//
//    //SPP -  status characteristic - Client Characteristic Configuration Descriptor
//    [SPP_IDX_SPP_STATUS_CFG]         =
//    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
//    sizeof(uint16_t),sizeof(spp_status_ccc), (uint8_t *)spp_status_ccc}},

#ifdef SUPPORT_HEARTBEAT
    //SPP -  Heart beat characteristic Declaration
    [SPP_IDX_SPP_HEARTBEAT_CHAR]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
    CHAR_DECLARATION_SIZE,CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write_notify}},

    //SPP -  Heart beat characteristic Value
    [SPP_IDX_SPP_HEARTBEAT_VAL]   =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&spp_heart_beat_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
    sizeof(spp_heart_beat_val), sizeof(spp_heart_beat_val), (uint8_t *)spp_heart_beat_val}},

    //SPP -  Heart beat characteristic - Client Characteristic Configuration Descriptor
    [SPP_IDX_SPP_HEARTBEAT_CFG]         =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
    sizeof(uint16_t),sizeof(spp_data_notify_ccc), (uint8_t *)spp_heart_beat_ccc}},
#endif
};

static uint8_t find_char_and_desr_index(uint16_t handle)
{
    uint8_t error = 0xff;

    for(uint8_t i = 0; i < SPP_IDX_NB; i++){
        if(handle == spp_handle_table[i]){
            return i;
        }
    }

    return error;
}

static bool store_wr_buffer(esp_ble_gatts_cb_param_t *p_data)
{
    temp_spp_recv_data_node_p1 = (spp_receive_data_node_t *)malloc(sizeof(spp_receive_data_node_t));

    if(temp_spp_recv_data_node_p1 == NULL){
        ESP_LOGI(GATTS_TABLE_TAG, "malloc error %s %d\n", __func__, __LINE__);
        return false;
    }
    if(temp_spp_recv_data_node_p2 != NULL){
        temp_spp_recv_data_node_p2->next_node = temp_spp_recv_data_node_p1;
    }
    temp_spp_recv_data_node_p1->len = p_data->write.len;
    SppRecvDataBuff.buff_size += p_data->write.len;
    temp_spp_recv_data_node_p1->next_node = NULL;
    temp_spp_recv_data_node_p1->node_buff = (uint8_t *)malloc(p_data->write.len);
    temp_spp_recv_data_node_p2 = temp_spp_recv_data_node_p1;
    memcpy(temp_spp_recv_data_node_p1->node_buff,p_data->write.value,p_data->write.len);
    if(SppRecvDataBuff.node_num == 0){
        SppRecvDataBuff.first_node = temp_spp_recv_data_node_p1;
        SppRecvDataBuff.node_num++;
    }else{
        SppRecvDataBuff.node_num++;
    }

    return true;
}

static void free_write_buffer(void)
{
    temp_spp_recv_data_node_p1 = SppRecvDataBuff.first_node;

    while(temp_spp_recv_data_node_p1 != NULL){
        temp_spp_recv_data_node_p2 = temp_spp_recv_data_node_p1->next_node;
        free(temp_spp_recv_data_node_p1->node_buff);
        free(temp_spp_recv_data_node_p1);
        temp_spp_recv_data_node_p1 = temp_spp_recv_data_node_p2;
    }

    SppRecvDataBuff.node_num = 0;
    SppRecvDataBuff.buff_size = 0;
    SppRecvDataBuff.first_node = NULL;
}

static void print_write_buffer(void)
{
    temp_spp_recv_data_node_p1 = SppRecvDataBuff.first_node;

    while(temp_spp_recv_data_node_p1 != NULL){
        uart_write_bytes(UART_NUM_0, (char *)(temp_spp_recv_data_node_p1->node_buff), temp_spp_recv_data_node_p1->len);
        temp_spp_recv_data_node_p1 = temp_spp_recv_data_node_p1->next_node;
    }
}

//void uart_task(void *pvParameters)
//{
//    uart_event_t event;
//    uint8_t total_num = 0;
//    uint8_t current_num = 0;
//
//    for (;;) {
//        //Waiting for UART event.
//        if (xQueueReceive(spp_uart_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
//            switch (event.type) {
//            //Event of UART receving data
//            case UART_DATA:
//                if ((event.size)&&(is_connected)) {
//                    uint8_t * temp = NULL;
//                    uint8_t * ntf_value_p = NULL;
//#ifdef SUPPORT_HEARTBEAT
//                    if(!enable_heart_ntf){
//                        ESP_LOGE(GATTS_TABLE_TAG, "%s do not enable heartbeat Notify\n", __func__);
//                        break;
//                    }
//#endif
//                    if(!enable_data_ntf){
//                        ESP_LOGE(GATTS_TABLE_TAG, "%s do not enable data Notify\n", __func__);
//                        break;
//                    }
//                    temp = (uint8_t *)malloc(sizeof(uint8_t)*event.size);
//                    if(temp == NULL){
//                        ESP_LOGE(GATTS_TABLE_TAG, "%s malloc.1 failed\n", __func__);
//                        break;
//                    }
//                    memset(temp,0x0,event.size);
//                    uart_read_bytes(UART_NUM_0,temp,event.size,portMAX_DELAY);
//                    if(event.size <= (spp_mtu_size - 3)){
//                        esp_ble_gatts_send_indicate(spp_gatts_if, spp_conn_id, spp_handle_table[SPP_IDX_SPP_DATA_NTY_VAL],event.size, temp, false);
//                    }else if(event.size > (spp_mtu_size - 3)){
//                        if((event.size%(spp_mtu_size - 7)) == 0){
//                            total_num = event.size/(spp_mtu_size - 7);
//                        }else{
//                            total_num = event.size/(spp_mtu_size - 7) + 1;
//                        }
//                        current_num = 1;
//                        ntf_value_p = (uint8_t *)malloc((spp_mtu_size-3)*sizeof(uint8_t));
//                        if(ntf_value_p == NULL){
//                            ESP_LOGE(GATTS_TABLE_TAG, "%s malloc.2 failed\n", __func__);
//                            free(temp);
//                            break;
//                        }
//                        while(current_num <= total_num){
//                            if(current_num < total_num){
//                                ntf_value_p[0] = '#';
//                                ntf_value_p[1] = '#';
//                                ntf_value_p[2] = total_num;
//                                ntf_value_p[3] = current_num;
//                                memcpy(ntf_value_p + 4,temp + (current_num - 1)*(spp_mtu_size-7),(spp_mtu_size-7));
//                                esp_ble_gatts_send_indicate(spp_gatts_if, spp_conn_id, spp_handle_table[SPP_IDX_SPP_DATA_NTY_VAL],(spp_mtu_size-3), ntf_value_p, false);
//                            }else if(current_num == total_num){
//                                ntf_value_p[0] = '#';
//                                ntf_value_p[1] = '#';
//                                ntf_value_p[2] = total_num;
//                                ntf_value_p[3] = current_num;
//                                memcpy(ntf_value_p + 4,temp + (current_num - 1)*(spp_mtu_size-7),(event.size - (current_num - 1)*(spp_mtu_size - 7)));
//                                esp_ble_gatts_send_indicate(spp_gatts_if, spp_conn_id, spp_handle_table[SPP_IDX_SPP_DATA_NTY_VAL],(event.size - (current_num - 1)*(spp_mtu_size - 7) + 4), ntf_value_p, false);
//                            }
//                            vTaskDelay(20 / portTICK_PERIOD_MS);
//                            current_num++;
//                        }
//                        free(ntf_value_p);
//                    }
//                    free(temp);
//                }
//                break;
//            default:
//                break;
//            }
//        }
//    }
//    vTaskDelete(NULL);
//}

//static void spp_uart_init(void)
//{
//    uart_config_t uart_config = {
//        .baud_rate = 115200,
//        .data_bits = UART_DATA_8_BITS,
//        .parity = UART_PARITY_DISABLE,
//        .stop_bits = UART_STOP_BITS_1,
//        .flow_ctrl = UART_HW_FLOWCTRL_RTS,
//        .rx_flow_ctrl_thresh = 122,
//    };
//
//    //Set UART parameters
//    uart_param_config(UART_NUM_0, &uart_config);
//    //Set UART pins
//    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
//    //Install UART driver, and get the queue.
//    uart_driver_install(UART_NUM_0, 4096, 8192, 10,&spp_uart_queue,0);
//    xTaskCreate(uart_task, "uTask", 2048, (void*)UART_NUM_0, 8, NULL);
//}

//#ifdef SUPPORT_HEARTBEAT
//void spp_heartbeat_task(void * arg)
//{
//    uint16_t cmd_id;
//
//    for(;;) {
//        vTaskDelay(50 / portTICK_PERIOD_MS);
//        if(xQueueReceive(cmd_heartbeat_queue, &cmd_id, portMAX_DELAY)) {
//            while(1){
//                heartbeat_count_num++;
//                vTaskDelay(5000/ portTICK_PERIOD_MS);
//                if((heartbeat_count_num >3)&&(is_connected)){
//                    esp_ble_gap_disconnect(spp_remote_bda);
//                }
//                if(is_connected && enable_heart_ntf){
//                    esp_ble_gatts_send_indicate(spp_gatts_if, spp_conn_id, spp_handle_table[SPP_IDX_SPP_HEARTBEAT_VAL],sizeof(heartbeat_s), heartbeat_s, false);
//                }else if(!is_connected){
//                    break;
//                }
//            }
//        }
//    }
//    vTaskDelete(NULL);
//}
//#endif

//void spp_cmd_task(void * arg)
//{
//    uint8_t * cmd_id;
//
//    for(;;){
//        vTaskDelay(50 / portTICK_PERIOD_MS);
//        if(xQueueReceive(cmd_cmd_queue, &cmd_id, portMAX_DELAY)) {
//            esp_log_buffer_char(GATTS_TABLE_TAG,(char *)(cmd_id),strlen((char *)cmd_id));
//            free(cmd_id);
//        }
//    }
//    vTaskDelete(NULL);
//}

//static void spp_task_init(void)
//{
//    spp_uart_init();
//
//#ifdef SUPPORT_HEARTBEAT
//    cmd_heartbeat_queue = xQueueCreate(10, sizeof(uint32_t));
//    xTaskCreate(spp_heartbeat_task, "spp_heartbeat_task", 2048, NULL, 10, NULL);
//#endif
//
//    cmd_cmd_queue = xQueueCreate(10, sizeof(uint32_t));
//    xTaskCreate(spp_cmd_task, "spp_cmd_task", 2048, NULL, 10, NULL);
//}

/*
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;
    ESP_LOGI(GATTS_TABLE_TAG, "GAP_EVT, event %d\n", event);

    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&spp_adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //advertising start complete event to indicate advertising start successfully or failed
        if((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(GATTS_TABLE_TAG, "Advertising start failed: %s\n", esp_err_to_name(err));
        }
        break;
    default:
        break;
    }
}
*/

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
#ifdef CONFIG_SET_RAW_ADV_DATA
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done==0){
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        adv_config_done &= (~scan_rsp_config_flag);
        if (adv_config_done==0){
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
#else
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~adv_config_flag);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~scan_rsp_config_flag);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
#endif
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            //advertising start complete event to indicate advertising start successfully or failed
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TABLE_TAG, "Advertising start failed\n");
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TABLE_TAG, "Advertising stop failed\n");
            } else {
                ESP_LOGI(GATTS_TABLE_TAG, "Stop adv successfully\n");
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                     param->update_conn_params.status,
                     param->update_conn_params.min_int,
                     param->update_conn_params.max_int,
                     param->update_conn_params.conn_int,
                     param->update_conn_params.latency,
                     param->update_conn_params.timeout);
            break;
        default:
            break;
    }
}

void send(uint8_t *buf, uint16_t size) {
    static uint8_t total_num = 0;
    static uint8_t current_num = 0;
    if (size && is_connected) {
        uint8_t *ntf_value_p = NULL;
#ifdef SUPPORT_HEARTBEAT
        if(!enable_heart_ntf){
                        ESP_LOGE(GATTS_TABLE_TAG, "%s do not enable heartbeat Notify\n", __func__);
                        break;
                    }
#endif
        if (!enable_data_ntf) {
            ESP_LOGE(GATTS_TABLE_TAG, "%s do not enable data Notify\n", __func__);
            return;
        }

        if (size <= (spp_mtu_size - 3)) {
            esp_ble_gatts_send_indicate(spp_gatts_if, spp_conn_id, spp_handle_table[SPP_IDX_SPP_DATA_NTY_VAL], size, buf, false);
        } else {
            if ((size % (spp_mtu_size - 7)) == 0) {
                total_num = size / (spp_mtu_size - 7);
            } else {
                total_num = size / (spp_mtu_size - 7) + 1;
            }
            current_num = 1;
            ntf_value_p = (uint8_t *) malloc((spp_mtu_size - 3) * sizeof(uint8_t));
            if (ntf_value_p == NULL) {
                ESP_LOGE(GATTS_TABLE_TAG, "%s malloc.2 failed\n", __func__);
                return;
            }
            while (current_num <= total_num) {
                if (current_num < total_num) {
                    ntf_value_p[0] = '#';
                    ntf_value_p[1] = '#';
                    ntf_value_p[2] = total_num;
                    ntf_value_p[3] = current_num;
                    memcpy(ntf_value_p + 4, buf + (current_num - 1) * (spp_mtu_size - 7), (spp_mtu_size - 7));
                    esp_ble_gatts_send_indicate(spp_gatts_if, spp_conn_id, spp_handle_table[SPP_IDX_SPP_DATA_NTY_VAL],
                                                (spp_mtu_size - 3), ntf_value_p, false);
                } else {
                    ntf_value_p[0] = '#';
                    ntf_value_p[1] = '#';
                    ntf_value_p[2] = total_num;
                    ntf_value_p[3] = current_num;
                    memcpy(ntf_value_p + 4, buf + (current_num - 1) * (spp_mtu_size - 7),
                           (size - (current_num - 1) * (spp_mtu_size - 7)));
                    esp_ble_gatts_send_indicate(spp_gatts_if, spp_conn_id, spp_handle_table[SPP_IDX_SPP_DATA_NTY_VAL],
                                                (size - (current_num - 1) * (spp_mtu_size - 7) + 4), ntf_value_p,
                                                false);
                }
                vTaskDelay(20 / portTICK_PERIOD_MS);
                current_num++;
            }
            free(ntf_value_p);
        }
    }
}

typedef struct {
    uint8_t buf[23];
    uint16_t len;
} rsp_buffer_t;

static void uart_profile_parse(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param){
//    esp_gatt_status_t status = ESP_GATT_OK;
    rsp_buffer_t rsp_buffer;

    if (!param->write.is_prep) {
        ESP_LOGI(GATTS_TABLE_TAG, "GATT_WRITE_EVT, value len %d, value :", param->write.len);
                esp_log_buffer_hex(GATTS_TABLE_TAG, param->write.value, param->write.len);

        // Normal Setting Data
        uint8_t *data = param->write.value;
        if (data[0] != 0x55) {
            ESP_LOGE(GATTS_TABLE_TAG, "Illegal Header");
            rsp_buffer.buf[0] = 0x55;
            rsp_buffer.buf[1] = 0;
            rsp_buffer.buf[2] = 0xFF;
            rsp_buffer.len = 3;
        } else {
            switch (data[1]) {
                case 0x00:  // system command
                    if (data[2] == 0x01) {
                        ESP_LOGI(GATTS_TABLE_TAG, "Start adjust curtain position");
                        // TODO actions
                        // TODO done
                        memcpy(rsp_buffer.buf, param->write.value, param->write.len);
                        rsp_buffer.len = param->write.len;
                    } else if (data[2] == 0x02) {
                        ESP_LOGI(GATTS_TABLE_TAG, "Reboot device");
                        memcpy(rsp_buffer.buf, param->write.value, param->write.len);
                        rsp_buffer.len = param->write.len;
                        // TODO actions
                    } else if (data[2] == 0x03) {
                        ESP_LOGI(GATTS_TABLE_TAG, "Set Server IP & Port");
                        snprintf(Curtain.device_params.server_address.ip, 16, "%d.%d.%d.%d", data[3], data[4], data[5], data[6]);
                        Curtain.device_params.server_address.port = data[7];
                        Curtain.device_params.server_address.port <<= 8;
                        Curtain.device_params.server_address.port += data[8];
                        memcpy(rsp_buffer.buf, param->write.value, param->write.len);
                        rsp_buffer.len = param->write.len;
                        // TODO: save to nvs
                        params_save();
                    } else if (data[2] == 0x04) {
                        ESP_LOGI(GATTS_TABLE_TAG, "Read Server IP & Port");
                        int d1, d2, d3, d4;
                        sscanf(Curtain.device_params.server_address.ip, "%d.%d.%d.%d", &d1, &d2, &d3, &d4);
                        rsp_buffer.buf[0] = 0x55;
                        rsp_buffer.buf[1] = 0x00;
                        rsp_buffer.buf[2] = 0x04;
                        rsp_buffer.buf[3] = (uint8_t)d1;
                        rsp_buffer.buf[4] = (uint8_t)d2;
                        rsp_buffer.buf[5] = (uint8_t)d3;
                        rsp_buffer.buf[6] = (uint8_t)d4;
                        rsp_buffer.buf[7] = (uint8_t)(Curtain.device_params.server_address.port >> 8);
                        rsp_buffer.buf[8] = (uint8_t)(Curtain.device_params.server_address.port & 0xFF);

                        rsp_buffer.len = 9;
                    } else {
                        ESP_LOGE(GATTS_TABLE_TAG, "Undefined operation");
                        rsp_buffer.buf[0] = 0x55;
                        rsp_buffer.buf[1] = 0;
                        rsp_buffer.buf[2] = 0xFE;
                        rsp_buffer.len = 3;
                    }
                    break;
                case 0x01:  // read
                    if (data[2] == 0x01) {
                        ESP_LOGI(GATTS_TABLE_TAG, "Read device info");
                        // 55 01 01
                        memcpy(rsp_buffer.buf, param->write.value, param->write.len);
                        // id 6bytes
                        memcpy(&(rsp_buffer.buf[3]), &Curtain.device_id, 6) ;
                        // battery
                        rsp_buffer.buf[10] = Curtain.device_params.battery;
                        // battery temp
                        rsp_buffer.buf[11] = Curtain.device_params.bat_temp;
                        // battery status, 0:not charge, 1: charging, 2:charged
                        rsp_buffer.buf[12] = Curtain.device_params.bat_state;
                        // curtain position
                        rsp_buffer.buf[13] = Curtain.device_params.curtain_position;
                        // optical status
                        rsp_buffer.buf[14] = Curtain.device_params.optical_sensor_status;
                        // lumen
                        rsp_buffer.buf[15] = Curtain.device_params.lumen;
                        // work mode, 0:no mode, 1: summer, 2:winter
                        rsp_buffer.buf[16] = Curtain.device_params.work_mode;
                        // light gate value
                        rsp_buffer.buf[17] = Curtain.device_params.lumen_gate_value;
                        rsp_buffer.len = 18;
                    } else if (data[2] == 0x03) {
                        ESP_LOGI(GATTS_TABLE_TAG, "Read switcher time");
                        if (data[3] > 3) {
                            rsp_buffer.buf[0] = 0x55;
                            rsp_buffer.buf[1] = 0;
                            rsp_buffer.buf[2] = 0xFC;
                            rsp_buffer.len = 3;
                        } else {
                            memcpy(rsp_buffer.buf, param->write.value, 4);
                            memcpy(&rsp_buffer.buf[4], &Curtain.device_params.curtain_timer[data[3]], 5);
                            rsp_buffer.len = 9;
                        }
                    } else {
                        ESP_LOGE(GATTS_TABLE_TAG, "Undefined operation");
                        rsp_buffer.buf[0] = 0x55;
                        rsp_buffer.buf[1] = 0;
                        rsp_buffer.buf[2] = 0xFE;
                        rsp_buffer.len = 3;
                    }
                    break;
                case 0x02: // set
                    if (data[2] == 0x01) {
                        ESP_LOGI(GATTS_TABLE_TAG, "Set time");
                        // TODO:Set system time

                        // 55 02 01
                        memcpy(rsp_buffer.buf, param->write.value, 3);
                        rsp_buffer.buf[3] = 1;
                        rsp_buffer.len = 4;
                    } else if (data[2] == 0x02) {
                        ESP_LOGI(GATTS_TABLE_TAG, "Set light work mode");
                        Curtain.device_params.work_mode = data[3];
                        Curtain.device_params.lumen_gate_value = data[4];
                        memcpy(rsp_buffer.buf, param->write.value, 3);
                        rsp_buffer.buf[3] = Curtain.device_params.optical_sensor_status;
                        rsp_buffer.len = 4;
                    } else if (data[2] == 0x03) {
                        ESP_LOGI(GATTS_TABLE_TAG, "Set switcher time");
                        if (data[3] > 3 || param->write.len != 9) {
                            rsp_buffer.buf[0] = 0x55;
                            rsp_buffer.buf[1] = 0;
                            rsp_buffer.buf[2] = 0xFC;
                            rsp_buffer.len = 3;
                        } else {
                            memcpy(&Curtain.device_params.curtain_timer[data[3]], &data[4], 5);
                            memcpy(rsp_buffer.buf, param->write.value, param->write.len);
                            rsp_buffer.len = param->write.len;
                        }
                    } else if (data[2] == 0x04) {
                        ESP_LOGI(GATTS_TABLE_TAG, "Set curtain ratio");
                        // TODO: move curtain to target position
                        // TODO: done
                        memcpy(rsp_buffer.buf, param->write.value, param->write.len);
                        rsp_buffer.len = param->write.len;
                    } else if (data[2] == 0x05) {
                        ESP_LOGI(GATTS_TABLE_TAG, "Motor control");
                        // TODO: toggle start or stop motor
                        memcpy(rsp_buffer.buf, param->write.value, param->write.len);
                        rsp_buffer.len = param->write.len;
                    } else {
                        ESP_LOGE(GATTS_TABLE_TAG, "Undefined operation");
                        rsp_buffer.buf[0] = 0x55;
                        rsp_buffer.buf[1] = 0;
                        rsp_buffer.buf[2] = 0xFE;
                        rsp_buffer.len = 3;
                    }
                    break;
                default:{
                    rsp_buffer.buf[0] = 0x55;
                    rsp_buffer.buf[1] = 0;
                    rsp_buffer.buf[2] = 0xFD;
                    rsp_buffer.len = 3;
                } break;
            }
            esp_log_buffer_hex(GATTS_TABLE_TAG " RSP:", rsp_buffer.buf, rsp_buffer.len);
            send(rsp_buffer.buf, rsp_buffer.len);
        }
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    esp_ble_gatts_cb_param_t *p_data = (esp_ble_gatts_cb_param_t *) param;
    uint8_t res = 0xff;

    ESP_LOGI(GATTS_TABLE_TAG, "event = %x\n",event);
    switch (event) {
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
            esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(SAMPLE_DEVICE_NAME);
            if (set_dev_name_ret){
                ESP_LOGE(GATTS_TABLE_TAG, "set device name failed, error code = %x", set_dev_name_ret);
            }
#ifdef CONFIG_SET_RAW_ADV_DATA
        esp_err_t raw_adv_ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
        if (raw_adv_ret){
            ESP_LOGE(GATTS_TAG, "config raw adv data failed, error code = %x ", raw_adv_ret);
        }
        adv_config_done |= adv_config_flag;
        esp_err_t raw_scan_ret = esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
        if (raw_scan_ret){
            ESP_LOGE(GATTS_TAG, "config raw scan rsp data failed, error code = %x", raw_scan_ret);
        }
        adv_config_done |= scan_rsp_config_flag;
#else
            //config adv data
            esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
            if (ret){
                ESP_LOGE(GATTS_TABLE_TAG, "config adv data failed, error code = %x", ret);
            }
            adv_config_done |= adv_config_flag;
            //config scan response data
            ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
            if (ret){
                ESP_LOGE(GATTS_TABLE_TAG, "config scan response data failed, error code = %x", ret);
            }
            adv_config_done |= scan_rsp_config_flag;

#endif
            esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(spp_gatt_db, gatts_if, SPP_IDX_NB, SPP_SVC_INST_ID);
            if (create_attr_ret){
                ESP_LOGE(GATTS_TABLE_TAG, "create attr table failed, error code = %x", create_attr_ret);
            }


/*    	    ESP_LOGI(GATTS_TABLE_TAG, "%s %d\n", __func__, __LINE__);
        	esp_ble_gap_set_device_name(SAMPLE_DEVICE_NAME);

        	ESP_LOGI(GATTS_TABLE_TAG, "%s %d\n", __func__, __LINE__);
            esp_err_t raw_adv_ret = esp_ble_gap_config_adv_data_raw((uint8_t *)spp_adv_data, sizeof(spp_adv_data));
            if (raw_adv_ret){
                ESP_LOGE(GATTS_TABLE_TAG, "config raw adv data failed, error code = %x ", raw_adv_ret);
            }

        	ESP_LOGI(GATTS_TABLE_TAG, "%s %d\n", __func__, __LINE__);
        	esp_ble_gatts_create_attr_tab(spp_gatt_db, gatts_if, SPP_IDX_NB, SPP_SVC_INST_ID);*/
       	break;
    	case ESP_GATTS_READ_EVT:
            res = find_char_and_desr_index(p_data->read.handle);
//            if(res == SPP_IDX_SPP_STATUS_VAL){
//                //TODO:client read the status characteristic
//            }
       	 break;
    	case ESP_GATTS_WRITE_EVT: {
    	    res = find_char_and_desr_index(p_data->write.handle);
            if(p_data->write.is_prep == false){
                ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_WRITE_EVT : handle = %d\n", res);
//                if(res == SPP_IDX_SPP_COMMAND_VAL){
//                    uint8_t * spp_cmd_buff = NULL;
//                    spp_cmd_buff = (uint8_t *)malloc((spp_mtu_size - 3) * sizeof(uint8_t));
//                    if(spp_cmd_buff == NULL){
//                        ESP_LOGE(GATTS_TABLE_TAG, "%s malloc failed\n", __func__);
//                        break;
//                    }
//                    memset(spp_cmd_buff,0x0,(spp_mtu_size - 3));
//                    memcpy(spp_cmd_buff,p_data->write.value,p_data->write.len);
//                    xQueueSend(cmd_cmd_queue,&spp_cmd_buff,10/portTICK_PERIOD_MS);
//                }else
                    if(res == SPP_IDX_SPP_DATA_NTF_CFG){
                    if((p_data->write.len == 2)&&(p_data->write.value[0] == 0x01)&&(p_data->write.value[1] == 0x00)){
                        enable_data_ntf = true;
                        ESP_LOGI(GATTS_TABLE_TAG, "notify enable");
                    }else if((p_data->write.len == 2)&&(p_data->write.value[0] == 0x00)&&(p_data->write.value[1] == 0x00)){
                        enable_data_ntf = false;
                        ESP_LOGI(GATTS_TABLE_TAG, "notify disable");
                    }
                }
#ifdef SUPPORT_HEARTBEAT
                else if(res == SPP_IDX_SPP_HEARTBEAT_CFG){
                    if((p_data->write.len == 2)&&(p_data->write.value[0] == 0x01)&&(p_data->write.value[1] == 0x00)){
                        enable_heart_ntf = true;
                    }else if((p_data->write.len == 2)&&(p_data->write.value[0] == 0x00)&&(p_data->write.value[1] == 0x00)){
                        enable_heart_ntf = false;
                    }
                }else if(res == SPP_IDX_SPP_HEARTBEAT_VAL){
                    if((p_data->write.len == sizeof(heartbeat_s))&&(memcmp(heartbeat_s,p_data->write.value,sizeof(heartbeat_s)) == 0)){
                        heartbeat_count_num = 0;
                    }
                }
#endif
                else if(res == SPP_IDX_SPP_DATA_RECV_VAL){
#ifdef SPP_DEBUG_MODE
                    esp_log_buffer_char(GATTS_TABLE_TAG,(char *)(p_data->write.value),p_data->write.len);
#else
                    uart_profile_parse(gatts_if, param);
                    //uart_write_bytes(UART_NUM_0, (char *)(p_data->write.value), p_data->write.len);
#endif
                }else{
                    //TODO:
                }
            }else if((p_data->write.is_prep == true)&&(res == SPP_IDX_SPP_DATA_RECV_VAL)){
                ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_PREP_WRITE_EVT : handle = %d\n", res);
                store_wr_buffer(p_data);
            }
      	 	break;
    	}
    	case ESP_GATTS_EXEC_WRITE_EVT:{
    	    ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_EXEC_WRITE_EVT\n");
    	    if(p_data->exec_write.exec_write_flag){
    	        print_write_buffer();
    	        free_write_buffer();
    	    }
    	    break;
    	}
    	case ESP_GATTS_MTU_EVT:
    	    spp_mtu_size = p_data->mtu.mtu;
    	    break;
    	case ESP_GATTS_CONF_EVT:
    	    break;
    	case ESP_GATTS_UNREG_EVT:
        	break;
    	case ESP_GATTS_DELETE_EVT:
        	break;
    	case ESP_GATTS_START_EVT:
        	break;
    	case ESP_GATTS_STOP_EVT:
        	break;
    	case ESP_GATTS_CONNECT_EVT:
    	    spp_conn_id = p_data->connect.conn_id;
    	    spp_gatts_if = gatts_if;
    	    is_connected = true;
    	    memcpy(&spp_remote_bda,&p_data->connect.remote_bda,sizeof(esp_bd_addr_t));
#ifdef SUPPORT_HEARTBEAT
    	    uint16_t cmd = 0;
            xQueueSend(cmd_heartbeat_queue,&cmd,10/portTICK_PERIOD_MS);
#endif
        	break;
    	case ESP_GATTS_DISCONNECT_EVT:
    	    is_connected = false;
    	    enable_data_ntf = false;
#ifdef SUPPORT_HEARTBEAT
    	    enable_heart_ntf = false;
    	    heartbeat_count_num = 0;
#endif
    	    esp_ble_gap_start_advertising(&spp_adv_params);
    	    break;
    	case ESP_GATTS_OPEN_EVT:
    	    break;
    	case ESP_GATTS_CANCEL_OPEN_EVT:
    	    break;
    	case ESP_GATTS_CLOSE_EVT:
    	    break;
    	case ESP_GATTS_LISTEN_EVT:
    	    break;
    	case ESP_GATTS_CONGEST_EVT:
    	    break;
    	case ESP_GATTS_CREAT_ATTR_TAB_EVT:{
    	    ESP_LOGI(GATTS_TABLE_TAG, "The number handle =%x\n",param->add_attr_tab.num_handle);
    	    if (param->add_attr_tab.status != ESP_GATT_OK){
    	        ESP_LOGE(GATTS_TABLE_TAG, "Create attribute table failed, error code=0x%x", param->add_attr_tab.status);
    	    }
    	    else if (param->add_attr_tab.num_handle != SPP_IDX_NB){
    	        ESP_LOGE(GATTS_TABLE_TAG, "Create attribute table abnormally, num_handle (%d) doesn't equal to HRS_IDX_NB(%d)", param->add_attr_tab.num_handle, SPP_IDX_NB);
    	    }
    	    else {
    	        memcpy(spp_handle_table, param->add_attr_tab.handles, sizeof(spp_handle_table));
    	        esp_ble_gatts_start_service(spp_handle_table[SPP_IDX_SVC]);
    	    }
    	    break;
    	}
    	default:
    	    break;
    }
}


static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    ESP_LOGI(GATTS_TABLE_TAG, "EVT %d, gatts if %d\n", event, gatts_if);

    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            spp_profile_tab[SPP_PROFILE_APP_IDX].gatts_if = gatts_if;
        } else {
            ESP_LOGI(GATTS_TABLE_TAG, "Reg app failed, app_id %04x, status %d\n",param->reg.app_id, param->reg.status);
            return;
        }
    }

    do {
        int idx;
        for (idx = 0; idx < SPP_PROFILE_NUM; idx++) {
            if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gatts_if == spp_profile_tab[idx].gatts_if) {
                if (spp_profile_tab[idx].gatts_cb) {
                    spp_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

void ble_server_start(void)
{
    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(GATTS_TABLE_TAG, "%s init bluetooth\n", __func__);
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_app_register(ESP_SPP_APP_ID);
}
void ble_spp_server_unit_test(void)
{
    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(GATTS_TABLE_TAG, "%s init bluetooth\n", __func__);
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(GATTS_TABLE_TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_app_register(ESP_SPP_APP_ID);

    //spp_task_init();
    return;
}
