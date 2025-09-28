# SISTEMA HALO 

## DESCRIPCIÓN GENERAL
El sistema HALO es una balanza inteligente basada en ESP32 que integra medición de peso, almacenamiento local, conectividad WiFi/MQTT y gestión de energía. El sistema está diseñado para funcionar de manera autónoma, realizando mediciones periódicas y sincronizando datos con un servidor central.

## ARQUITECTURA DEL SISTEMA

### HARDWARE PRINCIPAL
- **Microcontrolador**: ESP32 (Dual Core, 240MHz)
- **Sensor de Peso**: HX711 (24-bit ADC para celdas de carga)
- **Reloj de Tiempo Real**: DS3231 
- **Almacenamiento**: Tarjeta SD (SPI)
- **Medición de Batería**: BQ27427 (Fuel Gauge)
- **Comunicación**: WiFi 802.11 b/g/n
- **Interfaz de Usuario**: Botón físico con LED indicador

### CONFIGURACIÓN DE PINES
```
HX711_DOUT    = GPIO 17    (Datos del sensor)
HX711_SCK     = GPIO 16    (Clock del sensor)

I2C_SDA       = GPIO 21    (I2C Data - RTC y Batería)
I2C_SCL       = GPIO 22    (I2C Clock - RTC y Batería)

USER_BUTTON   = GPIO 25    (Botón de usuario)
LED_STATUS    = GPIO 0     (LED de estado)
PIN_POWER     = GPIO 26    (pin de alimentación)

SD_MOSI       = GPIO 23    (SPI MOSI)
SD_MISO       = GPIO 19    (SPI MISO)
SD_CLK        = GPIO 18    (SPI Clock)
SD_CS         = GPIO 5     (SPI Chip Select)
```

## ARQUITECTURA DE SOFTWARE

### DISTRIBUCIÓN DE TAREAS FREERTOS

#### NÚCLEO 0 (Protocolo):
- **task_HX711**: Lectura del sensor de peso
  - Stack: 3072 bytes
  - Prioridad: 6 (ALTA)
  - Función: Medición continua, calibración, logging a SD

#### NÚCLEO 1 (Aplicación):
- **task_MQTT**: Comunicaciones de red y envío de datos
  - Stack: 6144 bytes (SSL requiere más memoria)
  - Prioridad: 4 (MEDIA)
  - Función: Conexión WiFi, envío MQTT, gestión de credenciales

- **user_button_task**: Interfaz de usuario
  - Stack: 2048 bytes (lógica mínima)
  - Prioridad: 8 (MUY ALTA)
  - Función: Detección de pulsaciones, SmartConfig, comandos

### ESTADOS DEL SISTEMA

#### Estados de la Tarea HX711:
1. **HX711_ESPERA_INICIALIZACION**: Espera inicialización del sistema
2. **HX711_ESPERA_FECHA_HORA**: Espera sincronización de tiempo del servidor
3. **HX711_ESPERA_COMANDO**: Espera comandos de calibración
4. **HX711_CALIBRACION**: Ejecuta proceso de calibración
5. **HX711_ESPERA_PESO**: Espera peso conocido para calibración
6. **HX711_ESPERA_CONFIG_HORARIO**: Espera configuración de horario de envío
7. **HX711_MEDICION**: Estado principal de medición continua

#### Estados de la Tarea MQTT:
1. **MQTT_ESPERA_INICIALIZACION**: Espera inicialización del sistema
2. **MQTT_ESPERA_HORARIO_ENVIO**: Espera horario programado de envío
3. **MQTT_CONECTANDO_WIFI**: Establece conexión WiFi
4. **MQTT_CONECTANDO_BROKER**: Conecta al broker MQTT
5. **MQTT_ENVIANDO_DATOS**: Envía datos almacenados en SD
6. **MQTT_FINALIZANDO_ENVIO**: Finaliza envío y desconecta
7. **MQTT_ESPERANDO_SIGUIENTE_CICLO**: Espera próximo ciclo de envío

## FUNCIONALIDADES PRINCIPALES

### 1. MEDICIÓN DE PESO
- **Sensor**: HX711 con resolución de 24 bits
- **Calibración**: Sistema de calibración con peso conocido
- **Filtrado**: Validación de lecturas con umbral de error
- **Almacenamiento**: Guardado automático en tarjeta SD (CSV)

