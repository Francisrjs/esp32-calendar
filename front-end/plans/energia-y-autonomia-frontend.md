# Plan de desarrollo frontend: energía y autonomía

Estado: interfaz frontend implementada como borrador aislado; persistencia pendiente del contrato backend/firmware. Este documento es independiente del plan de firmware.

## Objetivo

Agregar al dashboard una sección Energía para configurar reposo, Wi-Fi por intervalos, suspensión entre actualizaciones y horarios de ausencia. El frontend guarda preferencias y muestra su estado; el ESP32 ejecuta las políticas y controla los periféricos.

Ejemplo de horario que debe poder configurarse: lunes a viernes de 08:00 a 17:00, zona America/Argentina/Buenos_Aires. No precargarlo como regla activa sin que el usuario guarde explícitamente.

## Alcance y dependencias

Frontend actual: Next.js, componentes del dashboard y cliente Supabase. Mantener diseño, sesión y selección de dispositivo existentes. Trabajar únicamente sobre el dispositivo activo del propietario autenticado.

El frontend no implementa apagado de micrófono, Wi-Fi ni suspensión del procesador. Tampoco puede despertar por Internet una placa desconectada. Requiere un contrato de backend y telemetría del firmware: hasta que existan, mostrar la función como no disponible, sin aparentar ejecución.

Plan de firmware relacionado: [energía y autonomía ESP32](energia-y-autonomia-esp32.md). Su implementación es una dependencia separada.

## Pantallas y controles

### Energía del dispositivo

- Selector de perfil: Siempre disponible, Equilibrado, Ahorro máximo y Personalizado.
- Micrófono en reposo: escucha continua o apagado. Explicar que apagado requiere KEY para hablar.
- Wi-Fi: conexión continua con ahorro o por intervalos; intervalos 5/15/30/60 minutos, sujetos a contrato del backend.
- Suspensión por inactividad: ninguna, ligera o profunda; demora 30/60/120/300 segundos.
- Pantalla durante suspensión: apagada. Ofrecer conservar imagen sólo si el firmware informa que el hardware lo permite.
- Comprobación remota durante ausencia: desactivada o cada 30/60 minutos. Mostrar la demora prevista para recibir cambios.
- Despertar por alarma: opción visible; propuesta predeterminada sí, pendiente de contrato.
- Resumen legible del comportamiento elegido, antes de Guardar.

Separar reposo visual de suspensión energética. La rotación actual de reposo/diario/semanal cada tres minutos no significa que el procesador esté dormido. No cambiar esa preferencia silenciosamente al seleccionar un perfil: mostrar el efecto sobre las vistas.

### Horarios de ausencia

Editor de reglas con nombre, interruptor de habilitación, días de la semana, hora inicial y final en formato de 24 horas, zona horaria y acciones de editar/eliminar.

Vista de resumen por semana y próximo período de ausencia. Para intervalos nocturnos, indicar el día de salida: lunes 22:00–martes 07:00. Rechazar inicio igual a fin; si se necesita día completo, ofrecer una opción explícita compatible con backend. Detectar solapamientos y mostrar el resultado combinado que aplicará el dispositivo. No resolver reglas de forma diferente al firmware.

Usar el término «Suspensión programada». Explicar que «apagado» es suspensión profunda, no desconexión eléctrica completa, salvo que el firmware declare hardware compatible.

### Estado y aplicación

Mostrar:

- Perfil y estado que informó la placa, separados de los ajustes guardados.
- Última conexión y última telemetría, con fecha/hora y antigüedad.
- Revisión guardada y revisión aplicada.
- Próxima conexión prevista y próximo despertar, sólo si fueron calculados e informados por firmware/backend.
- Estados Guardando, Guardado pendiente, Aplicado, No compatible, Error y Datos antiguos.

Mientras la placa esté dormida/desconectada: «Guardado. Se aplicará en la próxima conexión». No poner «Aplicado» por una escritura exitosa en Supabase. Si no hay comprobaciones remotas durante ausencia, explicar que el cambio llegará al final del horario o al despertar físicamente con KEY. No presentar un botón de despertar remoto como funcional.

### Batería y autonomía

Metadatos informados: 2.6 Ah, 3.7 V y 9.26 Wh. Presentan una diferencia: 2.6 × 3.7 = 9.62 Wh. Permitir revisar los datos de la etiqueta; no corregirlos silenciosamente.

Mostrar voltaje, carga estimada o estado de alimentación únicamente si el firmware los informa y documenta cómo se midieron. No derivar porcentaje preciso de un voltaje aislado. Autonomía estimada requiere consumo medido de placa completa y energía útil; si no existen, mostrar «Autonomía todavía no medida». Las capacidades nominales no justifican anunciar horas restantes.

## Contrato propuesto con backend/firmware

Acordar nombres y almacenamiento antes de implementar. No asumir que estas columnas o endpoints existen hoy.

Configuración propuesta: schema_version, device_id, owner_id, revision, profile, microphone_idle, wifi_mode, wifi_interval_minutes, sleep_mode, sleep_after_seconds, display_sleep, remote_check_minutes, wake_for_alarm y absence_rules.

Cada regla: id estable, enabled, label, weekdays, start_time, end_time, timezone y política de excepción compatible. Días definidos sin ambigüedad, por ejemplo ISO lunes=1/domingo=7. null en remote_check_minutes significa sin comprobación durante ausencia.

Estado propuesto: supported_capabilities, firmware_version, desired_revision, applied_revision, power_state, last_seen_at, telemetry_at, next_sync_at, next_wake_at, battery_voltage y error_code. Tiempos absolutos en ISO UTC; el frontend los presenta en la zona seleccionada. No inventar valores ausentes.

