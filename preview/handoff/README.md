# Compañero · Kit de implementación

Este paquete deja preparado el diseño aprobado. Todos los archivos se generan dentro de `preview/handoff`; no modifica `firmware`, no compila y no carga la placa.

## Entradas separadas

- `../index.html`: estudio de las diez pantallas, KEY, nivel RMS y galería.
- `../dashboard.html`: gestión mediante formularios, sin chat de IA.
- `../store.js`: contrato, validación, migración compatible de V2 y almacenamiento local compartido.
- `../screens.js`: renderizador de referencia independiente. `CompanionScreens.render(config, runtime, now)` devuelve el contenido de una pantalla, sin leer almacenamiento, operar la navegación o llamar servicios.
- `../app.js`: simulación de KEY y estados de voz. No es código de firmware.
- `../dashboard.js`: edición y configuración web local.

## Recursos preparados

| Archivo | Uso |
| --- | --- |
| `screen-spec.json` | Medidas reales de las regiones en píxeles, jerarquía de texto, navegación y tiempos |
| `config.schema.json` | Esquema JSON de configuración V2, con integraciones opcionales |
| `sample-config.json` | Datos de ejemplo para revisar el diseño; no usar como datos reales |
| `screens/*.png` | Referencias de 400 × 300: diez vistas y escucha en captura/procesamiento |
| `mono/*.png` | Revisión visual de esas referencias a 1 bit |
| `mono/*.bin` | Búferes de pantalla de exactamente 15.000 bytes, sin encabezado |
| `mascot.png` | Mascota recortada de 210 × 202, separada del texto |
| `mono/mascot.bin` | Mascota monocromática de 5.454 bytes, stride de 27 bytes por fila |
| `contact-sheet.png` | Comparación de las diez vistas |

Empaquetado: filas de arriba hacia abajo; dentro de cada byte, bit más significativo primero; `1` significa negro y `0` blanco. La mascota tiene 210 píxeles de ancho; cada fila ocupa 27 bytes y los seis bits sobrantes se ignoran. Verificar polaridad y orientación contra el driver concreto antes de enviar un búfer. Las referencias no comprueban cómo se verá la RLCD física.

Los PNG completos contienen datos de demostración. Usarlos para comparar o para una primera prueba estática; la implementación final debe dibujar los textos, listas y calendario con datos dinámicos. No reemplazar el firmware entero por una secuencia de capturas.

## Pantallas y fuentes de datos

| Vista | Campos dinámicos | Región y comportamiento |
| --- | --- | --- |
| Inicio | nombre, hora local, fecha, pendientes, alarmas activas, próximo evento | mascota de 210 px; reloj grande; agenda breve |
| Alarmas | hora, etiqueta, repetición, estado | 2 filas por página; estado informativo |
| Tareas | título, fecha, completada | 4 filas por página; pendientes primero |
| Calendario | año, mes, hoy, próximos eventos | calendario de lunes a domingo y 3 eventos por página |
| Música | título, artista, posición, duración, estado, volumen | disco decorativo; sin pausa, siguiente ni anterior táctiles |
| Reposo | hora, mes vigente, hoy y próximo evento | mascota grande; calendario opcional |
| Escucha | fase, RMS y umbral | waiting → recording → processing; anillo y medidor |
| Respuesta | texto de respuesta | reemplaza el ejemplo por texto real; doble KEY vuelve a voz |

El origen es la esquina superior izquierda, con coordenadas lógicas 400 × 300. Los rectángulos medidos y tamaños tipográficos están en el manifiesto. La referencia usa DM Sans para el cuerpo y Outfit para encabezados. En la placa se deben usar fuentes rasterizadas compatibles con el renderizador elegido, incluyendo caracteres españoles; los tamaños de CSS no son un formato de fuente para ESP32. Mantener truncamiento y el espacio reservado al pie para evitar solapamientos.

Si hay más alarmas, tareas o eventos de los que caben, rotar las páginas cada 6 segundos y mostrar el índice. Recalcular el calendario con la hora local del dispositivo; no dejar octubre de 2026 incrustado.

