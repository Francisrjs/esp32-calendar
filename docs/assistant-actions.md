# Acciones del asistente

Implementación: 10 de octubre de 2026. Firmware Waveshare ESP32-S3-RLCD-4.2, Arduino 2.0.17 / ESP-IDF 4.4 y codec ES8311/ES7210 existente.

## Flujo

Captura → `voice-transcribe` → `action-run` → resultado validado → snapshot → voz del resultado y señal de fin. La tarea de audio conserva la propiedad de I²S y de los buffers. Durante la red y la reproducción se inhibe la captura; después se conserva la guarda de un segundo y el umbral >800 RMS de la vista de resultado. La voz conserva el ajuste digital de 65% establecido antes de esta implementación.

La UI permanece en el hilo principal. KEY puede cambiar de vista durante la petición y el resultado no vuelve a abrir la pantalla anterior. Los resultados largos se paginan cada diez segundos; sólo se reproduce una señal por petición, y para listas largas la voz anuncia que el detalle está en pantalla.

## Acciones disponibles

| Acción | Ejemplo | Condiciones |
| --- | --- | --- |
| Crear tarea | Agregá comprar leche para mañana | Fecha ART; título explícito |
| Consultar pendientes | Qué tareas tengo pendientes | Datos del propietario |
| Completar tarea | Completá comprar leche | Pregunta ante duplicados; admite «la primera» |
| Consultar agenda | Qué tengo mañana en la agenda | Google autorizado en el backend |
| Crear evento | Agendá reunión mañana a las diez por una hora | Pregunta por título, fecha, hora o duración faltantes |
| Mover evento | Pasá la reunión a las once | Preserva duración; valida ETag antes de cambiar |
| Eliminar evento | Borrá la reunión de mañana | Confirmación del objeto concreto, vigente por dos minutos |
| Hora / ambiente | Qué hora es / qué temperatura hay | RTC/NTP válido; sensor con lectura de hasta 90 segundos |
| Navegación | Mostrame el calendario | Aplica evento local de UI |
| Preferencias | Desactivá la escucha automática | Persistencia del backend y actualización local |
| Alarmas | Alarma a las siete de lunes a viernes | Persistencia y chime local con hora válida; placa encendida |
| Alarma única | Alarma el 11 de octubre a las siete, una vez | Fecha persistida; acuse idempotente desactiva la alarma |
| Información | Explicame qué es… | Respuesta acotada; sin afirmar acceso a datos actuales |
| Música | Pausá la música | Informa no disponible mientras no exista reproductor vinculado |

Las alarmas emiten una señal breve al minuto programado. No recuperan alarmas perdidas con la placa apagada y no son un despertador con repetición continua. El último minuto emitido se guarda en NVS para evitar repetirlo al reiniciar. Se acepta un máximo de 50 alarmas por dispositivo desde voz.

## Contrato y recuperación

`action-run`: POST autenticado por el token del dispositivo, con `request_id` y `conversation_id` UUID, `text` (máximo 3000), reloj y sensor opcionales. `action-status` consulta el mismo ID; `action-cancel` cancela sólo el contexto pendiente de esa conversación. `alarm-ack` desactiva una alarma única del dispositivo autenticado.

El intérprete usa Responses con esquema estricto y `store:false`; `ASSISTANT_MODEL` permite configurar el modelo, por defecto `gpt-4.1-mini`. Sólo hay una intención por frase. El backend valida tipos, fechas, rangos y disponibilidad. No acepta SQL, código, IDs de propietario, URLs ni nombres de tablas generados por el modelo. Una frase de subtítulos conocida se rechaza antes de llamar al intérprete.

El ledger guarda hash de entrada, intención y resultado. Reusar un ID con otra entrada produce conflicto. Una petición en curso por dispositivo impide ejecutar otra en paralelo. Las tareas reutilizan los RPC atómicos existentes. Los eventos creados usan un ID determinista; las modificaciones usan ETag. Ante una respuesta perdida, `action-status` verifica RPC/tarea, preferencias, alarma o estado del evento; no repite una mutación automáticamente. Después de cinco minutos sin confirmación, devuelve un fallo indeterminado e indica consultar los datos antes de pedir el cambio otra vez.

La placa conserva el último ID y cuerpo de petición en NVS y consulta su estado después de reiniciar. El cuerpo no contiene audio ni claves del proveedor; se reemplaza con el siguiente pedido. No se guarda el historial completo de conversación. El contexto pendiente está aislado por propietario/dispositivo/conversación, contiene una intención acotada y vence en dos minutos. Un «sí» no puede confirmar otra conversación ni un evento cambiado sin una nueva pregunta.

Interpretación y respuestas comparten la reserva de uso existente: 60 solicitudes diarias y 1000 mensuales por propietario, también utilizada por transcripción y síntesis. Son límites de solicitudes; no una garantía exacta de gasto monetario.

## Google Calendar

El conector Google Calendar de este chat tiene acceso al calendario principal `franrojas331@gmail.com`. Esa autorización no suministra tokens al firmware ni a Supabase. La comprobación actual confirma las credenciales OAuth configuradas: `calendar-link` devuelve HTTP 200 y el enlace de consentimiento. Todavía no hay cuentas en `esp32_google_accounts`: la consulta real desde la placa devuelve `calendar_not_connected` hasta completar el consentimiento.

Preparar en Google Cloud un cliente OAuth web, con este redirect URI exacto:

