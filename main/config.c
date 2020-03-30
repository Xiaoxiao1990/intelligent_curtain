/* Non-Volatile Storage (NVS) Read and Write a Blob - Example

   For other examples please check:
   https://github.com/espressif/esp-idf/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <esp_wifi_types.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "config.h"

#define STORAGE_NAMESPACE "storage"
#define PARAMS_KEY        "Curtain"
static const char *TAG = "[CONFIG]";

Curtain_TypeDef Curtain;

esp_err_t params_save(void)
{
    nvs_handle my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    // Read the size of memory space required for blob
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, PARAMS_KEY, NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

    // Write value including previously saved blob if available
    err = nvs_set_blob(my_handle, PARAMS_KEY, &Curtain, required_size == 0 ? sizeof(Curtain_TypeDef) : required_size);

    if (err != ESP_OK) return err;

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

/* Read from NVS and print restart counter
   and the table with run times.
   Return an error if anything goes wrong
   during this process.
 */
esp_err_t params_init(void)
{
    nvs_handle my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    // Read run time blob
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    // obtain required memory space to store blob being read from NVS
    err = nvs_get_blob(my_handle, PARAMS_KEY, NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    printf("Params list:\n");
    if (required_size == 0) {
        ESP_LOGE(TAG, "Nothing saved yet!");
    } else {
        err = nvs_get_blob(my_handle, PARAMS_KEY, &Curtain, &required_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Read params list failed.");
            return err;
        }

        int *t1 = Curtain.device_params.optical_work_time, *t2 = Curtain.device_params.curtain_work_time;

        ESP_LOGI(TAG, "Wi-Fi config: %s\n"
                        "Wi-Fi STA SSID: %s\n"
                        "Wi-Fi STA PASS: %s\n"
                        "Wi-Fi AP SSID: %s\n"
                        "Wi-Fi AP PASS: %s\n"
                        "device params:{\n"
                        "\tbattery\": %d,\n"
                        "\toptical_sensor_status\": %d,\n"
                        "\tlumen\": %d,\n"
                        "\tcurtain_position: %d,\n"
                        "\tserver_ip: %s,\n"
                        "\tserver_port: %d,\n"
                        "\toptical_work_time: [%d, %d, %d, %d, %d, %d],\n"
                        "\tcurtain_work_time: [%d, %d, %d, %d, %d, %d],\n"
                        "\tcurtain_repeater: 0x%0X"
                        "\n}",
                 Curtain.is_wifi_config ? "true" : "false",
                 Curtain.wifi_config.sta.ssid,
                 Curtain.wifi_config.sta.password,
                 Curtain.wifi_config.ap.ssid,
                 Curtain.wifi_config.ap.password,
                 Curtain.device_params.battery,
                 Curtain.device_params.optical_sensor_status,
                 Curtain.device_params.lumen,
                 Curtain.device_params.curtain_position,
                 Curtain.device_params.server_address.ip,
                 Curtain.device_params.server_address.port,
                 t1[5], t1[4], t1[3], t1[2], t1[1], t1[0],
                 t2[5], t2[4], t2[3], t2[2], t2[1], t2[0],
                 Curtain.device_params.curtain_repeater
        );
    }

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

void config_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_LOGI(TAG, "MARK");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = params_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) reading data from NVS!\n", esp_err_to_name(err));
    }
}

void config_unit_test(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_LOGI(TAG, "MARK");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = params_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) reading data from NVS!\n", esp_err_to_name(err));
    }
}