## Navegación física

Consulta: inicio → alarmas → tareas → calendario → música → reposo → inicio.

KEY se agrupa durante 380 ms después de cada pulsación: una avanza, dos abren escucha, tres retroceden. Desde voz, siguiente/anterior se calcula respecto de la vista de consulta guardada. Cuatro o más pulsaciones se tratan como anterior en la preview. Elegir si conservar esta tolerancia al portar el controlador.

El controlador real también necesita antirrebote según el botón; la preview sólo recibe clics ya resueltos. Evitar llamadas bloqueantes de red en el procesamiento de KEY. El reposo visual se activa tras 15/30/60/120 segundos; no implica deep sleep.

Voz: RMS estrictamente mayor al umbral elegido, 500 u 800, durante tres bloques de 20 ms. Capture termina tras 900 ms bajo el umbral o a los 12 segundos. Estos son los parámetros de la preview; comprobar la calibración y el fin por silencio sobre hardware. El reconocimiento por amplitud no discrimina voz de otros sonidos. La respuesta del dispositivo depende de servicios aún por integrar.

## Contrato web → servicio → placa

`version: 2` se conserva para importar los archivos anteriores. Se añaden `integrations.calendar` y `integrations.music` opcionales al cargar; la exportación actual siempre los incluye. El validador también exige IDs únicos por lista, fechas existentes, textos no vacíos y posición de música menor o igual a duración, restricciones que no todas las herramientas de JSON Schema comprueban.

- `settings`: nombre, umbral, activación automática, segundos hasta reposo, calendario en reposo.
- `tasks`, `alarms`, `events`: listas con IDs estables; editar conserva el ID.
- `music`: estado de reproducción de referencia.
- `integrations.calendar`: vinculación de demostración, correo, calendario elegido, visibilidad de eventos y fecha de sincronización simulada.
- `integrations.music`: proveedor preferido y destino de demostración.

La cuenta de Google, los calendarios Principal/Trabajo/Personal y el botón de sincronización son simulados. Cambiar la selección no obtiene ni filtra calendarios reales; la agenda conserva la misma lista local. El futuro adaptador debe descubrir los IDs reales y servir los eventos del calendario elegido. `showOnDevice` filtra los eventos que presenta el simulador; el mes en reposo se controla por separado con `settings.calendarRest`.

OAuth, tareas persistidas en servidor, sincronización real de Calendar, reproducción externa y autenticación del dispositivo requieren adaptadores posteriores. No incluir secretos de proveedores en este JSON. `pair.html` sigue siendo el flujo anterior, con llamadas reales si se configura y se usa; no se invoca automáticamente desde la preview.

## Regeneración y revisión

Desde `preview`:

```powershell
node prepare-screens.cjs
python pack-screens.py
node verify.cjs
```

Preparación y pruebas usan Playwright y Edge headless. La ruta de Playwright predeterminada corresponde al runtime local de Codex; se puede indicar otra ruta mediante `PLAYWRIGHT_MODULE`. El empaquetado necesita Pillow. Los PNG y geometría se regeneran con el instante fijo del manifiesto; si no hay acceso a las fuentes web, se usarán fuentes de respaldo y el resultado tipográfico puede variar.

Revisar los PNG de 1 bit, los estados vacíos, textos largos y el calendario de seis semanas antes de integrar. En placa comprobar contraste, orientación, acentos, legibilidad del calendario y latencia de KEY/voz. Una captura correcta o una prueba web no verifica estos aspectos físicos.

## Reposo diario y semanal

Las referencias `daily` y `weekly` se suman al ciclo de consultas. El reposo rota `rest → daily → weekly → rest`, cada 180000 ms; en firmware la rotación pausa mientras graba, reproduce audio o ejecuta una acción. Diario combina eventos con tareas; semana comienza el lunes. KEY: 1× escuchar, 2× siguiente, 3× anterior. El preview usa demo local; el ESP32 usa Calendar real y tareas del propietario autenticado. Campos opcionales de eventos: `endTime`, `endDate`, `allDay`.
