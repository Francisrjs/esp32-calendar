# Plan de energía y autonomía del ESP32

Estado: propuesta para implementar después de revisar este documento. Este trabajo sólo crea el plan: no cambia frontend, backend, firmware, configuración activa ni carga la placa.

## Relación con el plan de frontend

El desarrollo de la interfaz y su checklist se definen exclusivamente en [plan frontend de energía y autonomía](energia-y-autonomia-frontend.md). Este documento conserva la implementación del ESP32 y los requisitos de contrato que necesita recibir. No implementar interfaz desde este plan.

## Objetivo y equipo

Recibir y ejecutar la configuración de escucha, reposo, Wi-Fi por intervalos y horarios de suspensión. El ESP32 ejecuta la política localmente, incluso sin Internet. Alcance técnico centrado en firmware; el frontend sólo necesita presentar y enviar los ajustes definidos aquí.

Equipo actual: Waveshare ESP32-S3-RLCD-4.2, ESP32-S3 N16R8, Arduino 2.0.17 / ESP-IDF 4.4, KEY GPIO18 activo bajo, RTC externo, micrófonos ES7210, salida ES8311 y amplificador GPIO46, I²S compartido de entrada/salida. Supabase y Google Calendar ya vinculados. Verificar revisión y esquema del fabricante antes de cambiar alimentación o pines.

Situación actual: captura I²S continua; umbral 1200 RMS con tres bloques consecutivos; Wi-Fi conectado y sincronización cada cinco minutos. Desactivar la activación automática no apaga la captura. Reposo visual no significa suspensión del procesador. Reposo rota cada tres minutos; diario se omite si no hay tareas para mostrar.

## Batería y medición

Datos informados: 2.6 Ah, 3.7 V y 9.26 Wh. Confirmar etiqueta: 2.6 Ah × 3.7 V = 9.62 Wh; 9.26 Wh equivale aproximadamente a 2.50 Ah a 3.7 V. No confundir capacidad en Ah con corriente en A. Usar provisionalmente 9.26 Wh para escenarios conservadores, sin prometer autonomía.

Medir corriente en el lado de la batería, sin que el USB esté alimentando la placa. Registrar voltaje, corriente promedio, picos y energía integrada para cada modo; repetir con mismo brillo/estado de pantalla, señal Wi-Fi, volumen y batería. Incluir consumo de reguladores, codecs, pantalla, RTC, amplificador y radio; los valores de consumo del chip solo no describen la placa completa.

Autonomía estimada = energía útil medida en Wh / potencia promedio medida en W. Si se mide corriente a tensión variable, integrar V × I; no comparar directamente mAh medidos a 5 V USB con mAh de la celda a 3.7 V. Escenarios meramente ilustrativos con 9.26 Wh nominales: 0.5 W → 18.5 h; 0.2 W → 46.3 h; 0.05 W → 185 h, antes de pérdidas y capacidad no utilizable. No son resultados de esta placa.

La documentación del fabricante describe medición de batería mediante GPIO4 y divisor; validar esquema/revisión, relación del divisor y calibración ADC antes de implementar. Voltaje estimado no es porcentaje preciso ni permite anunciar horas restantes sin caracterización. Nunca dar por disponible un medidor de carga.

## Modos que debe ejecutar el firmware

| Modo | Audio | Wi-Fi | Procesador y pantalla | Activación |
|---|---|---|---|---|
| Siempre disponible | Captura continua, umbral configurado | Conectado, ahorro de módem verificado | Procesador activo; vistas actuales | Voz o KEY |
| Equilibrado | Apagado en reposo; habilitado durante conversación | Conexión al usar y por intervalos | Light-sleep cuando sea compatible con RLCD | KEY, temporizador o alarma |
| Ahorro máximo | Micrófono, DAC y amplificador apagados fuera del uso | Apagado fuera de ventanas de conexión | Deep-sleep; pantalla apagada según secuencia validada | KEY, alarma o temporizador |
| Ausencia programada | Apagado | Sin conexión continua; política de comprobación configurable | Suspensión profunda | Fin del horario, KEY o alarma permitida |

No prometer apagado eléctrico completo con encendido automático. Sin un circuito de corte y despertar verificado, «apagado programado» significa suspensión profunda y alimentación residual de la placa. El frontend debe mostrar esta descripción.

El RLCD no se debe tratar como una pantalla e-paper: verificar refresco/VCOM y secuencia de apagado. Sólo conservar una imagen durante suspensión si el hardware permite mantener sus señales con el consumo medido; de lo contrario apagarla. La rotación de tres minutos requiere despertar o mantener procesamiento; en ahorro máximo proponer pantalla apagada en lugar de despertares innecesarios.

## Contrato requerido por el firmware (sin desarrollo frontend)

Contrato propuesto, versionado y validado también en backend/firmware:

