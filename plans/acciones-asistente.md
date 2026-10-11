# Plan de acciones del asistente y conversación continua

Fecha: 10 de octubre de 2026.
Base: [vistas](refactorizacion-vistas.md), [dashboard](implementacion-dashboard.md) y [firmware](../docs/screen/firmware-refactor.md).

## Objetivo y estado real

Convertir una frase hablada en una acción validada, ejecutarla con permisos del propietario y mostrar su resultado real. Permitir otra frase desde «Tu asistente» después de una señal sonora, sin obligar a usar KEY cada vez.

Actualmente el servicio `voice-transcribe` devuelve texto; no interpreta órdenes ni las ejecuta. Existen endpoints autenticados de tareas y Google Calendar. Dashboard, alarmas y música todavía tienen integración parcial o local. Este plan no afirma que las acciones nuevas ya estén implementadas.

En esta etapa se implementó y cargó la señal sonora de fin de transcripción y la posibilidad de reactivación desde el resultado con RMS estrictamente mayor a 800 durante tres bloques de 20 ms, cuando `autoVoice` está activo. El pitido se solicita a la tarea de audio; se inhibe captura durante la reproducción y un segundo posterior. Se descarta el audio previo al sonido. Doble KEY sigue ofreciendo entrada manual. El usuario confirmó que la segunda prueba de sonido fue audible.

## Acciones y etapas

| Acción | Ejemplo hablado | Ejecución prevista | Resultado en pantalla | Disponibilidad |
| --- | --- | --- | --- | --- |
| Crear tarea | «Agregá comprar leche para mañana» | `task-create`, fecha local resuelta | «Tarea creada: Comprar leche · mañana» | Endpoint existente; interpretar frase pendiente |
| Consultar tareas | «¿Qué tengo pendiente?» | Snapshot del propietario | Pendientes reales y paginación | Datos existentes |
| Completar tarea | «Completá comprar leche» | Resolver ID y `task-complete` | Título y estado confirmado | Endpoint existente |
| Consultar agenda | «¿Qué tengo mañana?» | `calendar-events` y filtro por fecha | Horarios y títulos reales | Requiere Google vinculado |
| Crear evento | «Agendá reunión mañana a las diez por una hora» | `calendar-event-create` | Título, fecha, inicio y fin confirmados | Endpoint existente |
| Modificar evento | «Pasá la reunión a las once» | Resolver ID y `calendar-event-update` | Horario nuevo confirmado | Endpoint existente; resolver ambigüedad pendiente |
| Eliminar evento | «Borrá la reunión de mañana» | Confirmación específica y endpoint autorizado para dispositivo | «Evento eliminado» sólo después de confirmar | Ruta web existente; ruta de dispositivo pendiente |
| Consultar hora/fecha | «¿Qué hora es?» | RTC/NTP local | Hora real o «Hora no disponible» | Puede ser local |
| Consultar ambiente | «¿Qué temperatura hay?» | Última lectura SHTC3 con antigüedad | Temperatura, humedad y estado de lectura | Puede ser local |
| Cambiar vista | «Mostrame el calendario» | Evento de navegación en hilo de UI | Vista solicitada | Implementación local pendiente |
| Preferencias | «Desactivá la escucha automática» | Actualización validada y persistida | Valor guardado y modo vigente | Contrato/importación existentes; orden por voz pendiente |
| Alarmas | «Poné una alarma a las siete de lunes a viernes» | Servicio persistente más ejecución local de alarma | Horario, días y estado confirmado | Fase posterior; dibujar alarmas no equivale a programarlas |
| Música | «Pausá la música» / «Volumen cuarenta» | Adaptador del reproductor externo | Estado confirmado por proveedor | Fase posterior; sin reproductor conectado mostrar no disponible |
| Conversación informativa | «Explicame…» | Servicio de respuesta, separado de acciones | Texto con límite y paginación | Fase posterior |

No prometer acciones que el dispositivo o servicio no soportan. Una orden para música no modifica sólo el dibujo. Una alarma no se considera creada hasta tener persistencia y mecanismo que la ejecute.

## Flujo de una petición