### 2. GESTIÓN DE TIEMPO
- **RTC**: DS3231 para mantener timer sin alimentación
- **Sincronización**: Actualización de tiempo vía MQTT
- **Zona Horaria**: Configuración automática de zona horaria
- **Timestamp**: Marcado temporal preciso de cada medición

### 3. CONECTIVIDAD DE RED
- **WiFi**: Conexión automática con credenciales guardadas
- **SmartConfig**: Configuración inalámbrica mediante app móvil
- **MQTT**: Comunicación segura con broker (TLS/SSL)
- **Reconexión**: Sistema automático de reconexión en caso de fallo

### 4. ALMACENAMIENTO LOCAL
- **SD Card**: Almacenamiento persistente de mediciones
- **Formato CSV**: Estructura: Fecha,Hora,Peso
- **Rotación**: Gestión automática de espacio en disco
- **Sincronización**: Envío diferido de datos pendientes

### 5. GESTIÓN DE ENERGÍA
- **Fuel Gauge**: Monitoreo de batería con BQ27427
- **Modo Ahorro**: Desconexión automática tras envío de datos
- **Reactivación**: Reconexión en horarios programados
- **LEDs**: Indicadores visuales de estado del sistema

### 6. INTERFAZ DE USUARIO
- **Botón Físico**: Control manual del sistema
- **Pulsación Corta**: Coneccion al servidor
- **Pulsación Larga**: Activación de SmartConfig
- **LEDs**: Retroalimentación visual del estado

## PROTOCOLOS DE COMUNICACIÓN

### MQTT Topics
```
esp32/command              - Comandos generales del sistema
esp32/set_schedule         - Configuración de horario de envío
esp32/set_time             - Sincronización de fecha/hora
esp32/halo/status          - Estado del sistema
esp32/halo/conection       - Estado de conexión
esp32/halo/weight_data     - Datos de peso
esp32/halo/device_info     - Información del dispositivo
```

### Comandos MQTT Soportados
- **CALIBRAR**: Inicia proceso de calibración del sensor
- **PESO_XXXX**: Especifica peso conocido para calibración
- **HORARIO_HH:MM**: Configura horario de envío diario
- **FECHA_YYYY-MM-DD_HH:MM:SS**: Sincroniza fecha y hora
- **REINICIAR**: Reinicia el sistema completo

## CONFIGURACIÓN Y CALIBRACIÓN

### Proceso de Calibración
1. **Comando CALIBRAR** vía MQTT
2. **Medición de offset** (tara) automática
3. **Espera de peso conocido** (comando PESO_XXXX)
4. **Cálculo de factor de escala**
5. **Guardado en NVS** para persistencia
6. **Validación** de calibración

### Configuración de Red
1. **SmartConfig**: App móvil envía credenciales WiFi
2. **Almacenamiento**: Guardado seguro en NVS
3. **Múltiples redes**: Soporte para hasta 3 redes WiFi
4. **Reconexión automática**: Intento de conexión a redes guardadas

## GESTIÓN DE DATOS

### Almacenamiento Local (SD)
```csv
Fecha,Hora,Peso
2024-01-15,14:30:25,1250.5
2024-01-15,14:30:35,1251.2
2024-01-15,14:30:45,1250.8
```

### Sincronización con Servidor
- **Envío programado**: Diario a hora configurada
- **Envío diferido**: Datos pendientes en próximo ciclo
- **Validación temporal**: Solo envío de datos del día actual
- **Confirmación**: Actualización de índice de última muestra enviada

## CONFIGURACIÓN DE SISTEMA

### Parámetros NVS (Non-Volatile Storage)
```
halo/ultima_muestra        - Índice de última muestra enviada
halo/hora_envio           - Hora programada de envío (0-23)
halo/minuto_envio         - Minuto programado de envío (0-59)
halo/muestreo_ms          - Intervalo entre mediciones (ms)
halo/wifi_ssid_0/1/2      - Credenciales WiFi (3 slots)
halo/wifi_pass_0/1/2      - Contraseñas WiFi (3 slots)
hx711_cal/offset          - Offset de calibración del sensor
hx711_cal/scale           - Factor de escala del sensor
hx711_cal/calibrated      - Flag de calibración válida
```

