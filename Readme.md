# Proyecto EcoKey Firmware

![EcoKey Logo](assets/logo.png) <!-- Sugerencia: agregar un logo -->

## Índice
- [Descripción General](#descripción-general)
- [Características Principales](#características-principales)
- [Estructura del Proyecto](#estructura-del-proyecto)
- [Componentes del Sistema](#componentes-del-sistema)
- [Flujo de Operación](#flujo-de-operación)
- [Configuración del Entorno](#configuración-del-entorno-de-desarrollo)
- [Instalación y Uso](#instalación-y-uso)
- [Configuración del Hardware](#configuración-del-hardware)
- [Guía de Usuario](#guía-de-usuario)
- [Solución de Problemas](#solución-de-problemas)
- [Contribución y Desarrollo](#contribución-y-desarrollo)
- [Licencia](#licencia)
- [Contacto](#contacto)

## Descripción General

EcoKey Firmware es un sistema de control energético inteligente basado en ESP32-S3 que optimiza el consumo eléctrico mediante la detección de dispositivos Bluetooth. El sistema activa o desactiva automáticamente los relés conectados según la presencia de dispositivos BLE específicos, reduciendo el consumo energético innecesario y aumentando la eficiencia.

![Diagrama Conceptual](assets/diagrama_conceptual.png) <!-- Sugerencia: agregar un diagrama conceptual -->

## Características Principales

- **Detección BLE inteligente**: Escaneo eficiente de dispositivos Bluetooth Low Energy mediante direcciones MAC configurables
- **Control automático adaptativo**: Activación/desactivación de relés basada en algoritmos de presencia
- **Modos de operación flexibles**:
  - Modo automático (basado en presencia BLE)
  - Modo manual (control directo)
  - Modo configuración (interfaz web)
- **Almacenamiento persistente**: Configuraciones guardadas en memoria no volátil (NVS)
- **Configuración WiFi simplificada**: Portal cautivo intuitivo
- **Interfaz visual**: Sistema de indicadores LED para estado del dispositivo
- **Control físico**: Botones multifunción para operación sin smartphone
- **Temporizadores personalizables**: Ajuste preciso de tiempos de espera

## Estructura del Proyecto
El proyecto está organizado siguiendo la estructura estándar de ESP-IDF con componentes modulares:

```
Ecokey_Firmware/
├── .devcontainer/       # Configuración para desarrollo en contenedores
├── .vscode/             # Configuración de Visual Studio Code
├── components/          # Componentes modulares del firmware
│   ├── app_control/     # Gestión de estados de la aplicación
│   ├── app_inicializacion/ # Inicialización de componentes
│   ├── ble_scanner/     # Escáner de dispositivos Bluetooth
│   ├── button/          # Manejo de botones físicos
│   ├── control_button/  # Control de acciones por botones
│   ├── estado_automatico/ # Lógica del modo automático
│   ├── estado_configuracion/ # Lógica del modo configuración
│   ├── estado_manual/   # Lógica del modo manual
│   ├── led/             # Control de indicadores LED
│   ├── nvs_manager/     # Gestión de almacenamiento no volátil
│   ├── relay_controller/ # Control de relés
│   ├── wifi_provision_web/ # Portal cautivo para configuración WiFi
│   └── wifi_sta/        # Gestión de conexión WiFi como estación
├── main/                # Punto de entrada principal
│   ├── CMakeLists.txt
│   └── main.c           # Código principal de la aplicación
├── build/               # Directorio de compilación (generado)
├── CMakeLists.txt       # Configuración del sistema de compilación
├── sdkconfig            # Configuración del SDK
├── sdkconfig.old        # Copia de seguridad de configuración anterior
├── partitions.csv       # Tabla de particiones de la flash
└── README.md            # Este archivo
```

## Componentes del Sistema

### BLE Scanner (ble_scanner)
Encargado de escanear dispositivos BLE cercanos y detectar MAC addresses específicas configuradas por el usuario.

### Control de Aplicación (app_control)
Maneja los estados de la aplicación y las transiciones entre los diferentes modos de operación:

- Modo Automático
- Modo Manual
- Modo Configuración

### Control de Relés (relay_controller)
Gestiona la activación y desactivación de los relés que controlan los dispositivos eléctricos.

### Gestor NVS (nvs_manager)
Maneja el almacenamiento persistente de configuraciones en la memoria no volátil.

### Provisioning WiFi (wifi_provision_web)
Implementa un portal cautivo para la configuración de WiFi y parámetros del dispositivo.

### Control de Botones (control_button)
Maneja las pulsaciones de botones físicos para interactuar con el dispositivo:

- Pulsación corta: Cambio entre modos
- Pulsación larga: Entrar en modo configuración
- Pulsación muy larga: Acciones alternativas
- Pulsación reset: Restaurar a valores de fábrica

### Estado Automático (estado_automatico)
Implementa la lógica para el modo automático, incluyendo temporizadores y detección de dispositivos.

## Flujo de Operación

1. **Inicialización**: El sistema inicia y carga la configuración almacenada en NVS.
2. **Modo Automático**: El sistema escanea dispositivos BLE. Si detecta una MAC configurada, activa el relé; si no la detecta durante el tiempo configurado, lo desactiva.
3. **Modo Manual**: El usuario puede controlar directamente el estado del relé.
4. **Modo Configuración**: Activa un punto de acceso WiFi con portal cautivo para permitir la configuración de:
   - Dirección MAC objetivo para detección
   - Tiempo del temporizador de ausencia
   - Configuración de red WiFi

## Configuración del Entorno de Desarrollo

### Requisitos
- ESP-IDF v5.4.1 o superior
- Python 3.7 o superior
- Visual Studio Code (recomendado) con extensión de Espressif

### Preparación del Entorno
1. Instalar ESP-IDF siguiendo las instrucciones oficiales
2. Clonar este repositorio:

```sh
git clone https://github.com/tu_usuario/EcoKey_Firmware.git
cd EcoKey_Firmware
```

## Instalación y Uso

### Compilación y Flasheo
Para compilar el proyecto:

```sh
idf.py build
```

Para flashear el firmware en el dispositivo:

```sh
idf.py -p COM3 flash
```

Para monitorear la salida del dispositivo:

```sh
idf.py -p COM3 monitor
```

O para flashear y monitorear en un solo comando:

```sh
idf.py -p COM3 flash monitor
```

## Configuración del Hardware
El proyecto está diseñado para funcionar con hardware basado en ESP32-S3 con:

- Botón de control
- LED de estado
- Relé para control de dispositivos
- Antena WiFi y Bluetooth

## Guía de Usuario

### Primera Configuración
1. Al iniciar por primera vez, el dispositivo entrará en modo configuración
2. Conectarse a la red WiFi "EcoKey-Setup" desde un teléfono o computadora
3. Se abrirá automáticamente un portal cautivo (o navegar a http://192.168.4.1)
4. Seguir los pasos para configurar:
   - Dirección MAC del dispositivo a detectar
   - Tiempo de espera para desactivación
   - Credenciales de red WiFi

### Operación Normal
- **LED parpadeando lentamente**: Modo automático, buscando dispositivos
- **LED encendido fijo**: Relé activado
- **LED apagado**: Relé desactivado
- **LED parpadeando rápidamente**: Modo configuración activo

### Interacción con Botones
- **Pulsación corta**: Cambiar entre modo automático y manual
- **Pulsación larga**: Entrar en modo configuración
- **Pulsación muy larga**: Funciones avanzadas (según configuración)
- **Pulsación reset**: Restaurar valores de fábrica (mantener presionado durante el reinicio)

## Personalización
El sistema puede personalizarse modificando los siguientes parámetros:

- Tiempo máximo de temporizador (por defecto: 30 minutos)
- Comportamiento de los LED
- Lógica de control de relés
- Interfaz web de configuración

## Solución de Problemas

### El dispositivo no detecta las MACs configuradas
- Verificar que el formato de MAC sea correcto (AA:BB:CC:DD:EE:FF)
- Asegurar que el dispositivo Bluetooth esté encendido y visible
- Comprobar que la distancia no sea excesiva

### No se puede acceder al portal de configuración
- Reiniciar el dispositivo
- Verificar que está en modo configuración (LED parpadeando rápidamente)
- Conectarse a la red WiFi "EcoKey-Setup"
- Intentar acceder manualmente a http://192.168.4.1

### El relé no se activa/desactiva correctamente
- Verificar conexiones físicas del relé
- Comprobar el estado actual del dispositivo
- Reiniciar a valores de fábrica y reconfigurarlo

## Contribución y Desarrollo
Para contribuir al proyecto:

1. Hacer fork del repositorio
2. Crear una rama para tu característica (`git checkout -b feature/nueva-caracteristica`)
3. Hacer commit de tus cambios (`git commit -am 'Añadir nueva característica'`)
4. Subir la rama (`git push origin feature/nueva-caracteristica`)
5. Crear un Pull Request

## Licencia
Este proyecto está bajo licencia [Licencia] - Ver archivo LICENSE para más detalles.

## Contacto
Para más información sobre EcoKey y sus soluciones de control energético inteligente:

- **Email**: contacto@ecokey.com
- **Web**: www.ecokey.com

EcoKey © 2024 - Control energético inteligente