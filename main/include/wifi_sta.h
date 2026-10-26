#ifndef WIFI_STA_H
#define WIFI_STA_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t wifi_sta_init(const char *ssid, const char *password);
bool wifi_sta_is_connected(void);

#endif // WIFI_STA_H
