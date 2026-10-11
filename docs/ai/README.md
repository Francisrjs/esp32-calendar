# Asistente de IA: vista todavía visual

Las vistas de escucha y respuesta representan una interacción simulada. El texto de respuesta es fijo; el anillo y el movimiento se generan localmente en la vista previa. El firmware nuevo navega por esas vistas, pero todavía no captura voz, no reproduce sonido y no envía solicitudes a servicios de IA.

## Estados

Escuchando → hablando → escuchando, mediante el círculo central en la vista de PC. Pausa congela la animación. Volver al inicio cierra esa vista. En la placa KEY navega entre pantallas.

## Integración futura

La placa confirmada incluye arreglo dual ES7210, códec ES8311 y salida para parlante. Antes de habilitar audio hay que integrar los controladores compatibles, fijar el modelo/voz/API, añadir límites de gasto y validar captura, reproducción, eco, memoria y latencia en hardware. Las claves de proveedor permanecen sólo en el servidor. Los iconos de micrófono no habilitan por sí mismos esos componentes.
