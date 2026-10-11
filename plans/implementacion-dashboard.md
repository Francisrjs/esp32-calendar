# Plan de implementación del dashboard Compañero

Fecha: 10 de octubre de 2026.
Estado: aplicación Next.js en `ESP32-MIC-AI/front-end`; integración Supabase desplegada y etapas externas condicionadas por credenciales.
Plan relacionado: [Refactorización de las vistas del ESP32](refactorizacion-vistas.md).

## Objetivo

Convertir el dashboard independiente en el panel web de gestión del proyecto: tareas, alarmas, Google Calendar, música y preferencias del dispositivo mediante formularios, sin conversación con la IA desde la web. La voz y el asistente pertenecen a la experiencia del ESP32.

Este documento registra el trabajo preparado y ordena la implementación futura. No implica que las conexiones reales o la sincronización con la placa ya funcionen.

## Base preparada

- [x] Dashboard independiente en `preview/dashboard.html` y `preview/dashboard.js`.
- [x] Resumen, tareas, alarmas, agenda mensual, música, dispositivo y conexiones.
- [x] Crear, editar y eliminar tareas, alarmas y eventos; conservar IDs al editar.
- [x] Completar tareas, activar alarmas y filtrar pendientes/completadas.
- [x] Configurar música, proveedor y destino de demostración.
- [x] Configurar nombre, reposo, calendario en reposo y umbral de voz.
- [x] Vinculación, selección de calendario y sincronización Google simuladas.
- [x] Guardado local, importación/exportación validada y compatibilidad con JSON V2.
- [x] Compartir cambios entre dashboard y estudio de pantallas bajo el mismo origen.
- [x] Verificación de formularios, persistencia, importación/exportación y adaptación móvil.

Referencias: [preview](../preview/README.md), [contrato JSON](../preview/handoff/config.schema.json), [kit de implementación](../preview/handoff/README.md) y [plan general existente](../docs/implementation-plan.md).

## Decisiones de producto

1. Mantener el dashboard separado del estudio de pantallas y sin chat de IA.
2. Usar el dashboard para configurar; usar la pantalla del ESP32 para consultar.
3. Distinguir tareas, eventos de agenda y alarmas del dispositivo.
4. Mostrar guardado local, sincronización pendiente, sincronización confirmada y error como estados diferentes.
5. Conservar los ejemplos dentro del entorno de demostración; no copiarlos automáticamente a cuentas o dispositivos reales.
6. Preparar integraciones por etapas y reutilizar la vinculación existente después de revisar su comportamiento.

## Etapa 1 · Contrato y adaptadores de datos

- [ ] Inspeccionar el esquema y los endpoints existentes antes de crear tablas o servicios.
- [ ] Separar el adaptador local del adaptador remoto manteniendo la interfaz de gestión.
- [ ] Mapear `settings`, `tasks`, `alarms`, `events`, `music` e `integrations` a los datos reales.
- [ ] Conservar IDs estables y añadir revisión/fecha de actualización para detectar cambios concurrentes.
- [ ] Definir la estrategia de migración del contrato V2 antes de añadir campos incompatibles.
- [ ] Representar carga, listas vacías, errores y operaciones pendientes sin perder formularios.

Aceptación: los formularios utilizan el adaptador elegido, los cambios son validados y una operación fallida conserva el contenido ingresado.

## Etapa 2 · Cuenta y dispositivo

- [ ] Integrar acceso de usuario y vinculación con su ESP32, revisando `preview/pair.html`.
- [ ] Asociar configuración al propietario y al dispositivo correspondiente.
- [ ] Verificar autorización de lectura/escritura en el servicio y evitar acceso entre propietarios.
- [ ] Mantener secretos de proveedores fuera del navegador, el JSON exportado y el firmware.
- [ ] Diferenciar la sesión del usuario de la identidad usada por la placa.

Aceptación: sólo el propietario puede administrar su configuración y un dispositivo recibe únicamente los datos que le corresponden.

## Etapa 3 · Tareas y alarmas reales

- [ ] Conectar creación, edición, eliminación y cambio de estado de tareas al servicio existente.
- [ ] Persistir hora, repetición y activación de alarmas.
- [ ] Enviar a la placa una configuración compacta con revisión identificable.
- [ ] Confirmar cuándo el ESP32 recibió y aplicó una modificación.
- [ ] Resolver cambios concurrentes explícitamente, sin sobrescribir silenciosamente datos recientes.
- [ ] Mantener en la placa la información necesaria para ejecutar alarmas sin red, mientras haya energía y hora válida.