### Configuración por Defecto
- **Muestreo**: 10 segundos entre mediciones
- **Envío**: 00:00 (medianoche) diario
- **Timeout WiFi**: 30 segundos
- **Timeout MQTT**: 30 segundos
- **SmartConfig**: 120 segundos máximo

## DIAGNÓSTICO Y MONITOREO

### Función de Diagnóstico
```c
void diagnostico_sistema(void) {
    ESP_LOGI(TAG, "=== DIAGNÓSTICO DEL SISTEMA ===");
    ESP_LOGI(TAG, "SD: %s", sdcard_info.is_mounted ? "MONTADA" : "SIN SD");

    struct tm timeinfo;
    if (rtc_get_time(&timeinfo)) {
        ESP_LOGI(TAG, "RTC: FUNCIONANDO - %02d:%02d:%02d", 
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        ESP_LOGE(TAG, "RTC: ERROR");
    }
    
    // Verificar estado de credenciales WiFi
    if (smartconfig_has_saved_credentials()) {
        char ssid[20];
        char password[30];
        if (smartconfig_load_credentials(ssid, password, sizeof(ssid)) == ESP_OK) {
            ESP_LOGI(TAG, "WiFi: 📱 SMARTCONFIG ACTIVO - SSID: %s", ssid);
            ESP_LOGI(TAG, "WiFi: ✅ El sistema usará credenciales de SmartConfig");
        } else {
            ESP_LOGE(TAG, "WiFi: ❌ ERROR AL CARGAR CREDENCIALES DE SMARTCONFIG");
        }
    } else {
        ESP_LOGW(TAG, "WiFi: ⚠️ SIN CREDENCIALES DE SMARTCONFIG");
    }
}
```

### Logs del Sistema
- **Nivel ERROR**: Errores críticos del sistema
- **Nivel WARN**: Advertencias y fallos recuperables
- **Nivel INFO**: Información general de funcionamiento
- **Nivel DEBUG**: Información detallada de debug
- **Nivel VERBOSE**: Información muy detallada

## SEGURIDAD Y ROBUSTEZ

### Protección de Datos
- **Mutex**: Protección de acceso concurrente a SD y configuración
- **Timeouts**: Prevención de bloqueos indefinidos
- **Validación**: Verificación de integridad de datos
- **Recuperación**: Sistema de recuperación ante fallos

### Gestión de Errores
- **Reintentos**: Sistema automático de reintentos para conexiones
- **Fallback**: Funcionamiento offline cuando no hay conectividad
- **Logging**: Registro detallado de errores para diagnóstico
- **Recuperación**: Reinicio automático en casos críticos


## MANTENIMIENTO Y ACTUALIZACIONES

### Actualización de Firmware
- **OTA**: Actualización inalámbrica vía MQTT
- **Serial**: Actualización vía puerto serie
- **Bootloader**: Sistema de arranque seguro

### Resolución de Problemas
1. **Verificar logs** del sistema
2. **Comprobar conectividad** WiFi/MQTT
3. **Validar calibración** del sensor
4. **Revisar configuración** NVS
5. **Resetear sistema** si es necesario

## ESPECIFICACIONES TÉCNICAS

### Rendimiento
- **Frecuencia de muestreo**: Configurable (default: 10s)
- **Resolución de peso**: 24 bits (HX711)
- **Precisión temporal**: ±2ppm (DS3231)
- **Capacidad de almacenamiento**: Limitada por SD
- **Tiempo de respuesta**: <100ms para comandos

### Consumo de Energía
- **Modo activo**: ~200mA (medición + WiFi)
- **Modo ahorro**: ~50mA (solo medición)
- **Modo sleep**: ~10mA (RTC + sistema mínimo)
- **Autonomía**: Dependiente de capacidad de batería

### Conectividad
- **WiFi**: 802.11 b/g/n (2.4GHz)
- **MQTT**: TLS/SSL sobre TCP
- **I2C**: 400kHz (RTC y batería)
- **SPI**: 1MHz (SD card)
