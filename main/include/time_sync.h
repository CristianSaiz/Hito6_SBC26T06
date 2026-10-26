#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include "esp_err.h"

// Inicializa SNTP con huso horario de Madrid y arranca la tarea que comprueba la hora (ej. 03:00 AM)
esp_err_t time_sync_and_scheduler_start(void);

#endif // TIME_SYNC_H
