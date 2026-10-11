# Voz: estado y diagnóstico (10 de octubre de 2026)

La placa Waveshare ESP32-S3-RLCD-4.2 usa ES7210 con captura TDM de cuatro
canales: canal 0 para micrófono, canal 1 como referencia del DAC. Se reutiliza
el controlador esp_codec_dev 1.3.5 del fabricante y la interfaz I2S de IDF 4.4.
La ganancia se aplica **después** de habilitar el ADC: el encendido reescribe
los registros de ganancia. Confirmado por USB: registros 43–46 = 1A (30 dB).

La activación predeterminada requiere RMS mayor a 500 durante tres bloques
consecutivos de 20 ms (o un umbral mayor si el ruido ambiente lo exige),
según la preferencia del usuario. No se procesan aplausos en modo voz.
La captura incluye un búfer previo de
300 ms; también existe detección de dos aplausos. Esto no reconoce una palabra
clave y puede activarse por otros sonidos. El medidor en la vista de escucha
muestra el nivel RMS real. Doble KEY inicia captura manual.

Una tarea de audio posee I2S y el búfer PCM; el hilo principal usa I2C y
pantalla, mientras HTTPS corre en una tarea separada. La grabación termina por silencio o
a los 12 segundos. Sin suficiente actividad se descarta sin llamar a OpenAI.
La subida mantiene el búfer inmutable hasta terminar. La memoria se reserva
una sola vez; el PCM está en PSRAM y la cola entrega eventos al hilo principal.

Supabase sirve `voice-status` y `voice-transcribe`, ambos autenticados mediante
el token de la placa. El segundo admite sólo WAV PCM mono de 16 kHz / 16 bits.
El modelo solicitado es `whisper-1`, idioma español. `OPENAI_API_KEY` está
configurada como secreto de Edge Functions, nunca en firmware ni SD.
La cuota atómica limita 60 intentos/día y 1000/mes por propietario. Los intentos
fallidos también consumen reserva para evitar reintentos pagos sin límite.
No se guardan audio ni transcripciones en el backend. Whisper sólo transcribe;
no crea tareas ni eventos y no genera voz hablada en esta etapa.

## Comandos USB a 115200 baudios

```
audio status
audio regs
audio debug on
audio debug off
voice status
listen
wake voice
wake clap
clap off
clap on
status
```

`clap off/on` deshabilita/habilita toda activación automática; `listen` sigue
permitiendo la captura manual. Los registros y las muestras de diagnóstico se
guardaron en `logs/audio-diagnostic.log`.

El ES8311 está inicializado con PA GPIO46 y salida GPIO8. El pitido se usa en
modo aplausos; en activación por voz se omite para no tapar las primeras palabras.
La captura automática ya se observó por USB: RMS 945 y WAV de 126124 bytes.
El diagnóstico previo devolvió HTTP 401. El usuario confirmó después que su
clave ya funciona; esta sesión no cambió secretos ni repitió una transcripción
paga. `voice status` informa `provider_status` sin mostrar la clave.
La refactorización compilada usa umbral estricto de 500/800, cierre por 900 ms
de silencio y KEY 1×/2×/3×+. Todavía no se cargó esta versión ni se probó su
transcripción en placa. La reproducción audible sigue pendiente. Rest mantiene
el equipo encendido. Ver [vistas y voz](screen/firmware-refactor.md).

Fuentes:
- [OpenAI: transcripciones](https://developers.openai.com/api/reference/resources/audio/subresources/transcriptions/methods/create)
- [Supabase: secretos](https://supabase.com/docs/guides/functions/secrets)
- Pines: `07_Audio_Test/src/ExternLib/codec_board/board_cfg.h` del fabricante.
- TDM: `XiaoZhiCode_V2.1.0/main/audio/codecs/box_audio_codec.cc` del fabricante.

## Prueba de voz artificial (2026-10-10)

Después de `voice-transcribe`, el dispositivo solicita `voice-speak` y reproduce el texto con voz sintética. La clave permanece en el backend. El endpoint usa autenticación del dispositivo y la cuota de voz existente (la síntesis también consume una reserva). Límite: 600 caracteres y 40 segundos; los textos mayores permanecen en pantalla sin lectura. PCM mono 16 bits a 16 kHz, convertido desde los 24 kHz de OpenAI; buffer PSRAM limitado a 1.28 MB. La tarea de audio posee y libera el buffer. Durante solicitud/reproducción se bloquea la captura; al terminar suena el pitido y hay un segundo de protección contra eco. Después sigue escuchando con umbral 800 RMS en la vista de resultado. Comando de diagnóstico: `say Hola, prueba de voz artificial`.

Formato del proveedor: https://developers.openai.com/api/docs/guides/text-to-speech

Ajuste de volumen: la voz recibe ganancia digital de hasta 4× (+12 dB), reducida automáticamente según el pico del PCM para evitar recorte. El pitido mantiene su amplitud y el DAC permanece a -12 dB.

Segundo ajuste: DAC a 0 dB; voz normalizada hacia un pico del 80% de escala PCM (ganancia limitada a 8x). Esto es un objetivo digital, no una medición de potencia acústica. Pitido reducido de 6000 a 1507 unidades PCM para compensar los +12 dB del DAC y conservar su nivel anterior.

Ajuste tras escucha: objetivo de pico de voz reducido del 80% al 65% (-1.8 dB aproximadamente) por ligera saturación audible. El DAC y el pitido conservan su ajuste anterior.

Umbral solicitado: 1200 RMS, configurado en Supabase y permitido en firmware, acciones y dashboards. La respuesta usa max(800, umbral configurado) para conservar la menor sensibilidad. Captura siempre activa: elevar RMS reduce disparos pero no apaga I²S ni codecs. Consumo/autonomía de la placa no medidos; requieren medición de corriente en alimentación/batería.
