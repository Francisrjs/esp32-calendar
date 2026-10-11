# Módulos

## Diseño y recursos

`generate_assets.py` renderiza las siete páginas del PDF con Poppler. Recorta los márgenes de las primeras tres y normaliza a 400 × 300. `visual_design.py` dibuja los componentes refinados a tres veces la resolución, usando fuentes locales de Windows. Requiere Pillow, Poppler y Segoe UI.

La salida consta de siete PNG en `preview/assets` y `firmware/src/screens.h`. Para firmware, cada fila ocupa 50 bytes; el bit más significativo representa el primer píxel. Un bit activo en el recurso significa negro. El controlador convierte esa convención al formato físico de la RLCD.

## Vista de PC

`preview/index.html`, `styles.css` y `app.js` presentan recursos locales, zonas pulsables y animaciones. El estado vive en memoria y se reinicia al recargar. La vista no necesita servidor ni conexiones externas.

## Firmware

`firmware/src/main.cpp` maneja la secuencia, las transiciones y los movimientos. Dibuja directamente con el controlador original, sin LVGL en este prototipo. No implementa navegación táctil ni edición de datos en placa.

`display_bsp.cpp` y `display_bsp.h` provienen de `first -pio`. Inicializan SPI, el panel, su búfer y las tablas de posiciones en PSRAM. La configuración conserva los pines de la primera versión.

| Señal | GPIO |
| --- | --- |
| MOSI | 12 |
| SCLK | 11 |
| DC | 5 |
| CS | 40 |
| RESET | 41 |

La pantalla emplea SPI3 a 10 MHz. El búfer físico ocupa 15.000 bytes y las tablas de conversión aproximadamente 360.000 bytes de PSRAM; estas asignaciones dinámicas no aparecen completas en el resumen de RAM estática de PIO.
