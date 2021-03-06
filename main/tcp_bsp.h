//
// Created by root on 1/4/20.
//

#ifndef INTELLIGENT_CURTAIN_TCP_BSP_H
#define INTELLIGENT_CURTAIN_TCP_BSP_H

#ifdef __cplusplus
extern "C" {
#endif


#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE (!FALSE)
#endif

#define TCP_SERVER_CLIENT_OPTION FALSE              //esp32作为client
//#define TCP_SERVER_CLIENT_OPTION TRUE              //esp32作为server

//server
//AP热点模式的配置信息
#define SOFT_AP_SSID            "TCP-SERVER"     //账号
#define SOFT_AP_PAS             ""          //密码，可以为空
#define SOFT_AP_MAX_CONNECT     1                   //最多的连接点

//client
//STA模式配置信息,即要连上的路由器的账号密码
#define GATEWAY_SSID            "@PHICOMM_40"         //账号
#define GATEWAY_PAS             "zjyZJY160526"       //密码
//#define TCP_SERVER_ADDRESS      "8.129.164.195"     //作为client，要连接TCP服务器地址
//#define TCP_PORT                6001               //统一的端口号，包括TCP客户端或者服务端

// FreeRTOS event group to signal when we are connected to wifi
#define WIFI_CONNECTED_BIT BIT0
//extern EventGroupHandle_t tcp_event_group;
extern EventGroupHandle_t wifi_event_group;

extern int  g_total_data;
extern bool g_rxtx_need_restart;

//using esp as station
void wifi_init_sta();
//using esp as softap
void wifi_init_softap();

//create a tcp server socket. return ESP_OK:success ESP_FAIL:error
esp_err_t create_tcp_server(bool isCreatServer);
//create a tcp client socket. return ESP_OK:success ESP_FAIL:error
esp_err_t create_tcp_client();

// //send data task
// void send_data(void *pvParameters);
//receive data task
void recv_data(void *pvParameters);
void heart_beat(void);
//close all socket
void close_socket();

//get socket error code. return: error code
int get_socket_error_code(int socket);

//show socket error code. return: error code
int show_socket_error_reason(const char* str, int socket);

//check working socket
int check_working_socket();


#ifdef __cplusplus
}
#endif

#endif //INTELLIGENT_CURTAIN_TCP_BSP_H
