#include "nvs_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#include <inttypes.h> // Añadido para los especificadores de formato PRId32

static const char *TAG = "NVS_MANAGER";
static char storage_namespace[16] = "ecokey"; // Valor por defecto
static bool is_initialized = false;

// Claves NVS para WiFi
#define WIFI_NVS_SSID_KEY "ssid"
#define WIFI_NVS_PASS_KEY "password"

esp_err_t nvs_manager_init(const char* namespace)
{
    // Si se proporciona un namespace válido, lo usamos
    if (namespace && strlen(namespace) > 0) {
        strncpy(storage_namespace, namespace, sizeof(storage_namespace) - 1);
        storage_namespace[sizeof(storage_namespace) - 1] = '\0'; // Asegurar terminación null
    }
    
    ESP_LOGI(TAG, "Inicializando NVS con namespace: %s", storage_namespace);
    
    // Inicializar el almacenamiento no volátil (NVS)
    esp_err_t ret = nvs_flash_init();
    
    // Si hay problemas con NVS, lo borramos y reinicializamos
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "NVS requiere borrado, reinicializando...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar NVS: %s", esp_err_to_name(ret));
        is_initialized = false;
    } else {
        ESP_LOGI(TAG, "NVS inicializado correctamente");
        is_initialized = true;
    }
    
    return ret;
}

bool nvs_manager_is_initialized(void)
{
    return is_initialized;
}

esp_err_t nvs_manager_set_int(const char* key, int32_t value)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado. Llame a nvs_manager_init primero");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (key == NULL) {
        ESP_LOGE(TAG, "Clave no válida (NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t handle;
    esp_err_t ret;
    
    ret = nvs_open(storage_namespace, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al abrir NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_set_i32(handle, key, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al guardar valor '%s': %s", key, esp_err_to_name(ret));
    } else {
        // Confirmar los cambios de manera explícita
        ESP_LOGI(TAG, "Haciendo commit de los cambios para la clave '%s' con valor %" PRId32, key, value);
        ret = nvs_commit(handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error en nvs_commit: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Commit exitoso para la clave '%s'", key);
        }
    }
    
    nvs_close(handle);
    return ret;
}

int32_t nvs_manager_get_int(const char* key, int32_t default_value)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado. Llame a nvs_manager_init primero");
        return default_value;
    }
    
    if (key == NULL) {
        ESP_LOGE(TAG, "Clave no válida (NULL)");
        return default_value;
    }
    
    nvs_handle_t handle;
    esp_err_t ret;
    int32_t value = default_value;
    
    ret = nvs_open(storage_namespace, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al abrir NVS: %s", esp_err_to_name(ret));
        return default_value;
    }
    
    ret = nvs_get_i32(handle, key, &value);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Clave '%s' no encontrada, usando valor por defecto", key);
        value = default_value;
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al leer valor '%s': %s", key, esp_err_to_name(ret));
    }
    
    nvs_close(handle);
    return value;
}

esp_err_t nvs_manager_set_string(const char* key, const char* value)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado. Llame a nvs_manager_init primero");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (key == NULL || value == NULL) {
        ESP_LOGE(TAG, "Parámetros no válidos (NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t handle;
    esp_err_t ret;
    
    ret = nvs_open(storage_namespace, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al abrir NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_set_str(handle, key, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al guardar string '%s': %s", key, esp_err_to_name(ret));
    } else {
        // Confirmar los cambios
        ret = nvs_commit(handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error en nvs_commit: %s", esp_err_to_name(ret));
        }
    }
    
    nvs_close(handle);
    return ret;
}

esp_err_t nvs_manager_get_string(const char* key, char* value, size_t max_length)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado. Llame a nvs_manager_init primero");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (key == NULL || value == NULL || max_length == 0) {
        ESP_LOGE(TAG, "Parámetros no válidos");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t handle;
    esp_err_t ret;
    
    ret = nvs_open(storage_namespace, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al abrir NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    size_t required_size = 0;
    ret = nvs_get_str(handle, key, NULL, &required_size);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Clave '%s' no encontrada", key);
        nvs_close(handle);
        return ESP_ERR_NVS_NOT_FOUND;
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al obtener tamaño para '%s': %s", key, esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }
    
    if (required_size > max_length) {
        ESP_LOGE(TAG, "Buffer demasiado pequeño para '%s'. Se requiere %d bytes, disponible %d", 
                 key, required_size, max_length);
        nvs_close(handle);
        return ESP_ERR_INVALID_SIZE;
    }
    
    ret = nvs_get_str(handle, key, value, &max_length);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al leer string '%s': %s", key, esp_err_to_name(ret));
    }
    
    nvs_close(handle);
    return ret;
}

esp_err_t nvs_manager_set_float(const char* key, float value)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado. Llame a nvs_manager_init primero");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (key == NULL) {
        ESP_LOGE(TAG, "Clave no válida (NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    
    // NVS no tiene funciones específicas para float, así que lo convertimos a un blob
    return nvs_manager_set_blob(key, &value, sizeof(float));
}

