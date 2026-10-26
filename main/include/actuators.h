#ifndef ACTUATORS_H
#define ACTUATORS_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa los buses y pines del entrenador:
 * - Display 7 segmentos (Ánodo Común 5161BS)
 * - Pantalla OLED SSD1306 vía I2C (128x64)
 * - Sensor LDR mediante ADC1 canal 6
 */
esp_err_t actuators_init(void);

/**
 * @brief Configura el dígito mostrado en el display de 7 segmentos (0-9).
 */
void actuators_set_7seg_digit(int digit);

/**
 * @brief Actualiza las líneas informativas de la pantalla OLED.
 */
void actuators_update_oled(const char *version, const char *partition, const char *status);

/**
 * @brief Muestra el avance de la descarga OTA y una barra gráfica de progreso.
 */
void actuators_update_ota_progress(int percent, int kb_received);

/**
 * @brief Obtiene el último nivel lumínico muestreado en escala 0-9.
 */
int actuators_get_light_level(void);

#ifdef __cplusplus
}
#endif

#endif // ACTUATORS_H
