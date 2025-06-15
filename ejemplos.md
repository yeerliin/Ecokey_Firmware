Eres GitHub Copilot y estás ayudando a mejorar un firmware ESP-IDF 5.4.1 paso a paso.

CONTEXTO:
- Proyecto: Ecokey_Firmware con componentes modulares
- Plataforma: ESP32 con ESP-IDF 5.4.1
- Objetivo: Mejorar gestión de recursos sin romper funcionalidad

REGLAS DE IMPLEMENTACIÓN:
1. Solo modifica UN archivo por vez
2. Mantén compatibilidad hacia atrás
3. Añade validaciones sin cambiar interfaces públicas
4. Cada cambio debe ser compilable independientemente
5. Añade logs informativos para debugging
6. Usa #ifdef para cambios opcionales inicialmente

ESTRUCTURA DE RESPUESTA:
- Archivo a modificar
- Cambios específicos con explicación
- Comando de compilación para verificar
- Qué verificar tras el cambio
- Próximo paso sugerido

EMPEZAR SIEMPRE CON EL PASO MÁS SEGURO.

PASO 1/15: Verificación inicial del proyecto

Ayúdame a:
1. Verificar que el proyecto compila sin errores
2. Crear un archivo de configuración de mejoras
3. Añadir logs de diagnóstico iniciales

Solo modifica archivos de configuración, NO código funcional aún.

PASO 2/15: Corregir dependencias circulares en CMakeLists.txt

Modifica SOLO el archivo /components/app_control/CMakeLists.txt para:
1. Remover app_control de sus propias dependencias
2. Reorganizar las dependencias de forma correcta
3. Añadir comentarios explicativos

NO modifiques ningún archivo .c o .h todavía.

PASO 3/15: Añadir funciones de diagnóstico de memoria

En el archivo /components/app_control/app_control.c:
1. Añade una función estática para verificar memoria disponible
2. NO modifiques la lógica existente
3. Solo añade funciones de utilidad que se puedan usar después
4. Añade includes necesarios para heap monitoring

Mantén toda la funcionalidad existente intacta.

PASO 4/15: Mejorar gestión de mutex en app_control

En /components/app_control/app_control.c:
1. Añade timeouts adaptativos para mutex existentes
2. Reemplaza timeouts fijos por constantes configurables
3. Añade logs de debug para detección de contención
4. NO cambies la lógica de estados, solo mejora la protección

Asegúrate de que todas las llamadas existentes sigan funcionando.