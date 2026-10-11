# Compañero · preview V3

Dos vistas independientes con el diseño aprobado:

- **`index.html`**: estudio del dispositivo con diez pantallas, navegación KEY, simulación de RMS y galería.
- **`dashboard.html`**: panel web sin chat, para gestionar tareas, alarmas, Google Calendar, música, dispositivo y conexiones.

Servir esta carpeta con `python -m http.server 8765 --bind 127.0.0.1` y abrir http://127.0.0.1:8765/dashboard.html o http://127.0.0.1:8765/index.html. No requiere build. Se recomienda HTTP local para compartir configuración de forma consistente entre pestañas. Las fuentes web son opcionales y tienen respaldo.

## Dashboard separado

Formularios para crear/editar/eliminar tareas, alarmas y eventos; tareas completadas y alarmas activas; filtros de tareas; calendario mensual navegable con selección de día; configuración de cuenta y calendario Google simulados; música, volumen y reproductor externo; nombre, reposo y umbral de voz; importación/exportación JSON.

No hay conversación ni comandos del asistente en el dashboard. La vinculación de Google pide sólo un correo de ejemplo y guarda una preferencia local. «Sincronizar demo» actualiza una marca de tiempo; no obtiene eventos. Spotify permanece como preferencia para una futura integración. No se accede a cuentas reales.

La página anterior `pair.html` se conserva separada; si se abre y configura, puede usar el backend real. La preview no invoca su flujo.

## Pantallas preparadas

El renderizador `screens.js` está separado de `app.js` y recibe configuración, estado de ejecución y fecha explícitos. La guía, contrato de datos, posiciones, doce estados de referencia, mascota recortada y búferes de 1 bit están en **`handoff/`**. Ver [guía](handoff/README.md).

La pantalla no contiene controles táctiles. KEY: 1× siguiente, 2× escucha, 3× anterior; agrupación de 380 ms. Reposo conserva perrito, reloj y calendario configurable. Música presenta estado y progreso. Si hay más datos de los que caben, las páginas de alarmas/tareas/eventos rotan cada 6 segundos.

La voz es simulada: >500 o >800 RMS durante tres bloques de 20 ms, fin por 900 ms bajo umbral o a los 12 s. No se graba ni se usan servicios de IA. La tecla K también simula KEY cuando no se está escribiendo.

## Configuración compartida

`store.js` centraliza contrato, validación y almacenamiento en `companero-preview-v2`. Mantiene compatibilidad con exportaciones V2 y agrega integraciones de demostración opcionales. Abrir dashboard y estudio bajo el mismo origen permite ver los cambios en otra pestaña mediante eventos de almacenamiento. Los datos de ejemplo nunca se envían al firmware ni al backend.

## Comprobación

`node verify.cjs`: pruebas funcionales con Playwright/Edge headless. `node prepare-screens.cjs` y `python pack-screens.py`: regeneración de referencias y recursos de 1 bit. Las dependencias y la preparación están documentadas en `handoff/README.md`.

Capturas actuales: `dashboard-desktop.png`, `dashboard-calendar.png`, `dashboard-mobile.png`, `studio-desktop.png`. Las imágenes originales de `assets/`, `contact-sheet.png`, `dashboard-music.png` y `screen-rest.png` se conservan como historial. Las referencias actuales de todas las pantallas están en `handoff/screens/` y `handoff/contact-sheet.png`.

Se usó el contexto de `docs/screen`, `docs/voice-bringup.md`, `docs/implementation-plan.md` y `logs/audio-diagnostic.log`. Sólo se modificó `preview`; firmware y servicios reales permanecen pendientes de implementación.

## Reposo diario y semanal

`daily` reproduce la composición de `agenda-semana.html`: agenda del día con barras horarias y tareas del día, pendientes vencidas o sin fecha. `weekly` muestra lunes a domingo, bloques de eventos y cantidades de tareas por fecha. Las listas largas del diario paginan cada 6 segundos. El ciclo de reposo es `rest → daily → weekly → rest`, con 180000 ms por pantalla. KEY: una pulsación abre voz; dos avanzan; tres vuelven.

El preview usa los datos compartidos de demostración del dashboard local; no descarga tu Calendar. El firmware sí utiliza Google Calendar vinculado a Supabase, obtiene desde el lunes de la semana seleccionada hasta dos semanas después y utiliza comienzo y fin de los eventos. Acciones: «Abrí el reposo diario», «Abrí el reposo semanal», «Abrí el día uno con las tareas» (lunes de la semana actual), «Abrí el día 18 de octubre con tareas». Una fecha explícita fija la vista hasta el siguiente cambio automático de reposo; luego vuelve al día actual. Los datos grandes de agenda quedan en memoria y se descargan después de reiniciar.

Si no hay tareas para mostrar en el diario (del día, pendientes vencidas o sin fecha), el ciclo automático omite `daily`: alterna `rest ↔ weekly`, conservando los 3 minutos por pantalla, aunque haya eventos de Calendar. La apertura manual del diario sigue disponible.
