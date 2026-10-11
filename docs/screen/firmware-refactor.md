# Vistas dinámicas y voz · firmware

Implementado el 10 de octubre de 2026 para Waveshare ESP32-S3-RLCD-4.2 N16R8. Después del reporte de activación de voz, se confirmó por USB que seguía instalado el firmware antiguo. Se cargó `views-v2` en COM4, con hash verificado, y se confirmó Inicio, Wi-Fi, RTC y pairing conservados. Legibilidad física pendiente.

## Archivos y responsabilidades

- `firmware/src/ui_model.*`: datos acotados a 50 elementos por lista e importación JSON. Acepta el contrato V2 de `preview/handoff` y el snapshot actual del servicio. Las tareas pendientes preceden a las completadas. Campos ausentes conservan el último valor disponible.
- `firmware/src/ui_renderer.*`: ocho vistas dinámicas, calendario de lunes a domingo con seis semanas, estados vacíos, mascota monocroma recortada y fuentes de 12, 18 y 40 px con acentos y ñ. Ya no se usan bitmaps completos para dibujar texto.
- `firmware/src/main.cpp`: navegación, hora ART3, reposo, importación USB, caché NVS y aplicación de resultados. Sólo este hilo usa pantalla, NVS e I²C después del arranque.
- `firmware/src/network_worker.*`: HTTPS en una tarea separada; entradas copiadas y resultados por cola. KEY sigue operativo durante transcripción, sincronización y pairing. El resultado de voz no cambia la vista si el usuario salió de escucha.
- `firmware/src/voice_input.*`: I²S TDM de cuatro canales, micrófono en canal 0, PCM mono 16 kHz/16 bits y WAV reservado en PSRAM. Búfer inmutable hasta consumir el resultado de HTTPS.

## KEY y actualización

Antirrebote de 45 ms y agrupación de 380 ms desde la última liberación. Una pulsación avanza, dos abren escucha y tres o más retroceden. Ciclo: inicio → alarmas → tareas → calendario → música → reposo → inicio. Voz usa la última vista de consulta como referencia. Ya no se completan tareas por pulsación larga.

Dos alarmas, cuatro tareas y tres eventos por página, rotación cada seis segundos. Reposo tras 15/30/60/120 segundos según configuración; conserva mascota, reloj y calendario opcional. `calendarRest` no depende de `showOnDevice`. El equipo sigue encendido. Redibujos por cambios de minuto, página, conectividad, datos y fase; medidor como máximo cinco veces por segundo.

## Voz

Activación automática por RMS estrictamente mayor a 500 u 800 durante tres bloques completos de 320 muestras (20 ms). Igualdad o un bloque bajo el umbral reinicia la secuencia. Disponible desde consulta y reposo con `autoVoice` activo. Desde respuesta permite una nueva frase con umbral 800. Una transcripción exitosa solicita pitido; el micrófono drena muestras y queda inhibido durante el sonido y un segundo después. Doble KEY solicita captura manual incluso con `autoVoice` deshabilitado.

Captura con 300 ms previos, cierre tras 900 ms sin superar el umbral o al llenar el máximo de doce segundos de audio. Captura manual sin actividad suficiente se descarta a los 4,5 segundos. Estados: espera, captura, procesamiento y transcripción/error. `voice-transcribe` usa el servicio real y muestra su texto; no hay respuesta conversacional ni síntesis de voz. La amplitud también responde al ruido; requiere calibración física.

## Configuración y datos reales

`snapshot` sincroniza tareas y conserva preferencias recibidas. `calendar-events` descarga eventos de Google cuando hay conexión; convierte horarios con zona a ART3 y conserva la lista ante errores. NVS guarda snapshot, configuración importada y eventos para reinicios sin red si hay espacio. Si una escritura falla o el JSON supera 32 KiB, se conserva la caché previa; la consola informa fallos de escritura. No se cambió la partición NVS. Pantalla indica datos guardados/desactualizados y listas recortadas a 50.

