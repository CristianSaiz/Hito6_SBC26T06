#include "ota_manager.h"
#include "actuators.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "app_config.h"
#include "mqtt_tb.h"

static const char *TAG = "OTA_MGR";

static SemaphoreHandle_t s_ota_trigger_sem = NULL;
static SemaphoreHandle_t s_meta_mutex = NULL;
static SemaphoreHandle_t s_attr_sem = NULL;   // Se da cuando llegan los atributos pedidos a TB

static char s_target_version[32] = "";
static char s_target_url[256] = DEFAULT_OTA_BIN_URL;
static ota_trigger_source_t s_last_trigger = OTA_TRIGGER_BOOT;
static bool s_request_tb_sync = false;

static const char *trigger_to_str(ota_trigger_source_t src)
{
    switch (src) {
        case OTA_TRIGGER_BOOT:         return "BOOT_CHECK";
        case OTA_TRIGGER_SCHEDULED:    return "SCHEDULED_TIME";
        case OTA_TRIGGER_RPC_ONDEMAND: return "RPC_ON_DEMAND";
        case OTA_TRIGGER_ATTR_PUSH:    return "TB_SHARED_ATTR";
        default:                       return "UNKNOWN";
    }
}

esp_err_t ota_manager_verify_boot_partition(bool *out_was_updated)
{
    if (out_was_updated) {
        *out_was_updated = false;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;

    ESP_LOGI(TAG, "Particion activa en ejecucion: '%s' (offset 0x%08lx) | Version: %s",
             running->label, (unsigned long)running->address, FIRMWARE_VERSION);

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "Detectada imagen recien actualizada pendiente de validacion...");
            // Si hemos llegado hasta aquí tras inicializar hardware básico, validamos imagen y cancelamos rollback
            if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
                ESP_LOGI(TAG, "Imagen OTA marcada como VALIDA. Rollback desactivado.");
                if (out_was_updated) {
                    *out_was_updated = true;
                }
            } else {
                ESP_LOGE(TAG, "Fallo al confirmar validez de la imagen OTA");
                return ESP_FAIL;
            }
        }
    }
    return ESP_OK;
}

void ota_manager_set_target_metadata(const char *target_version, const char *target_url)
{
    if (s_meta_mutex == NULL) return;

    xSemaphoreTake(s_meta_mutex, portMAX_DELAY);
    if (target_version != NULL && strlen(target_version) > 0) {
        strncpy(s_target_version, target_version, sizeof(s_target_version) - 1);
        s_target_version[sizeof(s_target_version) - 1] = '\0';
    }
    if (target_url != NULL && strlen(target_url) > 0) {
        strncpy(s_target_url, target_url, sizeof(s_target_url) - 1);
        s_target_url[sizeof(s_target_url) - 1] = '\0';
    }
    xSemaphoreGive(s_meta_mutex);
}

void ota_manager_request_check(ota_trigger_source_t source, bool force_check_tb)
{
    s_last_trigger = source;
    s_request_tb_sync = force_check_tb;
    if (s_ota_trigger_sem != NULL) {
        xSemaphoreGive(s_ota_trigger_sem);
    }
}

void ota_manager_notify_attrs_received(void)
{
    if (s_attr_sem != NULL) {
        xSemaphoreGive(s_attr_sem);
    }
}

static esp_err_t perform_https_ota_download(const char *base_url, const char *new_ver)
{
    // Anadimos ?v=<version> a la URL: la cache de raw.githubusercontent.com va por URL completa,
    // asi cada version nueva se descarga siempre fresca y no el binario viejo cacheado.
    char url[300];
    snprintf(url, sizeof(url), "%s%cv=%s", base_url,
             strchr(base_url, '?') ? '&' : '?', new_ver);

    ESP_LOGI(TAG, "Iniciando descarga HTTPS OTA desde: %s", url);
    mqtt_tb_report_ota_state("DOWNLOADING", new_ver, "Descargando binario desde repositorio");

    // GitHub redirige (HTTP 302) a objects.githubusercontent.com con cabeceras largas.
    // Ampliar buffer_size a 4096 evita el error HTTP_HEADER_TOO_LONG en esp_http_client.
    esp_http_client_config_t http_cfg = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 20000,
        .keep_alive_enable = true,
        .buffer_size = 4096,
        .buffer_size_tx = 2048,
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_cfg, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Fallo en esp_https_ota_begin: %s", esp_err_to_name(err));
        mqtt_tb_report_ota_state("FAILED", new_ver, "Fallo al iniciar conexion HTTPS OTA");
        return err;
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo leer la cabecera del binario remoto: %s", esp_err_to_name(err));
        esp_https_ota_abort(https_ota_handle);
        mqtt_tb_report_ota_state("FAILED", new_ver, "Cabecera de binario invalida");
        return err;
    }

    ESP_LOGI(TAG, "Cabecera recibida -> Proyecto: %s | Version en binario: %s",
             app_desc.project_name, app_desc.version);

    // Si el binario descargado trae la misma version que el instalado, no tiene sentido
    // flashear y reiniciar (suele ser cache vieja o un .bin sin actualizar en GitHub).
    if (strcmp(app_desc.version, FIRMWARE_VERSION) == 0) {
        ESP_LOGW(TAG, "El binario remoto es la misma version instalada (%s). Abortando.", FIRMWARE_VERSION);
        esp_https_ota_abort(https_ota_handle);
        mqtt_tb_report_ota_state("FAILED", new_ver, "El binario remoto no es mas nuevo (revisa el .bin en GitHub)");
        return ESP_ERR_INVALID_VERSION;
    }

    int total_read = 0;
    int last_logged_kb = 0;
    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        total_read = esp_https_ota_get_image_len_read(https_ota_handle);
        if ((total_read / 1024) - last_logged_kb >= 64) {
            last_logged_kb = total_read / 1024;
            ESP_LOGI(TAG, "Progreso de descarga OTA: %d KB recibidos...", last_logged_kb);
            gpio_set_level(OTA_STATUS_LED_GPIO, (last_logged_kb / 64) % 2);
        }
    }

    if (err != ESP_OK || !esp_https_ota_is_complete_data_received(https_ota_handle)) {
        ESP_LOGE(TAG, "Descarga incompleta o interrumpida: %s", esp_err_to_name(err));
        esp_https_ota_abort(https_ota_handle);
        mqtt_tb_report_ota_state("FAILED", new_ver, "Corte durante la recepcion del flujo binario");
        return ESP_FAIL;
    }

    mqtt_tb_report_ota_state("DOWNLOADED", new_ver, "Binario descargado integramente en flash");
    vTaskDelay(pdMS_TO_TICKS(300));

    mqtt_tb_report_ota_state("VERIFIED", new_ver, "Imagen verificada con exito");
    err = esp_https_ota_finish(https_ota_handle);
    if (err == ESP_OK) {
        mqtt_tb_report_ota_state("UPDATING", new_ver, "Particion de arranque conmutada. Reiniciando ESP32...");
        ESP_LOGI(TAG, "OTA completada con exito. Reiniciando en 2 segundos para aplicar v%s...", new_ver);
        vTaskDelay(pdMS_TO_TICKS(2000));
        actuators_update_oled(FIRMWARE_VERSION, "REINICIO", "REBOOTING...");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    } else {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Imagen corrupta: fallo de validacion de checksum/firma");
        }
        mqtt_tb_report_ota_state("FAILED", new_ver, "Fallo en esp_https_ota_finish");
        return err;
    }

    return ESP_OK;
}