- Perfil: siempre disponible, equilibrado o ahorro máximo; presets editables mediante opciones compatibles.
- Micrófono durante reposo: continuo o apagado. Con micrófono apagado, sólo KEY inicia una conversación; no hay despertar por voz.
- Wi-Fi: conectado con ahorro o por intervalos. Intervalos propuestos 5/15/30/60 minutos; ventana de conexión acotada, por ejemplo 30 segundos, con tiempo máximo de petición compatible con los 45 segundos actuales. Separar ventana periódica de la sesión interactiva.
- Suspensión en reposo: ninguna, ligera o profunda; demora sin actividad 30/60/120/300 segundos. Separada de la demora y rotación de vistas.
- Pantalla durante suspensión: apagada; opción de conservar sólo si se verifica compatibilidad eléctrica.
- Horarios de ausencia: lista de reglas con identificador, habilitada, días, inicio y fin HH:MM, zona horaria y política de excepciones.
- Despertar por alarma: sí/no; propuesta inicial sí, pero visible y configurable. Pausa temporal por KEY: por ejemplo diez minutos o hasta terminar la conversación.
- Comprobación de configuración durante ausencia: desactivada o cada 30/60 minutos. Mostrar el impacto en batería y demora máxima para recibir cambios.
- Datos de batería: capacidad nominal/energía como metadatos; telemetría de voltaje sólo después de validar medición.

Conservar KEY 1× hablar, 2× siguiente pantalla, 3× anterior. Tras despertar por KEY, esperar su liberación, eliminar rebotes y consumir la pulsación de despertar; no iniciar una grabación accidental. Una segunda pulsación normal inicia la escucha. Mostrar «Listo, KEY para hablar» después de despertar sin audio de QA.

## Horarios y cálculo del próximo despertar

Ejemplo solicitado: lunes a viernes, 08:00–17:00, America/Argentina/Buenos_Aires. Es una propuesta de horario para cargar posteriormente, no una preferencia ya aplicada. Pedir selección explícita de horas en formato de 24 horas al configurar.

- Inicio incluido, final excluido: a las 08:00 entra en ausencia; a las 17:00 vuelve al perfil normal.
- Un intervalo que cruza medianoche se atribuye al día de inicio: lunes 22:00–martes 07:00.
- Inicio igual a fin se rechaza; para ausencia de día completo usar una opción expresa.
- Unir intervalos superpuestos y contiguos para evitar despertares breves innecesarios. Calcular correctamente domingo/lunes y cambio de mes/año.
- Guardar y ejecutar una zona IANA; convertir a regla POSIX compatible con el firmware. Inicialmente admitir la zona actual y no aceptar otras sin soporte probado, incluido horario de verano.
- Calcular el próximo despertar como el menor entre fin de ausencia, alarma habilitada, ventana de sincronización y actualización local necesaria. Despertar por alarma usa una excepción acotada y vuelve a dormir si sigue vigente la ausencia.
- KEY siempre permite recuperar el equipo físicamente. Durante ausencia ofrece uso temporal; al finalizar vuelve a evaluar el horario.
- Si no hay hora válida, no aplicar un horario que podría dejarlo inaccesible: permanecer despierto en modo manual, intentar RTC/NTP con límite y mostrar el problema.
- Al reiniciar, restaurar configuración y RTC antes de decidir la política. Deep-sleep reinicia ejecución: evitar inicializar pantalla/audio/radio si sólo toca comprobar el siguiente despertar y seguir dormido.

## Sincronización y cambios remotos

Mantener configuración en Supabase por propietario/dispositivo, con revisión. Guardarla en NVS sólo cuando cambie y publicar revisión recibida y aplicada. Validar límites y combinaciones antes de aceptar. Migraciones y contratos se diseñarán al implementar; no crear nuevas columnas ahora.

Mientras Wi-Fi esté apagado, el frontend no puede despertar la placa ni aplicar inmediatamente cambios. Mostrar estado «guardado, pendiente de la próxima conexión», última conexión y próxima ventana estimada. Si se desactiva la comprobación durante ausencia, el cambio llegará al fin del horario o al despertar físicamente con KEY.

Al conectar: autenticar, obtener ajustes, sincronizar tareas/Calendar, confirmar aplicación, actualizar hora si corresponde y desconectar al terminar. No abrir OAuth desde la placa como parte del ahorro: conservar autorización existente. Ajustes enviados durante una conversación se aplican al quedar libre.

Usar datos de Calendar guardados en memoria/caché compacta y mostrar antigüedad. Las respuestas completas actuales pueden superar NVS; diseñar un resumen persistente acotado de fechas, títulos y horarios. Sin Internet no ejecutar acciones de escritura ni anunciar éxito; explicar que falta conexión. Una reconexión interactiva puede tardar algunos segundos.

