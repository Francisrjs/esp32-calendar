# Documentación del asistente ESP32

Proyecto para la placa Waveshare ESP32-S3-RLCD-4.2 con pantalla monocroma de 400 × 300.

## Estado

El firmware ya navega las ocho vistas, obtiene hora del RTC PCF85063 (y NTP cuando hay Wi-Fi), mide SHTC3, configura Wi-Fi, se empareja con Supabase y sincroniza/completa tareas. El backend y las tablas separadas esp32_* están desplegados en el proyecto Supabase existente.

El firmware está cargado en COM4, conectado a Wi-Fi y emparejado con la cuenta Auth solicitada. Se comprobaron hora NTP/RTC válida, mediciones SHTC3 y respuestas HTTPS/snapshot de Supabase. Se corrigió el recorte de los glifos; el calendario mensual ahora se muestra con la fecha local aunque Google todavía no esté vinculado. El usuario confirmó la legibilidad de hora y calendario y su navegación con KEY. La captura y reproducción de audio, alarmas audibles, IA, Spotify y respaldo en SD no están implementados. Los eventos Google requieren secretos OAuth del servidor y consentimiento. La [vista previa](../preview/index.html) sigue usando datos simulados.

## Guías

- [Plan de implementación y estado](implementation-plan.md)
- [Pantallas y navegación](screen/README.md)
- [Asistente de IA](ai/README.md)
- [Módulos](modules/README.md)
- [Compilación, conexión y pairing](modules/build-upload.md)
- [Esquema inicial](database/initial-schema.sql)
