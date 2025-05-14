# Ejemplos de Aplicaciones de Aprovisionamiento

Esto consiste principalmente en un único ejemplo unificado wifi_prov_mgr

* wifi_prov_mgr
    Abstrae la mayor parte de la complejidad del aprovisionamiento Wi-Fi y permite cambiar fácilmente entre los transportes SoftAP (usando HTTP) y BLE. También demuestra cómo las aplicaciones pueden registrar y usar endpoints de datos personalizados adicionales.

Las aplicaciones de aprovisionamiento están disponibles para varias plataformas:

* Android:
    - [Aplicación de aprovisionamiento BLE en Play Store](https://play.google.com/store/apps/details?id=com.espressif.provble).
    - [Aplicación de aprovisionamiento SoftAP en Play Store](https://play.google.com/store/apps/details?id=com.espressif.provsoftap).
    - Código fuente en GitHub: [esp-idf-provisioning-android](https://github.com/espressif/esp-idf-provisioning-android).
* iOS:
    - [Aplicación de aprovisionamiento BLE en App Store](https://apps.apple.com/in/app/esp-ble-provisioning/id1473590141)
    - [Aplicación de aprovisionamiento SoftAP en App Store](https://apps.apple.com/in/app/esp-softap-provisioning/id1474040630)
    - Código fuente en GitHub: [esp-idf-provisioning-ios](https://github.com/espressif/esp-idf-provisioning-ios)
* Para todas las demás plataformas, se proporciona una herramienta de línea de comandos basada en Python en "$IDF_PATH/tools/esp_prov"

Las aplicaciones de aprovisionamiento para Android e iOS permiten al usuario configurar el dispositivo manualmente o escaneando un código QR. Los códigos QR pueden generarse con cualquier generador de códigos QR en línea. La carga útil del código QR se codifica con una cadena JSON que contiene el nombre del dispositivo, la clave de prueba de posesión (si se utiliza) y el tipo de transporte (BLE o softAP), por ejemplo:

```
{"ver":"v1","name":"PROV_000318","pop":"a1000318","transport":"softap"}
```

Para más detalles sobre el formato del código QR, puedes consultar [Escaneo de Código QR](https://github.com/espressif/esp-idf-provisioning-android#qr-code-scan).
