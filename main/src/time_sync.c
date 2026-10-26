#include "time_sync.h"
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"

#include "app_config.h"
#include "ota_manager.h"

static const char *TAG = "TIME_SCHED";

static void scheduler_task(void *pvParameters)
{
    // Esperar a que el reloj del sistema se sincronice vía NTP (año >= 2025)
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;

    while (timeinfo.tm_year < (2025 - 1900) && retry < 20) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeinfo);
        retry++;
    }

    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "Reloj sincronizado (CET/CEST): %s | Ventana OTA diaria programada a las %02d:%02d",
             strftime_buf, OTA_SCHEDULED_HOUR, OTA_SCHEDULED_MINUTE);

    int last_triggered_yday = -1;

    while (1) {
        time(&now);
        localtime_r(&now, &timeinfo);

        // Requisito 2: Ejecución programada a la hora fijada sin necesidad de reiniciar previamente
        if (timeinfo.tm_year >= (2025 - 1900)) {
            if (timeinfo.tm_hour == OTA_SCHEDULED_HOUR &&
                timeinfo.tm_min == OTA_SCHEDULED_MINUTE &&
                timeinfo.tm_yday != last_triggered_yday) {

                last_triggered_yday = timeinfo.tm_yday;
                ESP_LOGI(TAG, "Hora programada alcanzada (%02d:%02d). Disparando comprobacion OTA automatica...",
                         timeinfo.tm_hour, timeinfo.tm_min);
                ota_manager_request_check(OTA_TRIGGER_SCHEDULED, true);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20000));
    }
}

esp_err_t time_sync_and_scheduler_start(void)
{
    setenv("TZ", TIMEZONE_MADRID, 1);
    tzset();

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER_PRIMARY);
    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando SNTP: %s", esp_err_to_name(err));
        return err;
    }

    BaseType_t res = xTaskCreatePinnedToCore(
        scheduler_task,
        "ota_scheduler",
        4096,
        NULL,
        4,
        NULL,
        1
    );

    return (res == pdPASS) ? ESP_OK : ESP_FAIL;
}