Reintentos con espera creciente y límite: un router caído no debe producir ciclos continuos de reconexión. No dejar TLS, colas o tareas huérfanas al desconectar. Separar las ventanas de sincronización del consumo de interpretación y síntesis.

## Máquina de estados y preparación segura

Estados propuestos: activo, conversación, preparando suspensión, sueño ligero, sueño profundo, despertando y sincronizando. Una única tarea/controlador de energía decide las transiciones; las tareas de audio y red confirman estar detenidas.

Antes de dormir: finalizar o cancelar de forma consistente peticiones activas; conservar IDs de recuperación e idempotencia; persistir ajustes y alarmas modificados; parar I²S mediante su tarea propietaria; detener ADC/DAC y deshabilitar amplificador; apagar radio; aplicar secuencia de pantalla; programar temporizador y KEY. No cortar una creación de evento ni repetirla después de despertar.

No usar una simple bandera de «autoVoice=false» como ahorro de audio: actualmente la captura continúa. Requiere detener hardware y tarea, y restaurarlos verificando reloj, DMA, codecs y memoria. No cambiar frecuencia de CPU sin comprobar estabilidad de I²S, pantalla y comunicaciones.

Validar GPIO18 como fuente RTC de despertar en ESP32-S3, polaridad y resistencias del botón con el esquema. Evitar entrar en sueño con KEY retenido: produciría despertares repetidos. Mantener el RTC externo y no asumir que su pila auxiliar alimenta al ESP32 o la pantalla.

## Implementación por etapas, enfocada en ESP32

1. Línea base: instrumentar tiempo activo, tiempo de radio/audio, despertares y causas. Medir consumo real de perfiles actuales; no ensayar voz reproducida durante QA.
2. Contrato: especificar ajustes y revisiones, compatibilidad con firmware anterior, validaciones y estado pendiente/aplicado. El equipo de frontend/backend implementará esos contratos después.
3. Audio: parada y recuperación completa por KEY, pruebas de grabación manual y recuperación sin tocar credenciales.
4. Radio: conexiones periódicas y por demanda, límites de reconexión, caché y comportamiento sin Internet.
5. Light-sleep: despertar por KEY/temporizador, pantalla y alarmas; mantener el modo actual como alternativa.
6. Deep-sleep: arranque corto, apagado de periféricos, restauración de RTC/NVS y recuperación de acciones pendientes.
7. Horarios: días, medianoche, excepciones, alarma y overrides; integración con revisión aplicada.
8. Medición comparativa: energía por día de uso simulado y autonomía estimada con batería real. Elegir el perfil recomendado a partir de estos resultados.

## Aceptación y QA

- No alterar tareas, Calendar, vinculación OAuth ni Wi-Fi guardado al dormir o despertar.
- Probar límites 07:59/08:00/16:59/17:00, cruce de medianoche, viernes/lunes, intervalos superpuestos y reloj inválido.
- KEY y temporizador despiertan; KEY retenido no genera bucles. Tras despierto, agrupamiento 1/2/3 funciona.
- Micrófono apagado deja de capturar realmente; radio apagado no deja tareas reconectando.
- Ajustes remotos quedan pendientes durante desconexión y se confirman al reconectar. No indicar «aplicado» sólo porque se guardó en la base.
- Alarmas respetan la política elegida y no se duplican después de reiniciar. Ordenar alarmas y reprogramar próximo despertar cuando cambien.
- Reinicio durante una acción no la duplica; recuperar el mismo request_id. Comprobar caída de Wi-Fi, expiración OAuth y batería insuficiente.
- Verificar pantalla apagada/VCOM, salida de sueño, memoria y ausencia de reinicios durante varias jornadas.
- QA de energía sin reproducción de voz; pruebas acústicas separadas requieren autorización expresa del usuario.
- Publicar corriente/energía de placa completa y condiciones de medición. No usar cifras de deep-sleep del chip para anunciar autonomía del producto.

## Fuentes y comprobaciones previas

- [Espressif: suspensión, temporizador, GPIO y Wi-Fi en IDF 4.4.2](https://docs.espressif.com/projects/esp-idf/en/v4.4.2/esp32s3/api-reference/system/sleep_modes.html).
- [ESP32-S3: funciones RTC y consumo del chip](https://documentation.espressif.com/esp32_s3_datasheet_en.pdf).
- [Waveshare: placa ESP32-S3-RLCD-4.2](https://docs.waveshare.com/ESP32-S3-RLCD-4.2).
- [Waveshare: ejemplo de medición de batería](https://docs.waveshare.com/ESP32-ESPHome-Tutorials/Example-RLCD-Voice).

Los ejemplos del fabricante con otros frameworks orientan el hardware; no son prueba de compatibilidad con el firmware Arduino actual. Antes de programar ahorro, revisar esquema exacto, documentación del RLCD, API de apagado de codecs y soporte de sueño de la versión instalada.
