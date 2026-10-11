import { createBrowserClient } from "@supabase/ssr";

const url = process.env.NEXT_PUBLIC_SUPABASE_URL;
const key = process.env.NEXT_PUBLIC_SUPABASE_PUBLISHABLE_KEY;

export const isSupabaseConfigured = Boolean(url && key);
export const supabase = createBrowserClient(
  url || "https://placeholder.supabase.co",
  key || "placeholder-key",
  { auth: { autoRefreshToken: true, detectSessionInUrl: true, persistSession: true } },
);

export const backendUrl = () => {
  if (!url) throw new Error("Falta configurar NEXT_PUBLIC_SUPABASE_URL.");
  return `${url.replace(/\/$/, "")}/functions/v1/esp32-api`;
};