1. Espera de voz o doble KEY → captura → cierre por silencio.
2. Transcripción: validar audio y texto; rechazar audio insuficiente antes de interpretar. Una frase ajena como «subtítulos por la comunidad» no debe transformarse en una acción por conjetura.
3. Interpretación: producir una intención de la lista permitida y argumentos tipados. Sin ejecutar código, SQL ni URLs generados por el modelo.
4. Resolución: buscar objetos sólo en datos del propietario, resolver fechas con `America/Argentina/Buenos_Aires` y pedir datos faltantes.
5. Si hace falta aclaración, mostrar una pregunta breve y conservar contexto para la siguiente frase. Ejemplo: dos tareas «comprar leche» → preguntar cuál, sin completar ambas.
6. Validación: comprobar tipos, rangos, permisos, conexión y disponibilidad de la integración. Para borrar, mostrar el objeto concreto y pedir confirmación. Un «sí» sólo confirma la operación pendiente de esa conversación y dentro de su vencimiento.
7. Ejecución: llamar al adaptador autorizado con ID de petición idempotente; esperar respuesta definitiva.
8. Mostrar resultado confirmado o error útil. Sin confirmación del servidor, usar «No pude confirmar el cambio» y permitir consultar su estado antes de reintentar.
9. Actualizar datos y dibujar; emitir sonido de fin y habilitar otra frase. La petición ya iniciada conserva su estado aunque el usuario navegue con KEY.

## Contrato propuesto

El intérprete recibe transcripción, zona horaria, hora validada, contexto acotado e IDs de candidatos autorizados. La identidad del propietario procede del token autenticado, nunca del texto hablado ni del resultado del modelo.

Respuesta estructurada del servicio de acciones:

```json
{
  "request_id": "uuid",
  "conversation_id": "opaque-id",
  "status": "succeeded",
  "action": "task.create",
  "display_text": "Tarea creada: Comprar leche · mañana",
  "entity_id": "uuid",
  "revision": 12,
  "refresh": ["tasks"],
  "follow_up": true
}
```

Estados permitidos: `needs_clarification`, `needs_confirmation`, `running`, `succeeded`, `failed`, `unsupported`. Las aclaraciones incluyen pregunta y candidatos identificados; las confirmaciones incluyen operación pendiente, vencimiento y objeto concreto. Errores incluyen código estable, mensaje legible y si corresponde reintentar. Una respuesta HTTP exitosa no convierte `running` en `succeeded`.

Endpoints nuevos propuestos: interpretación/ejecución de acciones y continuación de conversación. Nombres y esquema definitivo se fijarán al implementar; reutilizar RPC y endpoints actuales en vez de duplicar las reglas de tareas. Las cuotas deben limitar interpretación y conversación además de transcripción. Claves de proveedores permanecen en el backend.

## Resultado, sonido y siguiente frase

- Mostrar «Procesando» mientras se interpreta y «Guardando» mientras se ejecuta. Mostrar «Tu asistente» con el resultado, no con una etiqueta que prometa acción cuando sólo hay transcripción.
- Conservar el resultado hasta que empiece otra captura, se use KEY o venza el reposo configurado. Texto largo: paginación legible y aviso de página; el pitido se emite una sola vez por resultado completo, no por página o redibujo.
- Sonido corto de fin: pitido de 880 Hz, aproximadamente 256 ms, amplitud PCM 6000 y DAC a −12 dB. La primera prueba (80 ms, amplitud 1000, −24 dB) no fue audible según el usuario; la segunda sí. No es una voz sintetizada.
- Durante sonido y un segundo posterior: bloquear captura, drenar I²S y limpiar preroll. Después, desde resultado, exigir RMS > 800 en tres bloques. No interpretar 800 como «más sensible»: un umbral mayor evita más ruido y exige hablar más fuerte/cerca.
- Si `autoVoice` está desactivado, no reactivar automáticamente; doble KEY continúa operativo. Mantener la ventana manual disponible durante peticiones de red, indicando ocupado cuando corresponda.
- No aceptar audio del futuro TTS como una nueva orden; ampliar el bloqueo a toda su reproducción y liberarlo sólo tras drenar muestras.
- Guardar preferencias de escucha, pero no conversaciones completas por defecto. Contexto pendiente acotado, con vencimiento y aislamiento por dispositivo/propietario.

## Orden de implementación

### 1. Confirmar experiencia de sonido y continuación

- [x] Comando USB `audio tone`, solicitado a la tarea que posee I²S.
- [x] Pitido al recibir una transcripción exitosa, con logs de resultado I²S.
- [x] Rearmar voz desde resultado con umbral 800 y protección contra el pitido.
- [x] Confirmar audibilidad con el usuario y ajustar volumen: segunda prueba audible.
- [ ] Probar una segunda frase automática, ruido, KEY y `autoVoice=false` en hardware.
- [x] Mostrar «Podés seguir hablando» o «2× KEY para hablar» según modo; el micrófono se rearma tras la guarda del sonido.

### 2. Ejecutar tareas y consultas

- [x] Implementar contrato tipado e intérprete con lista permitida.
- [x] Crear/consultar/completar tareas con resolución de IDs y fechas.
- [x] Implementar hora, sensor y navegación como acciones locales.
- [x] Mostrar éxito sólo después de persistencia y actualizar snapshot sin repetir la operación.
- [x] Añadir idempotencia, estados indeterminados y errores de conexión.

