# Pantallas

El firmware dibuja ocho vistas dinámicas en la RLCD monocromática de 400 × 300: inicio, alarmas, tareas, calendario, música, reposo, escucha y transcripción. No contiene controles táctiles ni datos de demostración como fuente de información.

KEY: 1× siguiente, 2× escucha, 3× o más anterior; espera de 380 ms. Voz automática: tres bloques consecutivos de 20 ms con RMS estrictamente mayor al umbral configurado. Reposo mantiene el equipo encendido.

Implementación, contrato de datos, importación USB y pruebas: [Firmware refactorizado](firmware-refactor.md). Diseño de referencia: `preview/handoff/screen-spec.json` y `preview/handoff/contact-sheet.png`.

Compilación y pruebas de reglas no verifican legibilidad ni micrófono en hardware. El dashboard de referencia sigue siendo local; sus cambios aún no se publican al servicio.
