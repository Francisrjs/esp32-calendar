# SmartClock Terminal · ESP32-S3-RLCD-4.2

La documentación y el estado del plan están en [docs/README.md](docs/README.md) y [docs/implementation-plan.md](docs/implementation-plan.md).

## Vista de diseño en PC

Abrí preview/index.html para recorrer las ocho vistas visuales. La interacción del navegador sigue siendo una maqueta: no controla el dispositivo ni representa una sesión real de Calendar o Spotify. preview/pair.html permite iniciar sesión, emitir un código temporal y comenzar OAuth de Google una vez configurados los secretos del servidor.

## Firmware en la placa

Usá la guía [Compilación y carga](docs/modules/build-upload.md). El firmware implementa navegación con KEY, hora desde PCF85063/NTP, lectura SHTC3, Wi-Fi, pairing con Supabase y sincronización/completado de tareas. La URL Supabase, publishable key y CA deben configurarse localmente; nunca se embeben claves service_role u OAuth de proveedores.

Audio ES7210/ES8311, voz, alarmas audibles, Spotify, SD y operación OAuth real aún requieren configuración o implementación adicional. El firmware está cargado en COM4: se verificaron Wi-Fi, NTP/RTC, mediciones SHTC3, HTTPS y pairing/snapshot de Supabase. El usuario confirmó que la hora y el calendario ya se ven correctamente en la placa.

Para regenerar los recursos desde el PDF, ejecutá python generate_assets.py con Pillow y Poppler disponibles.