float nvs_manager_get_float(const char* key, float default_value)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado. Llame a nvs_manager_init primero");
        return default_value;
    }
    
    if (key == NULL) {
        ESP_LOGE(TAG, "Clave no válida (NULL)");
        return default_value;
    }
    
    float value = default_value;
    size_t size = sizeof(float);
    
    esp_err_t ret = nvs_manager_get_blob(key, &value, &size);
    
    if (ret != ESP_OK || size != sizeof(float)) {
        return default_value;
    }
    
    return value;
}

esp_err_t nvs_manager_set_blob(const char* key, const void* data, size_t length)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado. Llame a nvs_manager_init primero");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (key == NULL || data == NULL || length == 0) {
        ESP_LOGE(TAG, "Parámetros no válidos");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t handle;
    esp_err_t ret;
    
    ret = nvs_open(storage_namespace, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al abrir NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_set_blob(handle, key, data, length);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al guardar blob '%s': %s", key, esp_err_to_name(ret));
    } else {
        // Confirmar los cambios
        ret = nvs_commit(handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error en nvs_commit: %s", esp_err_to_name(ret));
        }
    }
    
    nvs_close(handle);
    return ret;
}

esp_err_t nvs_manager_get_blob(const char* key, void* data, size_t* length)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado. Llame a nvs_manager_init primero");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (key == NULL || data == NULL || length == NULL || *length == 0) {
        ESP_LOGE(TAG, "Parámetros no válidos");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t handle;
    esp_err_t ret;
    
    ret = nvs_open(storage_namespace, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al abrir NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    size_t required_size = 0;
    ret = nvs_get_blob(handle, key, NULL, &required_size);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Clave '%s' no encontrada", key);
        nvs_close(handle);
        return ESP_ERR_NVS_NOT_FOUND;
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al obtener tamaño para '%s': %s", key, esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }
    
    if (required_size > *length) {
        ESP_LOGE(TAG, "Buffer demasiado pequeño para '%s'. Se requiere %d bytes, disponible %d", 
                 key, required_size, *length);
        nvs_close(handle);
        return ESP_ERR_INVALID_SIZE;
    }
    
    *length = required_size;
    ret = nvs_get_blob(handle, key, data, length);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al leer blob '%s': %s", key, esp_err_to_name(ret));
    }
    
    nvs_close(handle);
    return ret;
}

bool nvs_manager_key_exists(const char* key)
{
    if (!is_initialized || key == NULL) {
        ESP_LOGW(TAG, "NVS no inicializado o clave nula, key_exists retorna false");
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t ret;
    
    ret = nvs_open(storage_namespace, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al abrir NVS para verificar clave: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Verificar diferentes tipos de datos
    bool existe = false;
    
    // Probar como entero
    int32_t int_value;
    ret = nvs_get_i32(handle, key, &int_value);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "La clave '%s' existe en NVS como entero", key);
        existe = true;
    } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(TAG, "Error al verificar clave '%s' como entero: %s", key, esp_err_to_name(ret));
    }
    
    // Si no existe como entero, probar como string
    if (!existe) {
        size_t required_size = 0;
        ret = nvs_get_str(handle, key, NULL, &required_size);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "La clave '%s' existe en NVS como string", key);
            existe = true;
        } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGD(TAG, "Error al verificar clave '%s' como string: %s", key, esp_err_to_name(ret));
        }
    }
    
    // Si no existe como string, probar como blob
    if (!existe) {
        size_t required_size = 0;
        ret = nvs_get_blob(handle, key, NULL, &required_size);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "La clave '%s' existe en NVS como blob", key);
            existe = true;
        } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGD(TAG, "Error al verificar clave '%s' como blob: %s", key, esp_err_to_name(ret));
        }
    }
    
    nvs_close(handle);
    
    if (!existe) {
        ESP_LOGI(TAG, "La clave '%s' NO existe en NVS", key);
    }
    
    return existe;
}

esp_err_t nvs_manager_erase_key(const char* key)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado. Llame a nvs_manager_init primero");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (key == NULL) {
        ESP_LOGE(TAG, "Clave no válida (NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t handle;
    esp_err_t ret;
    
    ret = nvs_open(storage_namespace, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al abrir NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_erase_key(handle, key);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Clave '%s' no encontrada para borrar", key);
        nvs_close(handle);
        return ESP_OK; // No es un error si la clave no existe
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al borrar clave '%s': %s", key, esp_err_to_name(ret));
    } else {
        // Confirmar los cambios
        ret = nvs_commit(handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error en nvs_commit: %s", esp_err_to_name(ret));
        }
    }
    
    nvs_close(handle);
    return ret;
}

