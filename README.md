# Hito 6: Actualización Automática del Firmware mediante OTA (Over-the-Air)

## Metadatos del Proyecto

* **Asignatura:** Sistemas Basados en Computador (SBC) — ETSISI UPM
* **Identificador de Grupo:** `SBC26T06` (Turno de Prácticas: **Grupo B**)
* **Miembros del Grupo:**
  * Ana Cuevas
  * Cristian Saiz
* **Plataforma Hardware:** ESP32-WROOM-32 (Flash SPI 4 MB con particionado Dual-OTA)
* **Entorno y Framework:** ESP-IDF v5.3 / FreeRTOS Kernel
* **Plataforma IoT y Repositorio:** ThingsBoard Cloud (`eu.thingsboard.cloud`) + GitHub Releases/Raw
* **Repositorio GitHub:** https://github.com/CristianSaiz/Hito6_SBC26T06.git

---

## 1. Descripción Técnica y Cumplimiento de Requisitos

Este proyecto implementa un sistema completo de actualización inalámbrica de firmware (OTA) para el microcontrolador ESP32, gestionado desde ThingsBoard y alojando los binarios compilados en GitHub:

1. **Comprobación al Arrancar (On Boot):**
   Al iniciar el ESP32 y establecer sesión MQTT con ThingsBoard, el firmware solicita automáticamente los atributos compartidos (`fw_version` y `fw_url`). Si detecta que la versión publicada en ThingsBoard es distinta de la instalada en flash (`FIRMWARE_VERSION`), inicia la descarga HTTPS e instala la nueva imagen en la partición inactiva (`ota_0` u `ota_1`).

2. **Actualización Programada (Scheduled Check):**
   El módulo `time_sync` sincroniza el reloj RTC interno mediante SNTP (`pool.ntp.org`, huso horario `CET/CEST` de Madrid). Una tarea de FreeRTOS supervisa la hora local y dispara automáticamente la comprobación y actualización a la hora configurada (`OTA_SCHEDULED_HOUR = 3`, `OTA_SCHEDULED_MINUTE = 0`, 03:00 AM) sin requerir reinicio previo del dispositivo.

3. **Actualización Bajo Demanda (RPC On-Demand):**
   El cliente MQTT escucha en el topic `v1/devices/me/rpc/request/+`. Al pulsar el botón de actualización en el Dashboard de ThingsBoard (método RPC `checkOTA` o `startOTA`), el ESP32 confirma la orden, consulta los atributos de versión y ejecuta el flujo OTA en caliente.

4. **Verificación de la Actualización y Rollback:**
   Se habilita `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`. Tras flashear y reiniciar en la nueva partición, `ota_manager_verify_boot_partition()` valida el estado `ESP_OTA_IMG_PENDING_VERIFY`, cancela el rollback y publica en ThingsBoard la telemetría de confirmación (`fw_state = "UPDATED"`, `current_fw_version` y `active_partition`).

---

## 2. Estructura del Proyecto

    Hito6_SBC26T06/
    ├── CMakeLists.txt              # Proyecto CMake (hito6_sbc26t06)
    ├── partitions.csv              # Tabla Dual-OTA (nvs, otadata, ota_0, ota_1)
    ├── sdkconfig.defaults          # Rollback, Flash 4MB y Bundle de certificados X.509
    ├── README.md                   # Documentación técnica del Hito 6
    └── main/
        ├── CMakeLists.txt          # Dependencias (app_update, esp_https_ota, mqtt, json)
        ├── main.c                  # Arranque, verificación de partición y bucle principal
        ├── include/
        │   ├── app_config.h        # Parámetros Wi-Fi, ThingsBoard, versión y hora OTA
        │   ├── mqtt_tb.h           # Interfaz MQTT ThingsBoard (Atributos y RPC)
        │   ├── ota_manager.h       # API del motor HTTPS OTA
        │   ├── time_sync.h         # API de reloj SNTP y planificador diario
        │   └── wifi_sta.h          # Interfaz Wi-Fi en modo Estación
        └── src/
            ├── mqtt_tb.c           # Gestión de atributos compartidos, RPC y reporte OTA
            ├── ota_manager.c       # Descarga HTTPS con soporte de redirección GitHub
            ├── time_sync.c         # Sincronización NTP y disparo a las 03:00 AM
            └── wifi_sta.c          # Conexión Wi-Fi y control de eventos

---

## 3. Configuración en ThingsBoard y GitHub

1. **Alojar el Binario Nuevo en GitHub:**
   * Cambiar `FIRMWARE_VERSION` en `main/include/app_config.h` de `"1.0.0"` a `"1.1.0"`.
   * Compilar con `idf.py build` y subir el archivo `build/hito6_sbc26t06.bin` al repositorio de GitHub (por ejemplo en la carpeta `ota/hito6_sbc26t06.bin` o en *Releases*).
2. **Atributos Compartidos en ThingsBoard (*Shared Attributes*):**
   En el dispositivo de ThingsBoard -> pestaña *Attributes* -> *Shared attributes*, añadir:
   * `fw_version`: `"1.1.0"`
   * `fw_url`: `"https://raw.githubusercontent.com/CristianSaiz/Hito6_SBC26T06/main/ota/hito6_sbc26t06.bin"`
3. **Botón RPC en el Dashboard (Bajo Demanda):**
   Añadir un widget *Action Button / RPC Button* que envíe el método `checkOTA` con parámetros `{}`.

---

## 4. Compilación y Flasheo Inicial

    # 1. Configurar credenciales Wi-Fi y Token en main/include/app_config.h
    # 2. Compilar proyecto con la versión base 1.0.0
    idf.py build

    # 3. Flashear tabla de particiones Dual-OTA y aplicación inicial
    idf.py -p /dev/ttyUSB0 flash monitor