Validación autoritativa en backend; frontend replica las reglas para dar mensajes inmediatos. Cambios condicionados por revision para evitar sobrescribir otra pestaña. Separar configuración deseada de telemetría reportada. Acceso y RLS por propietario/dispositivo: no guardar credenciales Wi-Fi, OAuth o tokens del dispositivo en formularios, exportaciones o logs.

Compatibilidad: detectar capacidades/versiones. No enviar deep-sleep a un firmware que no lo soporta. Mantener las preferencias actuales al agregar la sección; no reemplazarlas con defaults por campos nuevos ausentes.

## Checklist de implementación frontend

### 1. Preparación y contrato

- [x] Revisar dashboard, selección de dispositivo, sesión y patrones de guardado actuales.
- [ ] Acordar contrato versionado, valores permitidos y capacidades con backend/ESP32.
- [x] Definir diferencia entre borrador de sesión, configuración guardada, pendiente y aplicada.
- [ ] Confirmar límites de reglas, tratamiento de solapamientos y medianoche.
- [x] Definir tipos y validadores frontend sin asumir una API ya desplegada.

### 2. Interfaz

- [x] Agregar entrada Energía al dashboard y controles de perfiles.
- [x] Implementar borradores de micrófono, Wi-Fi, suspensión y pantalla; no se guardan mientras falte el contrato.
- [x] Implementar editor y resumen semanal de horarios en memoria de la sesión.
- [x] Incorporar resumen del comportamiento antes de cualquier futura escritura.
- [x] Mostrar dependencias y estados no disponibles sin atribuir soporte a la placa.
- [x] Mantener accesibilidad: etiquetas, teclado, foco, errores asociados y estados anunciados.
- [x] Adaptar las tarjetas y formularios a pantallas móviles.

### 3. Validación

- [x] Validar días ISO, HH:MM, zona admitida y al menos un día seleccionado.
- [x] Rechazar intervalos iguales y mostrar las opciones incompletas.
- [x] Presentar claramente intervalos que cruzan medianoche y el día de salida.
- [x] Detectar solapamientos y señalar que la combinación final depende del firmware.
- [x] Advertir la incompatibilidad entre micrófono apagado y activación automática por voz.
- [x] No calcular próximas horas sin estado de reloj/telemetría informado.

### 4. Persistencia e integración

- [ ] Leer ajustes del dispositivo activo del usuario autenticado.
- [ ] Guardar con revisión esperada y conservar los demás campos.
- [ ] Resolver conflicto de revisión ofreciendo recargar/comparar, sin sobrescritura silenciosa.
- [ ] Mostrar Guardado pendiente hasta confirmación de firmware.
- [ ] Actualizar telemetría con suscripciones o consultas acotadas, según backend disponible.
- [ ] Tratar desconexión como estado previsto, no como error permanente.
- [ ] Diferenciar error de guardado, error de aplicación y datos antiguos.
- [ ] No simular éxito de una función cuyo backend/firmware no está listo.

### 5. Batería

- [x] Presentar metadatos y advertir la diferencia Ah/Wh sin modificar la etiqueta.
- [x] Mostrar mediciones sólo cuando estén disponibles.
- [x] Identificar la conexión como último contacto, sin llamarla telemetría de energía.
- [x] Mostrar «Autonomía todavía no medida» cuando falta caracterización de consumo.

### 6. Pruebas y aceptación

- [ ] Guardar una regla lunes–viernes 08:00–17:00 y recuperarla tras recargar.
- [ ] Probar regla nocturna, domingo/lunes, solapamiento y horario inválido.
- [ ] Probar dispositivos distintos y sesiones sin permisos.
- [ ] Probar guardado desde dos pestañas y conflicto de revisión.
- [ ] Probar guardado con placa dormida: permanece pendiente hasta el ack real.
- [ ] Probar firmware antiguo: controles incompatibles bloqueados y explicación visible.
- [ ] Probar telemetría atrasada y próxima conexión desconocida.
- [ ] Verificar que no se anuncie autonomía sin medición.
- [x] Validar tipo/build y pruebas unitarias de reglas y horarios.
- [ ] Registrar aceptación conjunta con firmware: reglas y revisiones coinciden, sin ejecutar pruebas de reproducción de voz.

## Entregables y criterio de cierre

Interfaz Energía, tipos/validadores, estados de aplicación, editor de horarios, pruebas y documentación de uso. La persistencia e integración siguen pendientes: la tabla actual `esp32_device_settings` no contiene el contrato de energía y el firmware no informa las capacidades/revisiones de aplicación. La sección sólo podrá considerarse integrada cuando guarde ajustes autorizados y refleje la confirmación real del ESP32; este borrador de frontend no la simula.

Implementación frontend · 11 de octubre de 2026

- Añadida la sección «Energía» a la navegación del dashboard, con resumen de borrador, telemetría diferenciada y panel de batería.
- Añadidos perfiles, controles de reposo/Wi‑Fi/suspensión y un editor semanal de horarios. Los cambios viven sólo en memoria de la pestaña; no escriben en Supabase ni envían comandos a la placa.
- Añadidas validaciones para días ISO, zona admitida, formato 24 h, medianoche, conflictos, y combinación micrófono/activación automática.
- La autonomía se muestra como no medida; se presenta la inconsistencia entre 2,6 Ah, 3,7 V y 9,26 Wh sin corregirla.
- Verificación frontend: `npm run test`, `npm run typecheck` y `npm run build`.
- El plan de firmware y el árbol `ESP32-MIC-AI/` no se modificaron.