esp_err_t nvs_manager_erase_all(void)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado. Llame a nvs_manager_init primero");
        return ESP_ERR_INVALID_STATE;
    }
    
    nvs_handle_t handle;
    esp_err_t ret;
    
    ret = nvs_open(storage_namespace, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al abrir NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = nvs_erase_all(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al borrar todas las claves: %s", esp_err_to_name(ret));
    } else {
        // Confirmar los cambios
        ret = nvs_commit(handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error en nvs_commit: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Todas las claves borradas correctamente");
        }
    }
    
    nvs_close(handle);
    return ret;
}

esp_err_t nvs_manager_save_mac(const char *mac)
{
    if (!mac || strlen(mac) < 17) {  // MAC formato AA:BB:CC:DD:EE:FF tiene 17 caracteres
        ESP_LOGE(TAG, "Formato MAC inválido");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(storage_namespace, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error abriendo NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(nvs_handle, "ble_mac", mac);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error guardando MAC: %s", esp_err_to_name(err));
    } else {
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error en commit: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "MAC guardada correctamente: %s", mac);
        }
    }

    nvs_close(nvs_handle);
    return err;
}

bool nvs_manager_has_wifi_credentials(void)
{
    if (!is_initialized) {
        ESP_LOGW(TAG, "NVS no inicializado");
        return false;
    }
    
    // Verificar si existe el SSID en NVS (requisito mínimo)
    if (!nvs_manager_key_exists(WIFI_NVS_SSID_KEY)) {
        ESP_LOGD(TAG, "No hay SSID almacenado en NVS");
        return false;
    }
    
    ESP_LOGI(TAG, "Credenciales WiFi encontradas en NVS");
    return true;
}

esp_err_t nvs_manager_get_wifi_credentials(char *ssid, size_t ssid_len, 
                                          char *password, size_t password_len)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!ssid || ssid_len < 33 || !password || password_len < 65) {
        ESP_LOGE(TAG, "Parámetros inválidos para obtener credenciales WiFi");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Verificar si existe el SSID
    if (!nvs_manager_key_exists(WIFI_NVS_SSID_KEY)) {
        ESP_LOGW(TAG, "No hay SSID almacenado en NVS");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Inicializar buffers
    memset(ssid, 0, ssid_len);
    memset(password, 0, password_len);
    
    // Obtener SSID
    esp_err_t ret = nvs_manager_get_string(WIFI_NVS_SSID_KEY, ssid, ssid_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al leer SSID de NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Obtener password si existe (opcional)
    if (nvs_manager_key_exists(WIFI_NVS_PASS_KEY)) {
        ret = nvs_manager_get_string(WIFI_NVS_PASS_KEY, password, password_len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error al leer contraseña de NVS: %s", esp_err_to_name(ret));
            // No es error crítico, podría ser una red abierta
        }
    }
    
    ESP_LOGI(TAG, "Credenciales WiFi obtenidas correctamente. SSID: %s", ssid);
    return ESP_OK;
}

esp_err_t nvs_manager_save_wifi_credentials(const char *ssid, const char *password)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!ssid || strlen(ssid) == 0) {
        ESP_LOGE(TAG, "SSID no válido");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Guardar SSID
    esp_err_t ret = nvs_manager_set_string(WIFI_NVS_SSID_KEY, ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al guardar SSID en NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Guardar password si existe
    if (password && strlen(password) > 0) {
        ret = nvs_manager_set_string(WIFI_NVS_PASS_KEY, password);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error al guardar contraseña en NVS: %s", esp_err_to_name(ret));
            return ret;
        }
    } else {
        // Si no hay password, eliminar la clave si existía
        if (nvs_manager_key_exists(WIFI_NVS_PASS_KEY)) {
            ret = nvs_manager_erase_key(WIFI_NVS_PASS_KEY);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Error al borrar contraseña anterior: %s", esp_err_to_name(ret));
                // No es un error crítico
            }
        }
    }
    
    ESP_LOGI(TAG, "Credenciales WiFi guardadas. SSID: %s", ssid);
    return ESP_OK;
}

esp_err_t nvs_manager_delete_wifi_credentials(void)
{
    if (!is_initialized) {
        ESP_LOGE(TAG, "NVS no inicializado");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = ESP_OK;
    bool error = false;
    
    // Eliminar SSID si existe
    if (nvs_manager_key_exists(WIFI_NVS_SSID_KEY)) {
        ret = nvs_manager_erase_key(WIFI_NVS_SSID_KEY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error al borrar SSID: %s", esp_err_to_name(ret));
            error = true;
        }
    }
    
    // Eliminar password si existe
    if (nvs_manager_key_exists(WIFI_NVS_PASS_KEY)) {
        esp_err_t ret2 = nvs_manager_erase_key(WIFI_NVS_PASS_KEY);
        if (ret2 != ESP_OK) {
            ESP_LOGE(TAG, "Error al borrar contraseña: %s", esp_err_to_name(ret2));
            error = true;
            if (ret == ESP_OK) {
                ret = ret2; // Preservar el primer error si hubo
            }
        }
    }
    
    if (!error) {
        ESP_LOGI(TAG, "Credenciales WiFi eliminadas correctamente");
    }
    
    return ret;
}