static void ota_worker_task(void *pvParameters)
{
    char local_target_ver[32];
    char local_target_url[256];

    while (1) {
        if (xSemaphoreTake(s_ota_trigger_sem, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "=== Comprobacion OTA disparada [Origen: %s] ===", trigger_to_str(s_last_trigger));

            if (s_request_tb_sync && mqtt_tb_is_connected()) {
                // Pedimos los atributos compartidos mas recientes y ESPERAMOS la respuesta real
                // (en vez de un delay fijo, que fallaba con Wi-Fi lento / ahorro de energia).
                xSemaphoreTake(s_attr_sem, 0);   // limpiar senal antigua
                mqtt_tb_request_firmware_attributes();
                if (xSemaphoreTake(s_attr_sem, pdMS_TO_TICKS(5000)) != pdTRUE) {
                    ESP_LOGW(TAG, "ThingsBoard no respondio con atributos en 5 s. Usando valores en cache.");
                }
            }

            xSemaphoreTake(s_meta_mutex, portMAX_DELAY);
            strncpy(local_target_ver, s_target_version, sizeof(local_target_ver) - 1);
            local_target_ver[sizeof(local_target_ver) - 1] = '\0';
            strncpy(local_target_url, s_target_url, sizeof(local_target_url) - 1);
            local_target_url[sizeof(local_target_url) - 1] = '\0';
            xSemaphoreGive(s_meta_mutex);

            if (strlen(local_target_ver) == 0) {
                ESP_LOGI(TAG, "Sin atributo 'fw_version' asignado aun en ThingsBoard. Version actual: %s", FIRMWARE_VERSION);
                mqtt_tb_report_ota_state("UP_TO_DATE", FIRMWARE_VERSION, "Sin nueva version asignada");
                continue;
            }

            ESP_LOGI(TAG, "Comparando versiones -> Instalada: %s | Disponible en TB: %s",
                     FIRMWARE_VERSION, local_target_ver);

            if (strcmp(FIRMWARE_VERSION, local_target_ver) == 0) {
                ESP_LOGI(TAG, "El dispositivo ya ejecuta la version mas reciente (%s).", FIRMWARE_VERSION);
                mqtt_tb_report_ota_state("UP_TO_DATE", FIRMWARE_VERSION, "El firmware ya esta actualizado");
            } else {
                ESP_LOGW(TAG, "Nueva version detectada (%s -> %s). Iniciando actualizacion...",
                         FIRMWARE_VERSION, local_target_ver);
                perform_https_ota_download(local_target_url, local_target_ver);
            }
        }
    }
}

esp_err_t ota_manager_start_task(void)
{
    gpio_reset_pin(OTA_STATUS_LED_GPIO);
    gpio_set_direction(OTA_STATUS_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(OTA_STATUS_LED_GPIO, 0);

    s_ota_trigger_sem = xSemaphoreCreateBinary();
    s_meta_mutex = xSemaphoreCreateMutex();
    s_attr_sem = xSemaphoreCreateBinary();

    if (s_ota_trigger_sem == NULL || s_meta_mutex == NULL || s_attr_sem == NULL) {
        return ESP_ERR_NO_MEM;
    }

    BaseType_t res = xTaskCreatePinnedToCore(
        ota_worker_task,
        "ota_worker_task",
        8192, // Stack amplio requerido por mbedTLS y cliente HTTPS
        NULL,
        5,
        NULL,
        1
    );

    return (res == pdPASS) ? ESP_OK : ESP_FAIL;
}