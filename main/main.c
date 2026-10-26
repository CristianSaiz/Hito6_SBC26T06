#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "app_config.h"
#include "wifi_sta.h"
#include "ota_manager.h"
#include "mqtt_tb.h"
#include "time_sync.h"

static const char *TAG = "MAIN_HITO6";

void app_main(void)
{
    ESP_LOGI(TAG, "=========================================================");
    ESP_LOGI(TAG, "SBC Hito 6 - Sistema OTA (Firmware: %s v%s)", FIRMWARE_TITLE, FIRMWARE_VERSION);
    ESP_LOGI(TAG, "=========================================================");

    // 1. Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Verificar partición actual y confirmar imagen si venimos de una actualización OTA
    bool updated_on_this_boot = false;
    ESP_ERROR_CHECK(ota_manager_verify_boot_partition(&updated_on_this_boot));

    // 3. Arrancar la tarea trabajadora de OTA en segundo plano
    ESP_ERROR_CHECK(ota_manager_start_task());

    // 4. Conectar a la red Wi-Fi
    ESP_LOGI(TAG, "Iniciando conexion Wi-Fi STA...");
    if (wifi_sta_init(WIFI_SSID, WIFI_PASS) != ESP_OK) {
        ESP_LOGE(TAG, "Sin conectividad Wi-Fi. No es posible contactar con ThingsBoard ni GitHub.");
        return;
    }

    // 5. Iniciar cliente MQTT con ThingsBoard (realiza la comprobación OTA de arranque al conectar)
    ESP_ERROR_CHECK(mqtt_tb_start(updated_on_this_boot));

    // 6. Iniciar sincronización NTP y planificador de actualización programada (03:00 AM)
    ESP_ERROR_CHECK(time_sync_and_scheduler_start());

    // 7. Bucle de telemetría periódica de estado de versión cada 30 segundos
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        if (mqtt_tb_is_connected()) {
            mqtt_tb_publish_heartbeat();
        }
    }
}
