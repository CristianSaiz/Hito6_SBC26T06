# Hito 6: Actualización automática del firmware mediante OTA (Over-the-Air)

## Metadatos del proyecto

* **Asignatura:** Sistemas Basados en Computador (SBC) — ETSISI UPM
* **Identificador de grupo:** `SBC26T06` (turno de prácticas: **Grupo B**)
* **Miembros del grupo:**
  * Ana Cuevas
  * Cristian Saiz
* **Plataforma hardware:** ESP32-WROOM-32 (flash SPI de 4 MB con particionado Dual-OTA) montada en el entrenador SBC
* **Entorno y framework:** ESP-IDF v5.3 / FreeRTOS
* **Plataforma IoT y repositorio de binarios:** ThingsBoard Cloud (`eu.thingsboard.cloud`) + GitHub (carpeta `ota/`)
* **Repositorio GitHub:** https://github.com/CristianSaiz/Hito6_SBC26T06.git

---

## 1. Qué hace el proyecto

El ESP32 se conecta por Wi-Fi a ThingsBoard y es capaz de actualizarse él solo: descarga el binario nuevo desde GitHub por HTTPS, lo graba en la partición libre y arranca desde ella. Si la imagen nueva da problemas al arrancar, el bootloader vuelve a la anterior sin que tengamos que tocar nada.

Además, la placa del entrenador nos sirve para ver el estado sin abrir el monitor serie: la pantalla OLED enseña la versión y la partición activa, y el display de 7 segmentos muestra el dígito intermedio de la versión (en la `1.3.0` aparece un `3`).

### Cómo cumple cada requisito

1. **Comprobación al arrancar.**
   Cuando el ESP32 conecta por MQTT, pide a ThingsBoard los atributos compartidos `fw_version` y `fw_url`. Si la versión publicada es distinta de la instalada (`FIRMWARE_VERSION`), lanza la descarga HTTPS y la instala en la partición inactiva (`ota_0` u `ota_1`).

2. **Actualización programada.**
   El módulo `time_sync` sincroniza el reloj por SNTP (`pool.ntp.org`, huso horario de Madrid, CET/CEST). Una tarea de FreeRTOS vigila la hora local y, a las 03:00 (`OTA_SCHEDULED_HOUR` y `OTA_SCHEDULED_MINUTE`), dispara la comprobación sin necesidad de reiniciar el dispositivo. Solo salta una vez al día.

3. **Actualización bajo demanda.**
   El cliente MQTT escucha en `v1/devices/me/rpc/request/+`. Al pulsar el botón del dashboard de ThingsBoard (método RPC `checkOTA`, también valen `startOTA` y `forceOTA`), el ESP32 responde a la orden, refresca los atributos y ejecuta el OTA al momento.

4. **Verificación y rollback.**
   Con `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`, la imagen recién instalada arranca en estado `ESP_OTA_IMG_PENDING_VERIFY`. Si el firmware llega hasta `ota_manager_verify_boot_partition()`, la marca como válida y manda a ThingsBoard la confirmación (`fw_state = "UPDATED"`, `current_fw_version` y `active_partition`). Si se cuelga antes, el bootloader vuelve solo a la versión anterior.

### Detalles del motor OTA que conviene conocer

* **Tarea dedicada.** Todo el OTA corre en `ota_worker_task` (stack de 8 KB, que mbedTLS agradece). Hay que arrancarla con `ota_manager_start_task()` **antes** de iniciar MQTT, porque crea los semáforos que usan el RPC y la recepción de atributos.
* **Espera real de los atributos.** Cuando el chequeo pide los atributos a ThingsBoard, la tarea espera hasta 5 s a que lleguen de verdad, en lugar de dormir un tiempo fijo. Así no compara contra una versión vieja si el Wi-Fi va lento.
* **URL con versión.** A la URL de descarga se le añade `?v=<versión>`. La caché de `raw.githubusercontent.com` funciona por URL completa, y de este modo cada versión se baja fresca y no se queda con el `.bin` anterior durante unos minutos.
* **Protección contra binarios repetidos.** Si la cabecera del `.bin` descargado trae la misma versión que la instalada, se aborta antes de flashear y se avisa a ThingsBoard. Suele significar que se olvidó subir el binario nuevo.

---

## 2. Hardware local (entrenador SBC)

| Elemento | Detalle | Pines ESP32 |
|---|---|---|
| Pantalla OLED SSD1306 (128x64) | Bus I2C, dirección `0x3C` | SDA = GPIO 21, SCL = GPIO 22 |
| Display de 7 segmentos 5161BS | Ánodo común, segmentos A a G | 13, 12, 14, 27, 26, 25, 33 |
| Sensor de luz LDR | ADC1 canal 6, muestreado en segundo plano | GPIO 34 |
| LED de estado | Parpadea durante la descarga OTA | GPIO 2 |

