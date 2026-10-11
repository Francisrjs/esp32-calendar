# Panel web · ESP32 Calendar

Aplicación Next.js para configurar el ESP32 desde una cuenta Supabase.

## Desarrollo

Requiere Node.js 20 o superior. Creá `.env.local` usando `.env.example` y completá:

- `NEXT_PUBLIC_SUPABASE_URL`
- `NEXT_PUBLIC_SUPABASE_PUBLISHABLE_KEY`

No uses una clave `service_role` en variables `NEXT_PUBLIC_*`.

```powershell
npm ci
npm run dev
```

## Netlify

En este monorepo, usar como **Base directory** `ESP32-MIC-AI/front-end` y como **Build command** `npm run build`. El `netlify.toml` de la raíz ya fija esa base. Netlify debe detectar Next.js; no configurar una carpeta de publicación estática.

Agregar las dos variables `NEXT_PUBLIC_SUPABASE_*` en la configuración del sitio. La app necesita funciones de servidor de Next.js y no usa `output: export`.

## Sección Energía

La página Energía permite explorar perfiles y horarios como un borrador temporal. El contrato actual de Supabase y el firmware todavía no exponen la configuración, capacidades ni revisión aplicada de energía, así que el panel no persiste esos controles ni afirma que la placa los recibió. Al recargar la página se descarta el borrador.

La autonomía sigue como no medida. El panel muestra los metadatos informados de la batería y la diferencia aritmética para revisión, sin corregir el valor de etiqueta ni estimar horas restantes.

## Verificaciones

```powershell
npm run test
npm run typecheck
npm run build
```