El dashboard de `preview/dashboard.html` es local: no publica alarmas, música ni preferencias V2 al backend. El firmware recibe estos campos cuando el servicio los incluya, o mediante importación USB. No se afirma sincronización web→placa de esos campos ni reproducción musical real.

Consola USB (115200): enviar una única línea `config JSON`, objeto V2 de hasta 16000 bytes. Ejemplo sin datos de demostración:

```text
config {"version":2,"settings":{"name":"Compañero","threshold":800,"autoVoice":true,"idle":30,"calendarRest":true},"alarms":[],"music":{"title":"","artist":"","duration":0,"position":0,"playing":false,"volume":45},"integrations":{"calendar":{"showOnDevice":true}}}
```

Omitir `tasks`/`events` para conservar listas reales. JSON demasiado largo se descarta completo. `threshold 500`/`threshold 800` cambia el umbral en memoria para calibrar; `config` lo persiste. `listen`, `audio status`, `audio debug on/off`, `voice status`, `sync`, `status` y `view home|alarms|tasks|calendar|music|listening|speaking|rest` permiten revisar el estado.

## Verificación

```powershell
python firmware/generate_ui_assets.py
python firmware/test_rules.py
pio run -d firmware
```

El generador requiere Pillow y DejaVu Sans y admite `--font RUTA`. Recursos generados incluidos; Pillow no es necesario para compilar. Licencia en `firmware/FONT-LICENSE.txt`.

`test_rules.py` evalúa `static_assert` con el compilador ESP32 en C++17, sin cambiar el estándar de la aplicación ni cargar hardware. Verifica umbrales, reinicio ante picos aislados, silencio, máximo de captura, KEY simple/doble/triple/5×, rebote, rollover de `millis`, año y bisiesto.

Pendiente en placa: orientación/polaridad, contraste, legibilidad, ruido ambiente/parlante, KEY durante HTTPS, primera transcripción con esta versión, reinicio sin red y recuperación. Fuentes compactas y disco trazado son adaptaciones del diseño web que necesitan revisión física.

Diagnóstico USB: `status` incluye versión, vista, fase, RMS, umbral y reposo. `ui transition` registra cambios de pantalla/fase; `ui key` registra pulsaciones agrupadas; `voice trigger` registra origen, RMS exacto de activación, umbral y bloques; `voice capture` registra duración, actividad, silencio y envío/descarte. `audio status` incluye umbral y permiso automático. `audio debug on/off` permite muestreo periódico. Primer registro de la nueva versión: Inicio, RMS 34, umbral 500, sin activación en la observación inicial. Registro en `logs/views-voice-diagnostic.log`.

Compilación con diagnóstico: SUCCESS, 1.053.197 bytes de flash y 64.120 bytes de RAM estática. Carga COM4: SUCCESS y hash verificado. Reglas: PASS. El WAV de hasta 384.044 bytes se reserva en PSRAM; colas, tareas y TLS añaden memoria dinámica.

Observación de la versión anterior: doble KEY desde respuesta inició captura manual con RMS 25,6 (`source=manual`), sin cumplir el gate automático. La captura posterior tuvo 87 bloques sobre el umbral, duración de 5760 ms y cierre tras 900 ms de silencio. `voice-transcribe` devolvió HTTP 200. En esa versión la respuesta bloqueaba reactivación automática; ahora admite continuación a 800. No se guardó el contenido de la transcripción; este resultado no confirma su exactitud.

Prueba de sonido: `view speaking` y `audio tone` dieron `audio chime: I2S OK`. El usuario no oyó la primera señal. Se aumentó a 256 ms, amplitud 6000 y volumen −12 dB; el usuario confirmó que ahora sí se escuchó. Registros: ES8311 responde, PA46=1, DAC31=00 sin mute, DAC32=AE. `audio status` confirmó umbral 800 y permiso automático desde respuesta. Registro en `logs/assistant-sound-test.log`. Plan de ejecución de acciones: [acciones del asistente](../../plans/acciones-asistente.md).
