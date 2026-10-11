# Plan de refactorización de las vistas del ESP32

Fecha: 10 de octubre de 2026.
Estado: vistas dinámicas y detección de voz portadas y compiladas; validación física y publicación desde el dashboard pendientes.
Actualización de diagnóstico: cargado `views-v2` en COM4 con hash verificado. Logs confirman Inicio, doble KEY, captura de 5760 ms, silencio de 900 ms y transcripción HTTP 200. Legibilidad y exactitud de transcripción siguen pendientes; ver `logs/views-voice-diagnostic.log`.
Plan relacionado: [Implementación del dashboard](implementacion-dashboard.md).

## Objetivo

Llevar las ocho vistas aprobadas a la RLCD monocromática de 400 × 300, eliminando la interacción táctil y usando un único botón KEY y activación por voz. La configuración se realiza desde el dashboard separado; el dispositivo presenta información y estados del asistente.

El diseño está portado al firmware. No se cargó la placa en esta sesión. Ver [implementación y límites](../docs/screen/firmware-refactor.md).

## Base preparada

- [x] Estudio independiente en `preview/index.html` y simulador en `preview/app.js`.
- [x] Renderizador separado en `preview/screens.js`, con configuración, estado y fecha como entradas.
- [x] Ocho vistas sin zonas táctiles ni iconos de navegación, más escucha en captura/procesamiento.
- [x] Navegación simulada KEY 1×/2×/3× y activación por umbral de audio.
- [x] Perrito grande, calendario en reposo y música sin botones táctiles.
- [x] Paginación automática de listas cada seis segundos.
- [x] Medidas, contrato, capturas y recursos monocromáticos preparados en `preview/handoff/`.
- [x] Pruebas web de navegación, estados de voz y actualización desde el dashboard.

Referencias: [guía del kit](../preview/handoff/README.md), [especificación de pantallas](../preview/handoff/screen-spec.json), [galería](../preview/handoff/contact-sheet.png), [documentación de voz](../docs/voice-bringup.md) y [documentación previa de pantallas](../docs/screen/README.md).

## Diseño por vista

| Vista | Resultado previsto |
| --- | --- |
| Inicio | Perrito grande, hora de 24 horas, fecha, pendientes y próximo evento |
| Alarmas | Dos horarios por página, nombre, repetición y estado; sin interruptores ni «+» |
| Tareas | Cuatro filas por página, pendientes primero y estado de completado informativo |
| Calendario | Mes vigente, hoy y próximos eventos; sin selección táctil |
| Música | Disco decorativo, título, artista, estado, volumen y progreso; sin controles táctiles |
| Reposo | Perrito, reloj y calendario opcional; sin barra de navegación |
| Escucha | Espera, captura y procesamiento, con medidor RMS |
| Respuesta | Texto del asistente; doble KEY permite volver a escuchar |

## Etapa 1 · Separar datos, estado y dibujo

- [x] Revisar el renderizador y los recursos actuales del firmware antes de sustituirlos.
- [x] Crear un modelo de datos de pantalla separado del controlador de navegación y audio.
- [x] Convertir el contrato web a estructuras compactas adecuadas para la placa.
- [x] Dibujar títulos, listas, hora y calendario dinámicamente; reservar bitmaps para la mascota y recursos estáticos.
- [x] Elegir fuentes compatibles con el renderizador e incluir acentos, ñ y signos necesarios.
- [ ] Mantener medidas, truncamiento y espacio del pie definidos en la especificación.

Aceptación: cada vista puede dibujarse con datos de prueba, sin texto de demostración incrustado como fuente definitiva de información.

## Etapa 2 · Recursos y primera revisión física

- [ ] Usar la mascota recortada y verificar escala, contraste y tramado en la RLCD.
- [ ] Comprobar orientación y polaridad del búfer contra el driver concreto.
- [ ] Revisar legibilidad de reloj, listas, calendario y estados en el tamaño físico.
- [ ] Ajustar tipografía y detalles de contraste si el resultado físico lo exige.

Recursos: diez búferes completos de 15.000 bytes cada uno; filas de arriba abajo, bit más significativo primero, `1` negro. La mascota de 210 × 202 tiene stride de 27 bytes y ocupa 5.454 bytes. Ignorar los seis bits de relleno al final de cada fila de mascota. Las capturas completas sirven como referencia o prueba estática inicial, no como solución final para datos dinámicos.

Aceptación: no hay recortes ni inversión de colores; la información principal puede leerse en la placa.

## Etapa 3 · Controlador KEY

- [x] Implementar antirrebote del botón físico y agrupación no bloqueante de pulsaciones.
- [x] Esperar 380 ms desde la última pulsación antes de ejecutar la acción.
- [x] Una pulsación avanza; dos abren escucha; tres retroceden.
- [x] Mantener el ciclo inicio → alarmas → tareas → calendario → música → reposo → inicio.
- [x] Guardar la última vista de consulta al entrar en voz; calcular siguiente/anterior respecto de ella.
- [x] Definir el tratamiento definitivo de cuatro o más pulsaciones; la preview las interpreta como anterior.
- [x] Mantener KEY operativo durante captura, procesamiento y solicitudes de red.

