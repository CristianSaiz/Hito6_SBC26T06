#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// --- Credenciales de Red Wi-Fi ---
#define WIFI_SSID                   "TU_RED_WIFI"
#define WIFI_PASS                   "TU_PASSWORD"
#define WIFI_MAX_RETRY              5

// --- Configuración de ThingsBoard (MQTT) ---
#define TB_BROKER_URI               "mqtt://eu.thingsboard.cloud:1883"
#define TB_DEVICE_TOKEN             "TU_DEVICE_TOKEN"

// Topics MQTT de ThingsBoard para Telemetría, Atributos y RPC
#define TB_TELEMETRY_TOPIC          "v1/devices/me/telemetry"
#define TB_ATTRIBUTES_TOPIC         "v1/devices/me/attributes"
#define TB_ATTR_REQUEST_TOPIC       "v1/devices/me/attributes/request/1"
#define TB_ATTR_RESPONSE_SUB        "v1/devices/me/attributes/response/+"
#define TB_RPC_REQUEST_SUB          "v1/devices/me/rpc/request/+"
#define TB_RPC_RESPONSE_PREFIX      "v1/devices/me/rpc/response/"

// --- Identificación del Firmware Actual ---
// Al compilar el binario nuevo para subir a GitHub, incrementar a "1.1.0"
#define FIRMWARE_TITLE              "hito6_sbc26t06"
#define FIRMWARE_VERSION            "1.2.0"

// URL por defecto del binario en GitHub Releases / Raw (puede sobreescribirse desde ThingsBoard con el atributo compartido 'fw_url')
#define DEFAULT_OTA_BIN_URL         "https://raw.githubusercontent.com/CristianSaiz/Hito6_SBC26T06/main/ota/hito6_sbc26t06.bin"

// --- Programación Horaria OTA (Requisito 2: Comprobación diaria sin reinicio) ---
#define NTP_SERVER_PRIMARY          "pool.ntp.org"
#define TIMEZONE_MADRID             "CET-1CEST,M3.5.0,M10.5.0/3"
#define OTA_SCHEDULED_HOUR          3
#define OTA_SCHEDULED_MINUTE        0

// Pin del LED integrado en placa para feedback visual durante el flasheo OTA
#define OTA_STATUS_LED_GPIO         2

#endif // APP_CONFIG_H
