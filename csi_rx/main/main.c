#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "CSI_SENDER";
#define WIFI_CHANNEL 1

// !!! IMPORTANT: REPLACE THIS with the MAC address from your receiver device !!!
static uint8_t receiver_mac[] = {0x60,0x55,0xf9,0xdf,0xfa,0xde};

// Callback function for when ESP-NOW data has been sent
void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        // Log sparingly on success to avoid clutter
        // ESP_LOGI(TAG, "Packet sent successfully");
    } else {
        ESP_LOGW(TAG, "Packet send failed");
    }
}

static void wifi_init(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Basic Wi-Fi setup
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));

    // --- NEW: Set ESP-NOW Rate to an 802.11n MCS value ---
    // This is the correct way to force a rate that generates CSI data.
    ESP_ERROR_CHECK(esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_MCS7_SGI));


    // Print this device's MAC address
    uint8_t mac_addr[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac_addr);
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "Sender MAC Address: %02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    ESP_LOGI(TAG, ">>> This is the address to put in the receiver's code <<<");
    ESP_LOGI(TAG, "==================================================");
}

static void sender_task(void *pvParameter) {
    // --- Configure Peer ---
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, receiver_mac, ESP_NOW_ETH_ALEN);
    peer_info.ifidx = WIFI_IF_STA;
    peer_info.channel = WIFI_CHANNEL;
    peer_info.encrypt = false;
    // The rate is now set globally in wifi_init(), not per-peer.

    if (!esp_now_is_peer_exist(receiver_mac)) {
        ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));
        ESP_LOGI(TAG, "Peer added");
    } else {
        ESP_LOGI(TAG, "Peer already exists");
    }

    uint8_t data_to_send[] = "Hello CSI!";

    while (1) {
        esp_err_t result = esp_now_send(receiver_mac, data_to_send, sizeof(data_to_send));
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Error sending data: %s", esp_err_to_name(result));
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}


void app_main(void) {
    wifi_init();
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    xTaskCreate(sender_task, "sender_task", 2048, NULL, 5, NULL);
}

