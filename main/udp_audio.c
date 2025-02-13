#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/i2s.h"
#include "esp_netif.h"

#define SSID "ESP32-CAM Access Point"
#define PASS "123456789"
#define UDP_PORT 12345
#define BUFFER_SIZE 1024

static const char *TAG = "UDP_SERVER";
static int udp_socket;

// Configuration I2S
void configure_i2s(void) {
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1
    };
    
    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = 5,
        .ws_io_num = 25,
        .data_out_num = 26,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    
    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM_0, &pin_config));
}

void wifi_init_sta(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PASS,
            .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Connecting to WiFi SSID:%s", SSID);
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void udp_server_task(void *pvParameters) {
    char rx_buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    socklen_t socklen = sizeof(client_addr);
    
    server_addr.sin_addr.s_addr = inet_addr("192.168.10.1");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);

    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (udp_socket < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int err = bind(udp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto CLEAN_UP;
    }

    ESP_LOGI(TAG, "UDP server running on IP: %s and port %d", "192.168.10.1", UDP_PORT);

    while (1) {
        int len = recvfrom(udp_socket, rx_buffer, sizeof(rx_buffer) - 1, 0, 
                          (struct sockaddr *)&client_addr, &socklen);
        
        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            continue;
        }

        rx_buffer[len] = 0;  // Null-terminate string
        ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);

        // Echo back to client
        err = sendto(udp_socket, rx_buffer, len, 0, 
                    (struct sockaddr *)&client_addr, sizeof(client_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Error sending response: errno %d", errno);
        }
    }

CLEAN_UP:
    close(udp_socket);
    vTaskDelete(NULL);
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    configure_i2s();
    wifi_init_sta();
    
    // Wait for WiFi to connect before starting the UDP server
    vTaskDelay(pdMS_TO_TICKS(5000)); // Delay for connection stability
    
    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
}