El código de estos periféricos está en `actuators.c`, y `main.c` solo llama a su API.

---

## 3. Estructura del proyecto

```text
Hito6_SBC26T06/
├── CMakeLists.txt              # Proyecto CMake (hito6_sbc26t06) y PROJECT_VER
├── partitions.csv              # Tabla Dual-OTA (nvs, otadata, phy_init, ota_0, ota_1)
├── sdkconfig                   # Configuración generada por menuconfig
├── sdkconfig.defaults          # Rollback, flash 4 MB y bundle de certificados X.509
├── README.md                   # Esta documentación
├── .gitignore
├── ota/
│   └── hito6_sbc26t06.bin      # Binario que descarga el ESP32 (se sube a mano en cada versión)
└── main/
    ├── CMakeLists.txt          # Fuentes y dependencias del componente
    ├── main.c                  # Arranque, verificación de partición y bucle principal
    ├── include/
    │   ├── actuators.h         # API de OLED, 7 segmentos y LDR
    │   ├── app_config.h        # Wi-Fi, ThingsBoard, versión del firmware y hora del OTA
    │   ├── mqtt_tb.h           # Interfaz MQTT con ThingsBoard (atributos y RPC)
    │   ├── ota_manager.h       # API del motor OTA
    │   ├── time_sync.h         # API del reloj SNTP y del planificador diario
    │   └── wifi_sta.h          # Interfaz Wi-Fi en modo estación
    └── src/
        ├── actuators.c         # Drivers de OLED (I2C), 7 segmentos (GPIO) y LDR (ADC)
        ├── mqtt_tb.c           # Atributos compartidos, RPC y reporte de estado OTA
        ├── ota_manager.c       # Descarga HTTPS, validación y reinicio
        ├── time_sync.c         # Sincronización NTP y disparo a las 03:00
        └── wifi_sta.c          # Conexión Wi-Fi y gestión de eventos
```

---

## 4. Configuración en ThingsBoard y GitHub

1. **Preparar el binario nuevo.**
   Sube la versión en **los dos sitios** y procura que coincidan siempre:
   * `FIRMWARE_VERSION` en `main/include/app_config.h` (lo que el código muestra y compara).
   * `PROJECT_VER` en el `CMakeLists.txt` de la raíz (lo que queda grabado en la cabecera del `.bin`).

   Después compila y copia el binario a `ota/`:

   ```bash
   idf.py build
   cp build/hito6_sbc26t06.bin ota/hito6_sbc26t06.bin
   git add CMakeLists.txt main/ ota/hito6_sbc26t06.bin
   git commit -m "release: firmware 1.x.0"
   git push origin main
   ```

2. **Atributos compartidos en ThingsBoard** (dispositivo, pestaña *Attributes*, *Shared attributes*):
   * `fw_version`: la versión nueva, por ejemplo `"1.2.0"`.
   * `fw_url` (opcional): si no se pone, se usa `DEFAULT_OTA_BIN_URL` de `app_config.h`, que ya apunta a
     `https://raw.githubusercontent.com/CristianSaiz/Hito6_SBC26T06/main/ota/hito6_sbc26t06.bin`

3. **Botón en el dashboard.**
   Un widget *RPC Button* que envíe el método `checkOTA` con parámetros `{}`.

---

## 5. Compilación, flasheo y prueba

```bash
# 1. Rellenar Wi-Fi y token de ThingsBoard en main/include/app_config.h
# 2. Compilar y flashear por USB la versión "vieja" (la que vamos a actualizar)
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Para probar un salto de versión (por ejemplo de `1.1.0` a `1.2.0`):

1. Compila la `1.2.0`, cópiala a `ota/` y haz push a GitHub.
2. Pon `fw_version = 1.2.0` en ThingsBoard.
3. Cambia las dos versiones del código a `1.1.0`, compila y flashea por USB. No subas esta compilación al repositorio.
4. Pulsa el botón del dashboard y mira el monitor serie. Debe aparecer la comparación de versiones, la descarga, el reinicio y, ya en la partición nueva, el mensaje de imagen validada.
5. Vuelve a dejar el código en `1.2.0` para que el repositorio quede coherente.

### Mensajes típicos en el monitor serie

| Mensaje | Qué significa |
|---|---|
| `Comprobacion OTA disparada [Origen: RPC_ON_DEMAND]` | El botón del dashboard ha llegado al ESP32 |
| `Comparando versiones -> Instalada: X \| Disponible en TB: Y` | La tarea OTA ya tiene la versión objetivo |
| `Cabecera recibida -> ... Version en binario: Y` | El `.bin` de GitHub es el esperado |
| `El binario remoto es la misma version instalada` | Falta subir el `.bin` nuevo a `ota/` o `PROJECT_VER` no coincide |
| `Imagen OTA marcada como VALIDA` | Arranque correcto en la partición nueva, rollback cancelado |
