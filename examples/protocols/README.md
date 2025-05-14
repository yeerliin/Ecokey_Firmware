# Ejemplos de Protocolos

Implementación de protocolos y servicios de comunicación por Internet.

Consulta el archivo [README.md](../README.md) en el directorio [examples](../) de nivel superior para obtener más información sobre los ejemplos.

## Establecimiento de Conexión Wi-Fi o Ethernet

### Acerca de la Función `example_connect()`

Los ejemplos de protocolos utilizan una función auxiliar simple, `example_connect()`, para establecer la conexión Wi-Fi y/o Ethernet. Esta función se implementa en [examples/common_components/protocol_examples_common/include/protocol_examples_common.h](../common_components/protocol_examples_common/include/protocol_examples_common.h), y tiene un comportamiento muy simple: bloquea hasta que se establece la conexión y se obtiene la dirección IP, luego retorna. Esta función se utiliza para reducir la cantidad de código repetitivo y para mantener el código de ejemplo centrado en el protocolo o biblioteca que se está demostrando.

La función simple `example_connect()` no maneja tiempos de espera, no maneja con elegancia varias condiciones de error, y solo es adecuada para su uso en ejemplos. Al desarrollar aplicaciones reales, esta función auxiliar debe ser reemplazada por un código completo de manejo de conexiones Wi-Fi / Ethernet. Dicho código se puede encontrar en los ejemplos [examples/wifi/getting_started/](../wifi/getting_started) y [examples/ethernet/basic/](../ethernet/basic).

### Configuración del Ejemplo

Para configurar el ejemplo para usar Wi-Fi, Ethernet o ambas conexiones, abre el menú de configuración del proyecto (`idf.py menuconfig`) y navega al menú "Example Connection Configuration". Selecciona "Wi-Fi" o "Ethernet" o ambos en la opción "Connect using".

Al conectar usando Wi-Fi, ingresa el SSID y la contraseña de tu punto de acceso Wi-Fi en los campos correspondientes. Si te conectas a una red Wi-Fi abierta, mantén el campo de contraseña vacío.

Al conectar usando Ethernet, configura el tipo de PHY y la configuración en los campos proporcionados. Si usas Ethernet por primera vez, se recomienda comenzar con el [readme del ejemplo de Ethernet](../ethernet/basic/README.md), que contiene instrucciones para conectar y configurar el PHY. Una vez que el ejemplo de Ethernet obtiene la dirección IP con éxito, procede al ejemplo de protocolos y establece las mismas opciones de configuración.

### Desactivación de IPv6

Por defecto, la función `example_connect()` espera hasta que se establezca la conexión Wi-Fi o Ethernet, y se obtenga la dirección IPv4 y la dirección link-local IPv6. En entornos de red donde no se puede obtener la dirección link-local IPv6, desactiva la opción "Obtain IPv6 link-local address" que se encuentra en el menú "Example Connection Configuration".
