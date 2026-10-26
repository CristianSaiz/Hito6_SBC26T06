#ifndef MQTT_TB_H
#define MQTT_TB_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t mqtt_tb_start(bool boot_was_updated);
bool mqtt_tb_is_connected(void);
void mqtt_tb_request_firmware_attributes(void);
void mqtt_tb_report_ota_state(const char *fw_state, const char *target_ver, const char *detail);
void mqtt_tb_publish_heartbeat(void);

#endif // MQTT_TB_H
