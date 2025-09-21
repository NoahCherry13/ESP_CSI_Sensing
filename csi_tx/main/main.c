#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "CSI_RECEIVER";
#define WIFI_CHANNEL 1

// !!! IMPORTANT: REPLACE THIS with the MAC address from your SENDER device !!!
static uint8_t sender_mac[] = {0x60,0x55,0xf9,0xe0,0x29,0x4c};

// Callback function for when ESP-NOW data is received
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len > 0) {
        ESP_LOGI(TAG, "ESP-NOW packet received, length: %d. Communication is OK.", len);
    }
}

// Callback function for when CSI data is available
void csi_rx_cb(void *ctx, wifi_csi_info_t *info) {
    if (!info || !info->buf) {
        // info->mac can be NULL in some cases, so check it
        return;
    }

    // --- NEW: Diagnostic Log ---
    // Print the source MAC of every packet that generates CSI data, BEFORE filtering.
    // This will help verify if the sender's MAC address is correct.
    ESP_LOGI(TAG, "CSI packet detected from MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             info->mac[0], info->mac[1], info->mac[2], info->mac[3], info->mac[4], info->mac[5]);


    // --- Filter by sender's MAC address ---
    // Compare the source MAC address of the packet with our known sender
    if (memcmp(info->mac, sender_mac, 6) != 0) {
        return; // Not from our sender, so ignore it
    }

    // A more prominent log to ensure it's not missed
    ESP_LOGW(TAG, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    ESP_LOGW(TAG, "!!! CSI DATA from correct sender!  !!!");
    ESP_LOGW(TAG, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

    // Print some basic info from the CSI packet
    ESP_LOGI(TAG, "RSSI: %d, Rate: %d, SIG_MODE: %s, MCS: %d, Channel BW: %d, Channel: %d",
             info->rx_ctrl.rssi,
             info->rx_ctrl.rate,
             info->rx_ctrl.sig_mode ? "HT (802.11n)" : "Legacy",
             info->rx_ctrl.mcs,
             info->rx_ctrl.cwb,
             info->rx_ctrl.channel);
    // Print the CSI data buffer (I/Q pairs)
    ESP_LOGI(TAG, "CSI Data:");
    printf("CSI_DATA,[%d", info->buf[0]);
    // CHANGE THIS LOOP to send all data points
    for (int i = 1; i < info->len; i++) {
        printf(",%d", info->buf[i]);
    }
    printf("]\n");
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

    // Print this device's MAC address
    uint8_t mac_addr[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac_addr);
    ESP_LOGI(TAG, "Receiver MAC Address: %02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    ESP_LOGI(TAG, ">>> Copy this MAC address into the csi_sender_main.c file <<<");


    // Set channel
    ESP_ERROR_CHECK(esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));

    // --- Enable Promiscuous Mode & Set Protocol ---
    ESP_LOGI(TAG, "Enabling Promiscuous Mode");
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_LOGI(TAG, "Setting Wi-Fi protocol to 802.11 B/G/N");
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));


    // --- CSI Configuration ---
    // 1. Enable CSI
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));

    // 2. Set CSI configuration
    wifi_csi_config_t csi_config = {
        .lltf_en = true,
        .htltf_en = true,
        .stbc_htltf2_en = true,
        .ltf_merge_en = true,
        .channel_filter_en = true,
        .manu_scale = false,
    };
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));

    // 3. Register the CSI callback
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL));
}

void app_main(void) {
    wifi_init();

    // --- ESP-NOW Initialization ---
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    ESP_LOGI(TAG, "Receiver initialized. Waiting for packets from configured sender...");
}


