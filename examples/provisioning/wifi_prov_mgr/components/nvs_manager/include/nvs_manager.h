#pragma once


#ifndef NVS_MANAGER_H

#define NVS_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa el subsistema de almacenamiento persistente
 * 
 * Configura el NVS para su uso en la aplicación con el namespace especificado.
 * Si no se proporciona un namespace (NULL o cadena vacía), se usará "ecokey" por defecto.
 * 
 * @param namespace Espacio de nombres para el almacenamiento, o NULL para usar el predeterminado
 * @return ESP_OK si la inicialización es exitosa, código de error en caso contrario
 */
esp_err_t nvs_manager_init(const char* namespace);

/**
 * @brief Verifica si el componente ha sido inicializado correctamente
 * 
 * @return true si está inicializado, false en caso contrario
 */
bool nvs_manager_is_initialized(void);

/**
 * @brief Guarda un valor entero en el almacenamiento persistente
 * 
 * @param key Clave bajo la cual se guarda el valor
 * @param value Valor a guardar
 * @return ESP_OK si la operación es exitosa, código de error en caso contrario
 */
esp_err_t nvs_manager_set_int(const char* key, int32_t value);

/**
 * @brief Recupera un valor entero del almacenamiento persistente
 * 
 * @param key Clave del valor a recuperar
 * @param default_value Valor por defecto si la clave no existe
 * @return El valor recuperado o default_value si la clave no existe
 */
int32_t nvs_manager_get_int(const char* key, int32_t default_value);

/**
 * @brief Guarda un valor string en el almacenamiento persistente
 * 
 * @param key Clave bajo la cual se guarda el valor
 * @param value String a guardar
 * @return ESP_OK si la operación es exitosa, código de error en caso contrario
 */
esp_err_t nvs_manager_set_string(const char* key, const char* value);

/**
 * @brief Recupera un valor string del almacenamiento persistente
 * 
 * @param key Clave del valor a recuperar
 * @param value Buffer donde se almacenará el string
 * @param max_length Tamaño máximo del buffer
 * @return ESP_OK si la operación es exitosa, código de error en caso contrario
 */
esp_err_t nvs_manager_get_string(const char* key, char* value, size_t max_length);

/**
 * @brief Guarda un valor float en el almacenamiento persistente
 * 
 * @param key Clave bajo la cual se guarda el valor
 * @param value Valor a guardar
 * @return ESP_OK si la operación es exitosa, código de error en caso contrario
 */
esp_err_t nvs_manager_set_float(const char* key, float value);

/**
 * @brief Recupera un valor float del almacenamiento persistente
 * 
 * @param key Clave del valor a recuperar
 * @param default_value Valor por defecto si la clave no existe
 * @return El valor recuperado o default_value si la clave no existe
 */
float nvs_manager_get_float(const char* key, float default_value);

/**
 * @brief Guarda un array de bytes en el almacenamiento persistente
 * 
 * @param key Clave bajo la cual se guarda el valor
 * @param data Puntero a los datos a guardar
 * @param length Longitud en bytes de los datos
 * @return ESP_OK si la operación es exitosa, código de error en caso contrario
 */
esp_err_t nvs_manager_set_blob(const char* key, const void* data, size_t length);

/**
 * @brief Recupera un array de bytes del almacenamiento persistente
 * 
 * @param key Clave del valor a recuperar
 * @param data Buffer donde se almacenarán los datos
 * @param length Puntero que al ingresar contiene el tamaño del buffer, 
 *               al salir contiene la cantidad de bytes leídos
 * @return ESP_OK si la operación es exitosa, código de error en caso contrario
 */
esp_err_t nvs_manager_get_blob(const char* key, void* data, size_t* length);

/**
 * @brief Comprueba si una clave existe en el almacenamiento
 * 
 * @param key Clave a comprobar
 * @return true si la clave existe, false en caso contrario
 */
bool nvs_manager_key_exists(const char* key);

/**
 * @brief Elimina una clave del almacenamiento
 * 
 * @param key Clave a eliminar
 * @return ESP_OK si la operación es exitosa, código de error en caso contrario
 */
esp_err_t nvs_manager_erase_key(const char* key);

/**
 * @brief Borra todas las claves del namespace actual
 * 
 * @return ESP_OK si la operación es exitosa, código de error en caso contrario
 */
esp_err_t nvs_manager_erase_all(void);

/**
 * @brief Guarda un valor booleano en NVS
 * 
 * @param key Clave bajo la que se guardará el valor
 * @param value Valor booleano a guardar
 * @return esp_err_t ESP_OK en caso de éxito
 */
static inline esp_err_t nvs_manager_set_bool(const char* key, bool value) {
    return nvs_manager_set_int(key, value ? 1 : 0);
}

/**
 * @brief Obtiene un valor booleano de NVS
 * 
 * @param key Clave a buscar
 * @param default_value Valor por defecto si la clave no existe
 * @return bool Valor booleano guardado o default_value si no existe
 */
static inline bool nvs_manager_get_bool(const char* key, bool default_value) {
    return nvs_manager_get_int(key, default_value ? 1 : 0) != 0;
}

/**
 * @brief Guarda un valor uint8_t en NVS
 * 
 * @param key Clave bajo la que se guardará el valor
 * @param value Valor uint8_t a guardar
 * @return esp_err_t ESP_OK en caso de éxito
 */
static inline esp_err_t nvs_manager_set_u8(const char* key, uint8_t value) {
    return nvs_manager_set_int(key, (int32_t)value);
}

/**
 * @brief Obtiene un valor uint8_t de NVS
 * 
 * @param key Clave a buscar
 * @param default_value Valor por defecto si la clave no existe
 * @return uint8_t Valor guardado o default_value si no existe
 */
static inline uint8_t nvs_manager_get_u8(const char* key, uint8_t default_value) {
    return (uint8_t)nvs_manager_get_int(key, default_value);
}

/**
 * @brief Guardar dirección MAC BLE en NVS
 * 
 * @param mac Cadena con formato AA:BB:CC:DD:EE:FF
 * @return esp_err_t ESP_OK si se guardó correctamente
 */
esp_err_t nvs_manager_save_mac(const char *mac);

#ifdef __cplusplus
}
#endif

#endif // NVS_MANAGER_H
