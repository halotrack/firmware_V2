#ifndef INIT_H
#define INIT_H


#include "HALO.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include <limits.h>
#include <inttypes.h>
#include "freertos/portmacro.h"
#include "esp_log.h"

esp_err_t sistema_init_config(void); 
bool hay_datos_pendientes_envio(void);

void inicializar_sistema(void);                    
void enviar_mac(void);
void guardar_ultima_muestra_enviada(void); 
int leer_ultimas_muestras_sd(int n, float *pesos, struct tm *tiempos);
void restaurar_ultima_muestra_enviada(void);
void guardar_horario_envio(void);
void restaurar_horario_envio(void);  
#endif // INIT_H 