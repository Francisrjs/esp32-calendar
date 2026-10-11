# Compilación y carga

## Placa y conexiones

Target: Waveshare ESP32-S3-RLCD-4.2, ESP32-S3-WROOM-1-N16R8, 16 MB flash y 8 MB PSRAM. La pantalla es RLCD monocroma de 400 × 300.

| Función | GPIO / bus |
| --- | --- |
| Display SPI: MOSI, SCLK, DC, CS, reset | 12, 11, 5, 40, 41 |
| I²C: SDA, SCL | 13, 14 |
| KEY | 18, activo en LOW |
| RTC PCF85063 | I²C, dirección 0x51 |
| Sensor SHTC3 | I²C, dirección 0x70 |

ES7210 y ES8311 usan el controlador del fabricante 1.3.5. I²S: MCLK16, BCLK9, WS45, DIN10, DOUT8 y PA46. BOOT queda reservado al modo descarga; PWR controla la alimentación.

PlatformIO usa Espressif32 6.12.0 y Arduino. huge_app.csv deja 3 MB para la aplicación; no hay OTA implementado. El ID de placa base es esp32-s3-devkitc-1; la configuración de PlatformIO fija N16R8.

## Compilar y cargar

Desde ESP32-MIC-AI:

    python generate_assets.py
    pio run -d firmware
    pio device list
    pio run -d firmware -t upload --upload-port COM4
    pio device monitor --port COM4 --baud 115200

El puerto COM puede cambiar al reconectar. Cerrá otros monitores serie antes de cargar.

## Wi-Fi, Supabase y pairing

Copiá firmware/src/device_secrets.h.example a firmware/src/device_secrets.h y completá la publishable key y el PEM de la CA de TLS del endpoint. Son configuración pública de cliente; nunca pegues service_role, claves de Google u OpenAI en el firmware. device_secrets.h está excluido del control de versiones.

Desde la carpeta del proyecto, iniciá un servidor para la página de acceso con python -m http.server 8000 y abrí http://localhost:8000/preview/pair.html. En la configuración Auth de Supabase, permití ese redirect local. En el monitor serial, usá wifi SSID|PASSWORD; después iniciá sesión, creá un código temporal y escribí pair CODIGO en el monitor. El dispositivo guarda las credenciales Wi-Fi y el token de dispositivo en NVS. task TITULO crea una tarea sin fecha; event TITULO|START_ISO8601|END_ISO8601 crea un evento de Calendar (horarios ISO 8601 con zona, por ejemplo -03:00). sync fuerza la descarga; status informa conectividad, reloj, pairing y heap. La sincronización periódica ocurre cada cinco minutos.

KEY 1× avanza por inicio → alarmas → tareas → calendario → música → reposo; 2× abre captura de voz; 3× o más retrocede. Agrupación de 380 ms y antirrebote de 45 ms. No completa tareas por pulsación larga. Ver [vistas y voz](../screen/firmware-refactor.md) para importar configuración, calibrar y ejecutar pruebas.

OAuth de Google requiere configurar GOOGLE_CLIENT_ID, GOOGLE_CLIENT_SECRET, GOOGLE_REDIRECT_URI en los secretos de la Edge Function esp32-api. La URI de callback es https://ofyohmwlhvinrgsgjuul.supabase.co/functions/v1/esp32-api/calendar-callback; la conexión usa el calendario franrojas331@gmail.com. La credencial OAuth no se copia al dispositivo.

## Validación previa a la refactorización

El firmware está cargado en COM4 con hash verificado y SUCCESS, usando Espressif32 6.12.0 / Arduino 2.0.17. La última compilación ocupa 1,051,109 bytes de flash y 46,748 bytes de RAM estática. Se confirmó Wi-Fi conectado, RTC/hora local válidos, mediciones reales SHTC3, HTTPS HTTP 200 hacia esp32-api y snapshot exitoso. La placa quedó emparejada con la cuenta Auth solicitada franrojas331@gmail.com; hay cero tareas en esa cuenta. La creación Auth envió correo de acceso y requiere confirmación del usuario. La clave Wi-Fi se guardó únicamente en NVS del dispositivo; no se incorporó al código ni a la documentación.

Se corrigió la fuente: el bitmap de 15 bits se truncaba a 8 al dibujar, causando texto incompleto. Tareas usa el ancho de la pantalla y Calendario muestra el mes actual y el día subrayado aun sin OAuth. La hora y fecha de Inicio son reales; la zona POSIX ART3 permite recuperar la hora del RTC también sin red. device_secrets.h local contiene sólo la clave publicable y la CA obtenida de una cadena TLS verificada.

En el arranque persiste Incorrect size of core dump image: 516186, aunque la app continúa. No se borró la flash. El usuario confirmó que la hora y el calendario se ven correctamente y que KEY permite llegar a esa vista. Los eventos Google todavía necesitan secretos OAuth y consentimiento del backend.


La refactorización de vistas y voz se compiló en esta sesión, pero aún no se cargó. Las observaciones de hardware anteriores no validan el código nuevo.