`https://ofyohmwlhvinrgsgjuul.supabase.co/functions/v1/esp32-api/calendar-callback`

Guardar `GOOGLE_CLIENT_ID`, `GOOGLE_CLIENT_SECRET` y `GOOGLE_REDIRECT_URI` en [los secretos de Edge Functions](https://supabase.com/dashboard/project/ofyohmwlhvinrgsgjuul/functions/secrets). Opcional: `GOOGLE_CALENDAR_ID=franrojas331@gmail.com`. El secreto no debe enviarse al chat ni incorporarse al firmware. Habilitar Google Calendar API y autorizar la cuenta de prueba si la aplicación OAuth está en modo de prueba.

Una vez configurado, usar Conectar en Agenda del dashboard, o `calendar link` por USB para obtener el enlace del mismo propietario autenticado de la placa. Completar el consentimiento de Google en el navegador. Sólo después de ese consentimiento la placa podrá consultar y modificar Calendar sin depender de este chat.

## Pruebas y operación

KEY: una pulsación inicia escucha; dos avanzan de pantalla; tres vuelven. El agrupamiento espera 380 ms para evitar iniciar grabación al hacer doble clic.

`node ESP32-MIC-AI/supabase/tests/actions.test.cjs` ejecuta los handlers reales con base y proveedor simulados: validación, cambio de día ART, falta de reloj, idempotencia, recuperación de RPC, aislamiento, ambigüedad y ordinales, ruido, sensor, preferencias y confirmación expirada/cruzada. `python ESP32-MIC-AI/firmware/test_rules.py` incluye horarios recurrentes y alarmas únicas junto a KEY, RMS y fechas.

QA en hardware utiliza `qa on` y `action TEXTO`: no reproduce audio ni activa captura automática. `action retry` reenvía exactamente el último cuerpo; `action status` consulta el resultado. `qa off` devuelve el dispositivo a operación normal. El modo QA se conserva en NVS para que las cargas sucesivas sigan siendo silenciosas.

Se verificó una creación real para 2026-10-11, reintento sin duplicados y finalización con revisión 2; persistencia de una alarma recurrente; lectura del sensor (24.7 °C, 51% de humedad); y navegación por USB a Calendar durante una consulta de tareas, sin que el resultado cambiara esa vista. Los dos registros de QA creados se eliminaron al terminar. Calendar respondió con vinculación necesaria. No se reprodujo voz, ni se crearon o borraron eventos reales durante QA. La prueba acústica de alarmas y la aceptación de Calendar después de OAuth quedan pendientes.

Las tablas nuevas tienen RLS y permisos sólo para `service_role`; no políticas públicas porque no son una API de usuario. El asesor de Supabase informa esa ausencia de políticas como INFO, coherente con el acceso exclusivamente de servidor: [regla del asesor](https://supabase.com/docs/guides/database/database-linter?lint=0008_rls_enabled_no_policy). No se modificaron tablas ajenas al asistente.

Fuentes: [Structured Outputs](https://developers.openai.com/api/docs/guides/structured-outputs), [Google OAuth web](https://developers.google.com/identity/protocols/oauth2/web-server), [eventos con ID proporcionado](https://developers.google.com/workspace/calendar/api/v3/reference/events/insert).

Última comprobación: Google rechazó el consentimiento con 403 `access_denied` porque la app está en Testing. En el proyecto del cliente OAuth, agregar `franrojas331@gmail.com` en Google Auth Platform → Audience → Test users y repetir Conectar. El backend no está autorizado hasta completar ese consentimiento. Firmware cargado y KEY 1/2/3 verificado por pruebas de agrupamiento; QA sin reproducción de voz.

Estado actual: OAuth completado para `franrojas331@gmail.com`. Consulta real desde ESP32 rechazada con `calendar_provider_403_accessNotConfigured`: falta habilitar Google Calendar API en el proyecto del cliente OAuth. Diagnóstico desplegado sin exponer tokens; pruebas de acciones pasan; QA silenciosa.

Verificación final: Google Calendar API habilitada y lectura real desde el ESP32 exitosa para 2026-10-11. `calendar.list` succeeded y calendario HTTP 200. Se corrigió un reinicio al guardar una respuesta de 5498 bytes en NVS: la agenda permanece disponible en memoria; se omite la caché de texto sobre 3500 bytes, y los errores NVS se registran numéricamente. Firmware cargado y segunda consulta estable. Sin reproducción de voz ni mutaciones reales de Calendar.

## Reposo diario y semanal

Implementadas vistas `daily` y `weekly`, basadas en `agenda-semana.html`. Rotación de reposo: perrito → diario → semanal → perrito cada 180000 ms; pausada durante captura, reproducción o acción pendiente. Diario: eventos con duración y tareas del día más pendientes vencidas/sin fecha; semana: lunes a domingo, eventos y cantidades de tareas por fecha. Vista de día explícito mediante `local.date`; al rotar vuelve al día actual. Calendar incluye la semana seleccionada y siguiente, con límite actual de 50 eventos. Listas del diario paginan cada 6 segundos.

QA real sin audio: «Abrir el reposo diario con tareas del 11 de octubre de 2026» abrió daily con fecha correcta y refresh calendar/tasks; «Abrir el reposo semanal» abrió weekly siguiendo el día actual. Calendar devolvió HTTP 200. Se observó weekly → rest después de tres minutos reales sin reinicios. Pruebas unitarias cubren el umbral temporal y rollover; preview prueba el ciclo completo.
