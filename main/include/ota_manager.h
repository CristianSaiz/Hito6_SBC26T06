#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    OTA_TRIGGER_BOOT = 0,
    OTA_TRIGGER_SCHEDULED,
    OTA_TRIGGER_RPC_ONDEMAND,
    OTA_TRIGGER_ATTR_PUSH
} ota_trigger_source_t;

// Verifica al arrancar si venimos de una actualización OTA pendiente de validar
esp_err_t ota_manager_verify_boot_partition(bool *out_was_updated);

// Actualiza los metadatos del firmware objetivo recibidos desde ThingsBoard
void ota_manager_set_target_metadata(const char *target_version, const char *target_url);

// Dispara una comprobación/ejecución de OTA en la tarea dedicada (no bloqueante)
void ota_manager_request_check(ota_trigger_source_t source, bool force_check_tb);

// Inicializa la tarea en segundo plano de gestión OTA
esp_err_t ota_manager_start_task(void);

#endif // OTA_MANAGER_H
