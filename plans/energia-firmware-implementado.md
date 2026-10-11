# Energía y autonomía: implementación del firmware

Alcance: `ESP32-MIC-AI/firmware`. No modifica frontend ni servicio Supabase. Arduino 2.0.17 / ESP-IDF 4.4, Waveshare ESP32-S3-RLCD-4.2 N16R8, KEY GPIO18 activo bajo. Mantiene particiones y credenciales existentes.

## Uso por USB

`power status` informa política aplicada, reloj, audio/radio, próxima sincronización y tiempos acumulados. `power profile available`, `power profile balanced` y `power profile maximum` guardan el perfil en NVS. `power wake` abre una excepción de diez minutos. El perfil inicial conserva la escucha continua; instalar el firmware no activa automáticamente horarios de ausencia.

| Perfil | Micrófono en reposo | Wi-Fi | Suspensión |
|---|---|---|---|
| available | continuo | conectado con ahorro de módem | ninguna |
| balanced | apagado después de inactividad | al usar y por intervalos | ligera, pantalla apagada; actualización local hasta cada minuto |
| maximum | apagado después de inactividad | al usar y por intervalos | profunda, pantalla apagada |

KEY conserva 1× hablar, 2× siguiente, 3× anterior. Después de despertar por KEY se consume la pulsación hasta liberarlo; otra pulsación inicia escucha. No hay despertar por voz con audio apagado. Los resultados permanecen disponibles durante la demora de inactividad.

`power config JSON` permite horarios y opciones. Ejemplo **no activado automáticamente**:

```json
{"version":1,"revision":0,"profile":"balanced","mic_idle":"off","wifi_mode":"interval","sleep_mode":"light","idle_seconds":60,"wifi_interval_minutes":15,"absence_check_minutes":30,"wake_alarm":true,"timezone":"America/Argentina/Buenos_Aires","absence_rules":[{"enabled":true,"days":[1,2,3,4,5],"start":"08:00","end":"17:00"}]}
```

Enviar el JSON en una sola línea precedido por `power config `. Días ISO: lunes 1, domingo 7. Máximo ocho reglas. Demora: 30/60/120/300 s. Wi-Fi: 5/15/30/60 min. Comprobación en ausencia: 0/30/60 min; 0 espera al fin del horario o KEY. Inicio incluido, fin excluido; permite medianoche y une reglas contiguas/superpuestas. Rechaza inicio=fin y otras zonas horarias. Las opciones de suspensión requieren micrófono apagado y Wi-Fi por intervalos. Configuraciones recibidas durante una operación se rechazan por USB para que puedan reenviarse; no interrumpen la operación.

En ausencia usa suspensión profunda. KEY y alarmas permitidas conceden diez minutos de excepción; una acción en curso impide suspender incluso al expirar la excepción. Con reloj inválido evita dormir y reintenta obtener hora mediante conexiones acotadas.

## Seguridad de las transiciones

La tarea propietaria del audio confirma parada de ES7210, ES8311, amplificador GPIO46 e I²S; al recuperar restablece codecs y DMA, con guarda de detección de dos segundos. La radio espera que se consuman todas las respuestas pendientes. No suspende grabaciones, acciones, solicitudes HTTPS ni agrupaciones de clics. Los identificadores de recuperación de acciones y deduplicación de alarmas siguen en NVS.

Cada ventana periódica permite conexión durante 30 segundos; solicitudes iniciadas pueden continuar hasta terminar, incluyendo espera de Wi-Fi de 15 segundos y los límites HTTPS existentes de 12/45 segundos. Desactiva la reconexión automática; reintentos del modo conectado tienen espera de al menos un minuto. Durante una interacción inicia la conexión antes de transcribir; el trabajador espera conexión y comunica el fallo si no se consigue. Una operación sin respuesta satisfactoria no se anuncia exitosa.

El ST7305 recibe Display Off y Sleep In, con espera antes de suspender el ESP32. Reinicializa el panel al despertar. No trata RLCD como e-paper ni promete conservar imagen visible. El temporizador elige el mínimo entre fin de ausencia, sincronización y alarma. La suspensión ligera además despierta para reloj y rotación de vistas. La profunda reinicia `setup`; restaura NVS/RTC antes de los codecs. Puede volver a dormir en un arranque de temporizador durante ausencia sin inicializar codecs, pantalla ni red, si no hay recuperación de acción pendiente. El constructor actual de pantalla todavía prepara SPI y memoria antes de `setup`.