Aceptación: una edición se conserva al volver a iniciar sesión y su aplicación en el dispositivo puede comprobarse. Guardar en la web no se presenta como confirmación de recepción en la placa.

## Etapa 4 · Google Calendar

- [ ] Sustituir la vinculación simulada por el flujo de autorización real.
- [ ] Descubrir calendarios accesibles e IDs reales; reemplazar Principal/Trabajo/Personal de demostración.
- [ ] Obtener eventos del calendario seleccionado y representar fechas en la zona horaria definida.
- [ ] Conectar creación, edición y eliminación de eventos a ese calendario, respetando los permisos disponibles.
- [ ] Definir intervalo de sincronización, manejo de cancelaciones y última sincronización exitosa.
- [ ] Conservar una copia de próximos eventos para consulta sin conexión en la placa.
- [ ] Mantener separados `showOnDevice` —visibilidad de eventos— y `calendarRest` —mes visible en reposo—.

Aceptación: seleccionar otro calendario cambia los eventos reales mostrados. La marca de sincronización sólo se actualiza al confirmar una consulta exitosa.

## Etapa 5 · Música y preferencias

- [ ] Conectar el proveedor y descubrir destinos de reproducción reales.
- [ ] Confirmar las capacidades y permisos del reproductor antes de habilitar controles.
- [ ] Leer título, artista, duración, posición y reproducción desde el proveedor.
- [ ] Enviar acciones desde el dashboard y reflejar el estado confirmado por el servicio.
- [ ] Sincronizar nombre, reposo, calendario y umbral de voz con la placa.

Aceptación: el dashboard y el ESP32 muestran el mismo estado confirmado; los controles no anuncian éxito si el proveedor o la placa rechazaron la operación.

## Etapa 6 · Validación final

- [ ] Probar sesiones, permisos, desconexiones, reintentos y expiración de autorización.
- [ ] Probar CRUD completo con datos reales de prueba y sin mezclar los ejemplos locales.
- [ ] Verificar zona horaria, cambios de día y actualización entre web y dispositivo.
- [ ] Revisar escritorio, tablet y celular, teclado y mensajes de error.
- [ ] Probar recuperación del ESP32 tras reiniciar y tras perder conexión.

Orden recomendado: contrato → cuenta/dispositivo → tareas y alarmas → Calendar → música → validación integral. Coordinar la recepción de datos con el plan de vistas, sin bloquear el desarrollo del renderizador.

## Límites del estado actual

La preview antigua mantiene su modo de demostración: «Simular vinculación» sólo guarda una preferencia local y «Sincronizar demo» sólo cambia una marca de tiempo. El nuevo dashboard tiene un adaptador real de Google Calendar, disponible cuando se configuren los secretos OAuth. Spotify sigue pendiente. La placa recibe el snapshot actualizado, pero no informa qué revisión aplicó; tampoco se implementó la ejecución autónoma de alarmas en el firmware. El dashboard no graba audio, no ejecuta IA ni configura directamente el hardware.

## Implementación del dashboard Next.js · 10 de octubre de 2026

- La app nueva vive en `ESP32-MIC-AI/front-end`; la preview original permanece separada.
- Añadidos Supabase Auth, lectura y escritura con RLS por propietario, revisión optimista y flujos CRUD para tareas y alarmas.
- Añadidas preferencias JSON V2 compatibles con el importador existente del firmware. El snapshot de la Edge Function ahora entrega preferencias, tareas y alarmas en ese contrato.
- `esp32-api` desplegada como versión 9. Añadidas rutas con JWT de usuario para descubrir calendarios, consultar eventos y crear, editar y eliminar eventos de Google. Los tokens OAuth siguen sólo en el backend.
- Aplicadas las migraciones `dashboard_owner_access` y `dashboard_column_grants` al proyecto Supabase conectado. Los permisos de escritura están limitados a las columnas editables.
- Netlify usa la detección automática de Next.js desde `ESP32-MIC-AI/front-end` y requiere las dos variables `NEXT_PUBLIC_SUPABASE_*`.
- Verificación local: `npm run typecheck` y `npm run build` completados. `npm audit` informó cero vulnerabilidades después de fijar Next.js 15.5.27 y PostCSS 8.5.23.

Para completar la puesta en marcha faltan las variables de Supabase en `.env.local` y Netlify, configurar el callback/secretos OAuth de Google, y conectar credenciales del proveedor de música. La placa informa `last_seen_at`, pero aún no confirma la revisión concreta aplicada; las alarmas tampoco están implementadas como ejecución autónoma en el firmware. El repositorio GitHub no está vinculado en esta sesión.