### 3. Agenda, aclaraciones y confirmaciones

- [x] Consultar/crear/modificar eventos con duración y zona horaria validadas.
- [x] Preguntar por fecha, hora, duración o candidato faltante.
- [x] Añadir eliminación autenticada para dispositivo, con confirmación explícita.
- [x] Expirar/cancelar contexto pendiente; impedir confirmaciones cruzadas entre peticiones.

### 4. Preferencias, alarmas, música y respuestas

- [x] Conectar preferencias a persistencia y dashboard real.
- [x] Implementar almacenamiento y ejecución de alarmas, incluidos reinicios.
- [ ] Integrar reproductor externo y leer su estado después de cada orden.
- [x] Añadir respuestas informativas y, si se decide, TTS con protección de micrófono durante reproducción.

## Aceptación y pruebas

| Caso | Resultado requerido |
| --- | --- |
| «Agregá comprar leche» | Una tarea persistida, mensaje de éxito y datos actualizados |
| Reintento de la misma petición | Sin duplicados |
| Dos candidatos iguales | Pregunta aclaratoria, sin ejecutar aún |
| «Mañana» cerca de medianoche | Fecha calculada en ART3 con reloj válido |
| Sin reloj válido | Preguntar fecha o informar limitación; no inventar |
| Se corta red después de enviar | Estado indeterminado, consulta por request ID antes de repetir |
| Evento sin Google conectado | Explicar vinculación necesaria |
| Borrado sin confirmación o confirmación vencida | No borrar |
| Audio bajo, ruido o frase sin intención válida | No crear ni modificar datos |
| Resultado y pitido | Una señal; cero capturas del sonido propio |
| Segunda frase con RMS > 800 durante tres bloques | Nueva captura desde resultado sin KEY |
| RMS igual a 800 o picos aislados | No reactivar |
| Escucha automática deshabilitada | Sólo captura manual |
| KEY mientras se ejecuta | Navegación responde; el resultado no secuestra la vista elegida |
| Alarmas/música sin integración | Estado no disponible; sin éxito simulado |

Registrar vista, fase, source de activación, umbral efectivo, duración, action, request ID, estado y revisión. Evitar tokens, claves y audio crudo en logs. Para validar exactitud, usar frases de prueba acordadas y comprobar los registros persistidos, no sólo HTTP 200.

## Estado de implementación — 10 de octubre de 2026

Implementados `action-run`, `action-status`, `action-cancel` y `alarm-ack`; intérprete tipado, ledger por ID, contexto con vencimiento, resolución de duplicados por ordinal, RPC de tareas y adaptadores de agenda/preferencias/alarmas. El firmware muestra resultados paginados, conserva la navegación durante peticiones y consulta el último ID guardado después de reiniciar. Los detalles, límites y comandos están en [assistant-actions.md](../docs/assistant-actions.md).

Pruebas reales silenciosas: creación de tarea para 2026-10-11, reintento sin duplicados y tarea completada con revisión 2. Pasaron pruebas automatizadas de validación, aislamiento, fecha ART, falta de reloj, recuperación, aclaraciones y confirmación expirada/cruzada. No se reprodujo voz durante QA por pedido del usuario.

La conexión Google Calendar del chat fue comprobada: calendario principal `franrojas331@gmail.com`. OAuth completado y Google Calendar API habilitada. Consulta real desde el ESP32 confirmada con `calendar.list` succeeded: agenda del 2026-10-11 devuelve la reunión de las 19:00. Lectura y actualización de la vista devuelven HTTP 200 sin reinicios tras corregir el límite de caché NVS. Las mutaciones de Calendar están probadas con proveedor simulado; no se crearon ni borraron eventos reales durante QA. La autorización del conector del chat no se transfiere a Supabase.

Música permanece no disponible, con respuesta explícita y sin modificar una representación local del reproductor. La prueba acústica de alarmas y una nueva prueba de conversación hablada se dejan pendientes para respetar la restricción de QA silencioso.

El pitido y TTS conservan el ajuste actual: DAC 0 dB, pitido compensado (PCM 1507) y voz con objetivo de pico digital 65%; las cifras de la etapa inicial de este plan se conservan como registro histórico.

### Vistas de reposo añadidas

- [x] Reposo diario con agenda y tareas, y semanal con eventos de lunes a domingo.
- [x] Rotación perrito/diario/semanal cada tres minutos.
- [x] Acciones de apertura con fecha explícita, refrescando Calendar y tareas.
- [x] Preview, doce estados de referencia y pruebas de navegación/rotación.
- [x] QA en placa con Calendar real y transición temporizada, sin reproducción de voz.
