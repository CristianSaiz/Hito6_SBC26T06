#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

#include "app_config.h"
#include "wifi_sta.h"
#include "time_sync.h"
#include "ota_manager.h"
#include "mqtt_tb.h"
#include "actuators.h"

static const char *TAG = "MAIN_APP";

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "INICIANDO FIRMWARE HITO 6 (OTA + HW) - SBC26T06");
    ESP_LOGI(TAG, "Version compilada en ejecucion: %s", FIRMWARE_VERSION);
    ESP_LOGI(TAG, "==================================================");

    // 1. Inicializar perifericos locales del entrenador (OLED + 7-Seg + LDR)
    actuators_init();

    // 2. Extraer el digito menor de la version para el display 7-Seg (ej. de '1.5.0' toma el 5)
    // Extraer la version menor (ej. de '1.5.0' extrae el 5) para el display de 7 segmentos
    int major = 0, minor = 0, patch = 0;
    sscanf(FIRMWARE_VERSION, "%d.%d.%d", &major, &minor, &patch);
    actuators_set_7seg_digit(minor);

    // 3. Inicializar almacenamiento no volatil (NVS)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 4. Validar si arrancamos tras una actualizacion OTA (Anti-Rollback)
    bool was_updated = false;
    ota_manager_verify_boot_partition(&was_updated);

    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "Particion activa en ejecucion: '%s' | Version: %s", 
             running ? running->label : "desconocida", FIRMWARE_VERSION);

    // 5. Refrescar la pantalla OLED con la version y ranura real
    actuators_update_oled(FIRMWARE_VERSION, running ? running->label : "ota_0", was_updated ? "UPDATED" : "NORMAL");

    // 6. Conectar Wi-Fi en modo Estacion (STA) usando credenciales configuradas
    ESP_ERROR_CHECK(wifi_sta_init(WIFI_SSID, WIFI_PASS));

    // 7. Sincronizar reloj por SNTP y planificar comprobacion horaria (03:00 AM)
    ESP_ERROR_CHECK(time_sync_and_scheduler_start());

    // 8. Arrancar cliente MQTT ThingsBoard informando si venimos de actualizacion
    ESP_ERROR_CHECK(mqtt_tb_start(was_updated));

    ESP_LOGI(TAG, "Sistema completamente operativo y monitorizado.");

    // Bucle periodico de emision de telemetria / heartbeat
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (mqtt_tb_is_connected()) {
            mqtt_tb_publish_heartbeat();
        }
    }
}
