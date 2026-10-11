# Reposo visual

Mascota recortada de 210 × 202, reloj real y calendario mensual opcional. En firmware se activa tras 15, 30, 60 o 120 segundos de inactividad según `settings.idle`. No tiene navegación táctil.

KEY 1× avanza a Inicio, 2× abre escucha y 3× o más retrocede a Música. Voz automática sigue activa con `autoVoice` habilitado. El calendario depende de `calendarRest`, independientemente de la visibilidad de eventos.

No se activa deep sleep. Hora desde RTC/NTP en ART3. La nueva implementación requiere validación física; ver [firmware refactorizado](firmware-refactor.md).