Aceptación: doble y triple pulsación no ejecutan primero una acción simple. La navegación responde sin bloquear el audio o la pantalla.

## Etapa 4 · Activación y estados de voz

- [x] Integrar los eventos del sistema de audio existente con el estado visual del asistente.
- [x] Aplicar RMS estrictamente mayor a 500 u 800 según configuración.
- [x] Exigir tres bloques consecutivos de 20 ms y reiniciar el contador cuando no se supera el umbral.
- [x] Permitir activación desde vistas de consulta y reposo si el modo automático está habilitado.
- [x] Con modo automático deshabilitado, abrir escucha mediante doble KEY.
- [ ] Calibrar fin de captura por silencio; la referencia usa 900 ms bajo el umbral y un máximo de 12 segundos.
- [x] Mostrar espera → captura → procesamiento → respuesta, además de errores y ausencia de servicio.
- [x] Reemplazar la frase simulada por resultados del servicio real cuando estén disponibles.

Aceptación: valores iguales al umbral no activan captura; picos aislados no cumplen los tres bloques. Comprobar con voz, ruido ambiente y audio del parlante. La detección por amplitud no equivale a una palabra de activación.

## Etapa 5 · Datos dinámicos y sincronización

- [x] Actualizar reloj y fecha desde la fuente real de hora, con zona horaria definida.
- [x] Calcular calendario vigente, meses de seis semanas y cambios de mes/año.
- [ ] Conectar tareas, alarmas, eventos y preferencias recibidos del servicio.
- [ ] Conectar estado de música desde el reproductor externo, sin añadir controles táctiles.
- [x] Mostrar estados vacíos y datos disponibles sin conexión; indicar cuando una información esté desactualizada.
- [x] Rotar cada seis segundos las listas que excedan dos alarmas, cuatro tareas o tres eventos por página.
- [x] Evitar que una sincronización en segundo plano cambie la vista elegida por el usuario.

Aceptación: los cambios confirmados por el dashboard aparecen en la placa; las listas pueden recorrerse completas y no conservan fechas o ejemplos fijos.

## Etapa 6 · Reposo y actualización visual

- [x] Entrar en reposo tras el tiempo configurado: 15, 30, 60 o 120 segundos de inactividad.
- [x] Conservar mascota, hora y mes si `calendarRest` está habilitado.
- [x] Mantener separado el calendario mensual de la visibilidad de eventos `showOnDevice`.
- [x] Volver a la navegación o a voz con KEY sin perder la configuración.
- [x] Actualizar la pantalla con una frecuencia adecuada al driver y evitar redibujos innecesarios.

Aceptación: reposo sigue siendo visual y no activa deep sleep. KEY y la activación de voz configurada continúan funcionando.

## Etapa 7 · Verificación y entrega

- [x] Compilar con el entorno existente y revisar uso de memoria y recursos.
- [ ] Probar ocho vistas, fases de voz, datos vacíos, textos largos y listas con varias páginas.
- [ ] Verificar KEY simple/doble/triple en hardware y durante operaciones de audio/red.
- [ ] Probar reinicio, desconexión, recuperación de hora y recepción de configuración.
- [ ] Comparar con las referencias de `handoff` y registrar ajustes físicos justificados.
- [x] Actualizar la documentación de pantallas para reflejar el firmware realmente implementado.

Orden recomendado: modelo/dibujo → recursos físicos → KEY → estados de voz → datos reales → reposo → validación integral. La integración de datos depende del plan del dashboard; el dibujo y KEY pueden desarrollarse antes con fixtures.

## Límites del estado actual

Compilación PlatformIO exitosa: Espressif32 6.12.0 / Arduino 2.0.17, códec del fabricante 1.3.5. Flash: 1.052.093 bytes; RAM estática: 64.112 bytes. Las pruebas de reglas evaluadas por el compilador ESP32 cubren umbrales estrictos, picos aislados, bloques consecutivos, silencio, máximo de captura, KEY, antirrebote, rollover y fechas bisiestas.

Implementados modelo y renderizador separados, ocho vistas sin controles táctiles, mascota recortada, caracteres latinos, calendario dinámico, paginación, KEY agrupado y reposo configurable. Datos disponibles persistidos en NVS. La transcripción usa el servicio real en una tarea de red separada; KEY continúa operativo y los resultados no fuerzan navegación si el usuario salió de voz.

Pendiente comprobar orientación, contraste, textos largos, legibilidad y ruido en hardware. El cierre por silencio está implementado a 900 ms, pero su calibración física sigue pendiente. No se ha cargado la placa en esta sesión. El usuario informa que su clave funciona; no se cambiaron secretos ni se hizo una transcripción paga.

Tareas y eventos usan el backend actual; alarmas, música y preferencias V2 admiten importación USB persistente y futuros campos del snapshot. El dashboard aún guarda esos campos localmente: su publicación al backend depende del plan de dashboard. Máximo de 50 elementos por lista y 64 KiB por respuesta HTTPS. El servicio transcribe; interpretación de acciones, conversación y respuesta audible siguen pendientes.
