#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "audio_pipeline.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "board.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_netif.h"

#define WIFI_SSID "INTERPHONE"         // Votre SSID Wi-Fi
#define WIFI_PASS "azert717"           // Votre mot de passe Wi-Fi
#define UDP_PORT 12345                 // Port UDP pour le serveur
#define BUFFER_SIZE 1024               // Taille du buffer pour les données UDP

static const char *TAG = "UDP_AUDIO_SERVER";
static EventGroupHandle_t wifi_event_group; // Groupe d'événements pour le Wi-Fi
#define WIFI_CONNECTED_BIT BIT0     // Bit indiquant que le Wi-Fi est connecté

// Gestionnaire d'événements Wi-Fi
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Déconnexion Wi-Fi, tentative de reconnexion...");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Adresse IP obtenue: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Fonction pour initialiser le Wi-Fi
static void wifi_init(void) {
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connexion Wi-Fi en cours...");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Wi-Fi connecté avec succès");
}

// Fonction pour traiter les données UDP et les envoyer au pipeline audio
static void udp_server_task(void *pvParameters) {
    audio_pipeline_handle_t pipeline = (audio_pipeline_handle_t)pvParameters;
    audio_element_handle_t raw_write;

    raw_write = audio_pipeline_get_el_by_tag(pipeline, "raw");

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Erreur création socket: %d", errno);
        vTaskDelete(NULL);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(UDP_PORT);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Erreur bind socket: %d", errno);
        close(sock);
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "Serveur UDP démarré sur le port %d", UDP_PORT);

    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (1) {
        int len = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (len > 0) {
            ESP_LOGI(TAG, "Reçu %d octets depuis %s:%d", len, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            audio_element_output(raw_write, buffer, len);
        } else {
            ESP_LOGE(TAG, "Erreur réception: %d", errno);
        }
    }

    close(sock);
    vTaskDelete(NULL);
}

// Fonction principale
void app_main(void) {
    ESP_LOGI(TAG, "[1] Initialisation des périphériques");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[2] Initialisation du codec audio");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, 80);

    ESP_LOGI(TAG, "[3] Création du pipeline audio");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[4] Configuration du flux I2S pour la sortie audio");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.i2s_config.sample_rate = 16000;
    i2s_cfg.i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    audio_element_handle_t i2s_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[5] Configuration du flux brut pour recevoir les données UDP");
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t raw_stream = raw_stream_init(&raw_cfg);
    audio_element_set_tag(raw_stream, "raw");

    ESP_LOGI(TAG, "[6] Liaison des éléments dans le pipeline");
    audio_pipeline_register(pipeline, i2s_writer, "i2s");
    audio_pipeline_register(pipeline, raw_stream, "raw");
    const char *link_tag[2] = {"raw", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);

    ESP_LOGI(TAG, "[7] Démarrage du pipeline audio");
    audio_pipeline_run(pipeline);

    ESP_LOGI(TAG, "[8] Initialisation du Wi-Fi");
    wifi_init();

    ESP_LOGI(TAG, "[9] Lancement de la tâche serveur UDP");
    xTaskCreate(udp_server_task, "udp_server_task", 4096, pipeline, 5, NULL);
}