Un resumen persistente de hasta 8192 bytes conserva ajustes e identidades de todas las alarmas; limita eventos/tareas si exceden el espacio. Si no puede guardar alarmas/configuración, cancela suspensión profunda. Los datos restaurados se marcan desactualizados hasta la siguiente sincronización. No altera OAuth.

## Contrato para integración posterior

El firmware acepta opcionalmente `power_config` en el snapshot existente, con el mismo JSON y revisión creciente. Espera quedar libre para aplicar esa configuración. No necesita ese campo para funcionar localmente y es compatible con snapshots anteriores. Informa `revision_applied` localmente, pero **no implementa ni declara confirmación remota**: `remote_ack=not_configured`. La entrega desde dashboard, almacenamiento por propietario/dispositivo y publicación de telemetría requieren desarrollo posterior del backend/frontend. No se modificaron en este trabajo.

No se agregaron acciones de voz de energía a la lista cerrada del backend. Los ajustes de energía se aplican por USB o por el futuro campo de snapshot. Las acciones actuales de tareas y Calendar mantienen sus rutas e idempotencia.

## Comprobaciones y medición

- [x] Compilación PlatformIO con versiones fijadas.
- [x] Aserciones de horarios: límites 07:59/08:00/16:59/17:00, noche, domingo/lunes, unión de reglas y validaciones.
- [x] Suspensión ligera: temporizador de 10 segundos; contador `wakes=1`, `sleep_us=9999925`, USB recuperado y audio activo.
- [x] Suspensión profunda: temporizador de 10 segundos; `deep_wakes=1`, reloj válido, vinculación y configuración conservadas.
- [x] Apagado tras 30 segundos sin actividad: `audio=off`, `radio=off`; recuperación manual por comando: ambos activos.
- [x] Versión final: light-sleep de 5 segundos (`sleep_us=4999930`), USB y Wi-Fi recuperados; deep-sleep de 5 segundos conserva `revision_applied=2`.
- [ ] Verificar despertar físico por KEY y recuperación de una grabación manual.
- [ ] Medir corriente desde batería, sin alimentación USB; verificar visualmente pantalla apagada y recuperación.
- [ ] Ensayo de varias jornadas, router desconectado, alarmas y reinicio durante acciones.

Pruebas de energía son silenciosas: `qa on`, luego `power test light 10` o `power test deep 10` con trabajador libre. No modifica el perfil persistente. Después usar `qa off`. USB nativo puede desconectarse durante suspensión; volver a abrir COM4 después de despertar.

Estado al terminar: perfil `available`, ausencia sin reglas activas, QA desactivado. La configuración desde dashboard/backend y la medición eléctrica siguen pendientes.

Resultado en placa: la primera prueba ligera dejó COM4 sin respuesta. La corrección del PHY USB y del controlador Arduino se cargó y validó: ambas suspensiones volvieron por temporizador y permitieron consultar estado. Los datos Calendar se resincronizaron correctamente; las respuestas de 5651 bytes se conservaron mediante la caché compacta para energía. Se amplió RX USB a 16384 bytes para recibir configuraciones sin pérdida. La prueba rechazó entrar en sueño mientras había solicitudes pendientes. [Espressif documenta la posible falta de reenumeración del USB Serial/JTAG después de light-sleep](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-guides/usb-serial-jtag-console.html).


Batería declarada: 2.6 Ah / 3.7 V / 9.26 Wh; la multiplicación corresponde a 9.62 Wh, pendiente confirmar etiqueta. No se informa porcentaje ni autonomía inventados. ADC de batería pendiente de validación/calibración de revisión de placa. Los tiempos de actividad/suspensión son telemetría de software, no mediciones eléctricas.

Fuentes técnicas: [ST7305 v0.2, Sleep In sección 8.1.8](https://files.waveshare.com/wiki/common/ST_7305_V0_2.pdf), [Waveshare, recursos oficiales](https://docs.waveshare.com/ESP32-S3-RLCD-4.2/Resources-And-Documents), [Espressif, suspensión ESP32-S3 IDF 4.4](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/system/sleep_modes.html).
