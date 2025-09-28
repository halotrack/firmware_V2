// === conexion.h ===
#ifndef CONEXION_H
#define CONEXION_H

#include <stdbool.h>

bool conectar_wifi_con_reintentos(void);
bool conectar_mqtt_con_reintentos(void);
void desconectar_wifi_seguro(void);
void desconectar_mqtt_seguro(void);

bool esperar_estado(bool (*check_func)(void), int timeout_s, const char *msg);

#endif // CONEXION_H
