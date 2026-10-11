import { handleActions } from "./actions.ts";
import { createClient } from "npm:@supabase/supabase-js@2";

const corsHeaders = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Headers": "authorization, apikey, content-type",
  "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
  "Content-Type": "application/json; charset=utf-8",
};

const supabaseUrl = Deno.env.get("SUPABASE_URL") ?? "";
const secretKeys = JSON.parse(Deno.env.get("SUPABASE_SECRET_KEYS") ?? "{}");
const serviceKey = Deno.env.get("SUPABASE_SERVICE_ROLE_KEY") ?? secretKeys.default ?? "";
const admin = supabaseUrl && serviceKey
  ? createClient(supabaseUrl, serviceKey, { auth: { persistSession: false, autoRefreshToken: false } })
  : null;

function json(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), { status, headers: corsHeaders });
}

function requiredAdmin() {
  if (!admin) throw new ApiError(503, "supabase_server_key_missing");
  return admin;
}

class ApiError extends Error {
  status: number;
  constructor(status: number, message: string) {
    super(message);
    this.status = status;
  }
}

function randomToken(bytes = 32): string {
  const data = crypto.getRandomValues(new Uint8Array(bytes));
  let binary = "";
  for (const byte of data) binary += String.fromCharCode(byte);
  return btoa(binary).replaceAll("+", "-").replaceAll("/", "_").replaceAll("=", "");
}

async function sha256(value: string): Promise<string> {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(value));
  return Array.from(new Uint8Array(digest), (byte) => byte.toString(16).padStart(2, "0")).join("");
}

function bearer(req: Request): string {
  const match = /^Bearer\s+(.+)$/i.exec(req.headers.get("authorization") ?? "");
  if (!match) throw new ApiError(401, "bearer_token_required");
  return match[1];
}

async function jsonBody(req: Request): Promise<Record<string, unknown>> {
  const raw = await req.text();
  if (raw.length > 12_000) throw new ApiError(413, "request_too_large");
  try {
    const value = JSON.parse(raw);
    if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("object_required");
    return value;
  } catch {
    throw new ApiError(400, "invalid_json");
  }
}

function uuid(value: unknown): value is string {
  return typeof value === "string" && /^[0-9a-f]{8}-[0-9a-f]{4}-[1-8][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i.test(value);
}

function text(value: unknown, max: number): value is string {
  return typeof value === "string" && value.trim().length > 0 && value.length <= max;
}

async function authenticatedUser(req: Request) {
  const db = requiredAdmin();
  const { data, error } = await db.auth.getUser(bearer(req));
  if (error || !data.user) throw new ApiError(401, "invalid_user_session");
  return data.user;
}

async function authenticatedDevice(req: Request) {
  const db = requiredAdmin();
  const tokenHash = await sha256(bearer(req));
  const { data: device, error } = await db.from("esp32_devices")
    .select("id, owner_id, name, status, revoked_at")
    .eq("token_hash", tokenHash).eq("status", "active").is("revoked_at", null).maybeSingle();
  if (error || !device) throw new ApiError(401, "invalid_device_token");
  await db.from("esp32_devices").update({ last_seen_at: new Date().toISOString() }).eq("id", device.id);
  return device;
}

function googleConfig() {
  const clientId = (Deno.env.get("GOOGLE_CLIENT_ID") ?? "").trim();
  const clientSecret = (Deno.env.get("GOOGLE_CLIENT_SECRET") ?? "").trim();
  const redirectUri = (Deno.env.get("GOOGLE_REDIRECT_URI") ?? "").trim();
  if (!clientId || !clientSecret || !redirectUri) throw new ApiError(503, "google_oauth_not_configured");
  return { clientId, clientSecret, redirectUri };
}

async function googleAccessToken(ownerId: string): Promise<{ token: string; calendarId: string }> {
  const db = requiredAdmin();
  const { data: account, error } = await db.from("esp32_google_accounts")
    .select("calendar_id, refresh_token, access_token, access_token_expires_at")
    .eq("owner_id", ownerId).maybeSingle();
  if (error || !account) throw new ApiError(409, "google_calendar_not_connected");

  const expiry = account.access_token_expires_at ? Date.parse(account.access_token_expires_at) : 0;
  if (account.access_token && expiry > Date.now() + 60_000) {
    return { token: account.access_token, calendarId: account.calendar_id };
  }

  const { clientId, clientSecret } = googleConfig();
  const response = await fetch("https://oauth2.googleapis.com/token", {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: new URLSearchParams({
      client_id: clientId,
      client_secret: clientSecret,
      refresh_token: account.refresh_token,
      grant_type: "refresh_token",
    }),
  });
  const tokens = await response.json();
  if (!response.ok || typeof tokens.access_token !== "string") {
    throw new ApiError(502, "google_token_refresh_failed");
  }
  const expiresAt = new Date(Date.now() + Number(tokens.expires_in ?? 3600) * 1000).toISOString();
  const { error: saveError } = await db.from("esp32_google_accounts").update({
    access_token: tokens.access_token,
    access_token_expires_at: expiresAt,
  }).eq("owner_id", ownerId);
  if (saveError) throw new ApiError(500, "google_token_save_failed");
  return { token: tokens.access_token, calendarId: account.calendar_id };
}

async function handle(req: Request): Promise<Response> {
  if (req.method === "OPTIONS") return new Response("ok", { headers: corsHeaders });
  const url = new URL(req.url);
  const path = url.pathname.replace(/\/+$/, "").split("/").pop() ?? "";
  const db = requiredAdmin();
  const actionResponse = await handleActions(req,path,{db,authenticate:authenticatedDevice,calendar:googleAccessToken,json});
  if(actionResponse)return actionResponse;

  if (path === "health" && req.method === "GET") return json({ ok: true, service: "esp32-api" });
  if (path === "voice-status" && req.method === "GET") {
    await authenticatedDevice(req);
    const key = Deno.env.get("OPENAI_API_KEY");
    if(!key) return json({configured:false,model:"whisper-1"});
    try {
      const provider = await fetch("https://api.openai.com/v1/models/whisper-1", {
        headers:{Authorization:`Bearer ${key}`}, signal:AbortSignal.timeout(10000),
      });
      return json({ configured:true, model:"whisper-1", provider_status:provider.status });
    } catch { return json({configured:true, model:"whisper-1", provider_status:0}); }
  }

  if (path === "voice-speak" && req.method === "POST") {
    const device = await authenticatedDevice(req);
    const body = await jsonBody(req);
    if (!text(body.text, 600)) throw new ApiError(400, "speech_text_required_max_600");
    const key = Deno.env.get("OPENAI_API_KEY");
    if (!key) throw new ApiError(503, "openai_key_missing");
    const quota = await db.rpc("esp32_reserve_voice", { p_owner: device.owner_id });
    if (quota.error) throw new ApiError(503, "voice_quota_unavailable");
    if (!quota.data) throw new ApiError(429, "voice_quota_reached");
    const response = await fetch("https://api.openai.com/v1/audio/speech", {
      method: "POST", headers: { Authorization: `Bearer ${key}`, "Content-Type": "application/json" },
      body: JSON.stringify({ model: "tts-1", voice: "alloy", input: body.text.trim(), response_format: "pcm" }),
      signal: AbortSignal.timeout(35000),
    });
    if (!response.ok) throw new ApiError(response.status === 429 ? 429 : 502, "speech_provider_failed");
    const reader = response.body?.getReader();
    if (!reader) throw new ApiError(502, "speech_empty");
    const chunks: Uint8Array[] = []; let size = 0;
    while (true) {
      const { done, value } = await reader.read(); if (done) break;
      size += value.length;
      if (size > 24000 * 2 * 40) { await reader.cancel(); throw new ApiError(413, "speech_too_long"); }
      chunks.push(value);
    }
    if (!size || size % 2) throw new ApiError(502, "invalid_speech_pcm");
    const input = new Uint8Array(size); let offset = 0;
    for (const chunk of chunks) { input.set(chunk, offset); offset += chunk.length; }
    // Provider PCM is 24 kHz; the shared microphone/DAC clock remains 16 kHz.
    const source = new DataView(input.buffer);
    const output = new Uint8Array(Math.floor(size / 6) * 4);
    const target = new DataView(output.buffer);
    for (let i = 0, j = 0; i + 5 < size; i += 6, j += 4) {
      target.setInt16(j, source.getInt16(i, true), true);
      target.setInt16(j + 2, Math.round((source.getInt16(i + 2, true) + source.getInt16(i + 4, true)) / 2), true);
    }
    return new Response(output, { headers: { ...corsHeaders, "Content-Type": "audio/pcm", "Cache-Control": "no-store" } });
  }

  if (path === "voice-transcribe" && req.method === "POST") {
    const device = await authenticatedDevice(req);
    const key = Deno.env.get("OPENAI_API_KEY");
    if (!key) throw new ApiError(503, "openai_key_missing");
    if (req.headers.get("content-type")?.split(";")[0] !== "audio/wav") throw new ApiError(415, "wav_required");
    const maxBytes = 44 + 16000 * 2 * 12;
    const reader = req.body?.getReader();
    if (!reader) throw new ApiError(400, "audio_required");
    const chunks: Uint8Array[] = []; let size = 0;
    while (true) {
      const { done, value } = await reader.read(); if (done) break;
      size += value.length;
      if (size > maxBytes) { await reader.cancel(); throw new ApiError(413, "audio_too_long"); }
      chunks.push(value);
    }
    if (size < 44 + 1600) throw new ApiError(400, "audio_too_short");
    const audio = new Uint8Array(size); let offset = 0;
    for (const chunk of chunks) { audio.set(chunk, offset); offset += chunk.length; }
    const view = new DataView(audio.buffer);
    const ascii = (at: number, length: number) => new TextDecoder().decode(audio.subarray(at, at + length));
    if (ascii(0,4)!=="RIFF" || ascii(8,8)!=="WAVEfmt " || ascii(36,4)!=="data" ||
      view.getUint32(4,true)!==size-8 || view.getUint32(16,true)!==16 ||
      view.getUint16(20,true)!==1 || view.getUint16(22,true)!==1 ||
      view.getUint32(24,true)!==16000 || view.getUint32(28,true)!==32000 ||
      view.getUint16(32,true)!==2 || view.getUint16(34,true)!==16 ||
      view.getUint32(40,true)!==size-44 || (size-44)%2!==0) throw new ApiError(400,"invalid_pcm_wav");
    const quota = await db.rpc("esp32_reserve_voice", { p_owner: device.owner_id });
    if (quota.error) throw new ApiError(503,"voice_quota_unavailable");
    if (!quota.data) throw new ApiError(429,"voice_quota_reached");
    const form = new FormData();
    form.set("model", "whisper-1"); form.set("language", "es"); form.set("response_format", "json");
    form.set("file", new Blob([audio], { type: "audio/wav" }), "voice.wav");
    let response: Response;
    try {
      response = await fetch("https://api.openai.com/v1/audio/transcriptions", {
        method: "POST", headers: { Authorization: `Bearer ${key}` }, body: form,
        signal: AbortSignal.timeout(35000),
      });
    } catch { throw new ApiError(504,"transcription_timeout"); }
    if (!response.ok) {
      // Only status is logged; provider bodies may contain account details.
      console.error("Whisper provider status", response.status);
      if(response.status===401 || response.status===403) throw new ApiError(502,"openai_auth_failed");
      if(response.status===429) throw new ApiError(429,"openai_quota_or_rate_limit");
      if(response.status===404) throw new ApiError(502,"whisper_model_unavailable");
      if(response.status===400) throw new ApiError(502,"openai_audio_rejected");
      throw new ApiError(502,"transcription_provider_failed");
    }
    const transcript = await response.json();
    if (typeof transcript.text!=="string") throw new ApiError(502,"invalid_transcription_response");
    return json({ text: transcript.text.slice(0,3000), model: "whisper-1" });
  }

  if (path === "calendar-callback" && req.method === "GET") {
    const code = url.searchParams.get("code");
    const state = url.searchParams.get("state");
    if (!code || !state) throw new ApiError(400, "oauth_code_and_state_required");
    const stateHash = await sha256(state);
    const { data: oauthState, error: stateError } = await db.from("esp32_google_oauth_states")
      .update({ consumed_at: new Date().toISOString() })
      .eq("state_hash", stateHash).is("consumed_at", null).gt("expires_at", new Date().toISOString())
      .select("owner_id, calendar_id").maybeSingle();
    if (stateError || !oauthState) throw new ApiError(400, "oauth_state_invalid_or_expired");
    const { clientId, clientSecret, redirectUri } = googleConfig();
    const response = await fetch("https://oauth2.googleapis.com/token", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: new URLSearchParams({
        code,
        client_id: clientId,
        client_secret: clientSecret,
        redirect_uri: redirectUri,
        grant_type: "authorization_code",
      }),
    });
    const tokens = await response.json();
    if (!response.ok) {
      const reason = ["invalid_client", "invalid_grant", "redirect_uri_mismatch", "unauthorized_client", "invalid_request"].includes(tokens.error)
        ? tokens.error : "provider_failed";
      // Expose only a fixed error code; never log token bodies or authorization codes.
      console.error("Google OAuth exchange failed", response.status, reason);
      throw new ApiError(502, "google_oauth_exchange_" + reason);
    }
    if (typeof tokens.refresh_token !== "string" || !tokens.refresh_token) {
      throw new ApiError(502, "google_refresh_token_missing_reauthorize");
    }
    if (typeof tokens.access_token !== "string" || !tokens.access_token) {
      throw new ApiError(502, "google_access_token_missing");
    }
    const { error: saveError } = await db.from("esp32_google_accounts").upsert({
      owner_id: oauthState.owner_id,
      calendar_id: oauthState.calendar_id,
      refresh_token: tokens.refresh_token,
      access_token: tokens.access_token ?? null,
      access_token_expires_at: tokens.expires_in
        ? new Date(Date.now() + Number(tokens.expires_in) * 1000).toISOString()
        : null,
      granted_scope: tokens.scope ?? "https://www.googleapis.com/auth/calendar.events",
    });
    if (saveError) throw new ApiError(500, "google_account_save_failed");
    return new Response("<!doctype html><meta charset=utf-8><title>Calendar conectado</title><p>Google Calendar quedó conectado al asistente. Ya podés cerrar esta ventana.</p>", {
      headers: { "Content-Type": "text/html; charset=utf-8", "Cache-Control": "no-store" },
    });
  }

  if (path === "pair-start" && req.method === "POST") {
    const user = await authenticatedUser(req);
    const body = await jsonBody(req);
    const code = randomToken();
    const name = text(body.device_name, 120) ? body.device_name.trim() : "ESP32-S3-RLCD-4.2";
    const { data, error } = await db.from("esp32_device_pairings").insert({
      owner_id: user.id,
      code_hash: await sha256(code),
      device_name: name,
      expires_at: new Date(Date.now() + 10 * 60_000).toISOString(),
    }).select("id, expires_at").single();
    if (error) throw new ApiError(500, "pairing_code_create_failed");
    return json({ pairing_code: code, expires_at: data.expires_at, instructions: "Ingresá este código una sola vez en el dispositivo." });
  }

  if (path === "pair-claim" && req.method === "POST") {
    const body = await jsonBody(req);
    if (!text(body.pairing_code, 100)) throw new ApiError(400, "pairing_code_required");
    const deviceToken = randomToken();
    const { data, error } = await db.rpc("esp32_claim_pairing", {
      p_pair_code_hash: await sha256(body.pairing_code),
      p_device_token_hash: await sha256(deviceToken),
    });
    if (error || !Array.isArray(data) || !data[0]) throw new ApiError(401, "pairing_code_invalid_or_expired");
    return json({ device_id: data[0].device_id, device_name: data[0].device_name, device_token: deviceToken });
  }

  if (path === "snapshot" && req.method === "GET") {
    const device = await authenticatedDevice(req);
    const [projects, tasks, settings, alarms] = await Promise.all([
      db.from("esp32_projects").select("id,name,status,revision,updated_at")
        .eq("owner_id", device.owner_id).eq("status", "active").order("name"),
      db.from("esp32_tasks").select("id,project_id,title,notes,status,priority,due_date,due_at,remind_at,timezone,completed_at,revision,updated_at")
        .eq("owner_id", device.owner_id).order("updated_at", { ascending: false }).limit(50),
      db.from("esp32_device_settings").select("language,timezone,conversation_mode,speak_responses,display_name,voice_threshold,auto_voice,idle_seconds,calendar_rest,revision")
        .eq("device_id", device.id).maybeSingle(),
      db.from("esp32_device_alarms").select("id,label,time,days,due_date,enabled,revision")
        .eq("device_id", device.id).eq("owner_id", device.owner_id).order("time").limit(50),
    ]);
    if (projects.error || tasks.error || settings.error || alarms.error) throw new ApiError(500, "snapshot_query_failed");
    const revision = Math.max(1, ...[...(projects.data ?? []), ...(tasks.data ?? []), ...(alarms.data ?? []), ...(settings.data ? [settings.data] : [])].map((row) => Number(row.revision ?? 1)));
    const preferences = settings.data ?? {};
    return json({
      schema_version: 1,
      revision,
      server_time: new Date().toISOString(),
      clock_valid: true,
      projects: projects.data ?? [],
      tasks: (tasks.data ?? []).map((task) => ({ ...task, date: task.due_date ?? "", done: task.status === "completed" })),
      alarms: (alarms.data ?? []).map((alarm) => ({ ...alarm, time: String(alarm.time).slice(0, 5) })),
      settings: {
        ...preferences,
        name: preferences.display_name ?? "Compañero",
        threshold: preferences.voice_threshold ?? 500,
        autoVoice: preferences.auto_voice ?? true,
        idle: preferences.idle_seconds ?? 30,
        calendarRest: preferences.calendar_rest ?? true,
      },
      calendar_connected: Boolean((await db.from("esp32_google_accounts").select("owner_id").eq("owner_id", device.owner_id).maybeSingle()).data),
    });
  }

  if (path === "task-create" && req.method === "POST") {
    const device = await authenticatedDevice(req);
    const body = await jsonBody(req);
    if (!uuid(body.request_id) || !text(body.title, 300)) throw new ApiError(400, "request_id_and_title_required");
    if (body.project_id != null && !uuid(body.project_id)) throw new ApiError(400, "invalid_project_id");
    const result = await db.rpc("esp32_create_task", {
      p_device_id: device.id,
      p_request_id: body.request_id,
      p_title: body.title.trim(),
      p_project_id: body.project_id ?? null,
      p_notes: typeof body.notes === "string" ? body.notes.slice(0, 8000) : "",
      p_priority: Number.isInteger(body.priority) ? Math.max(0, Math.min(3, Number(body.priority))) : 0,
      p_due_date: typeof body.due_date === "string" ? body.due_date : null,
      p_due_at: typeof body.due_at === "string" ? body.due_at : null,
      p_remind_at: typeof body.remind_at === "string" ? body.remind_at : null,
      p_timezone: typeof body.timezone === "string" ? body.timezone.slice(0, 100) : "America/Argentina/Buenos_Aires",
    });
    if (result.error) throw new ApiError(400, "task_create_failed");
    return json(result.data);
  }

  if (path === "task-complete" && req.method === "POST") {
    const device = await authenticatedDevice(req);
    const body = await jsonBody(req);
    if (!uuid(body.request_id) || !uuid(body.task_id)) throw new ApiError(400, "request_id_and_task_id_required");
    const result = await db.rpc("esp32_complete_task", {
      p_device_id: device.id,
      p_request_id: body.request_id,
      p_task_id: body.task_id,
    });
    if (result.error) throw new ApiError(400, "task_complete_failed");
    return json(result.data);
  }

  if ((path === "calendar-connect" || path === "calendar-link") && req.method === "POST") {
    const user = path === "calendar-link" ? { id: (await authenticatedDevice(req)).owner_id } : await authenticatedUser(req);
    const { clientId, redirectUri } = googleConfig();
    const calendarId = Deno.env.get("GOOGLE_CALENDAR_ID") ?? "franrojas331@gmail.com";
    const state = randomToken();
    const { error } = await db.from("esp32_google_oauth_states").insert({
      state_hash: await sha256(state),
      owner_id: user.id,
      calendar_id: calendarId,
      expires_at: new Date(Date.now() + 10 * 60_000).toISOString(),
    });
    if (error) throw new ApiError(500, "oauth_state_create_failed");
    const authorizeUrl = new URL("https://accounts.google.com/o/oauth2/v2/auth");
    authorizeUrl.search = new URLSearchParams({
      client_id: clientId,
      redirect_uri: redirectUri,
      response_type: "code",
      scope: "https://www.googleapis.com/auth/calendar.events https://www.googleapis.com/auth/calendar.calendarlist.readonly",
      access_type: "offline",
      prompt: "consent",
      state,
    }).toString();
    return json({ authorize_url: authorizeUrl.toString(), calendar_id: calendarId });
  }

  if (path === "web-calendar-status" && req.method === "GET") {
    const user = await authenticatedUser(req);
    const { data, error } = await db.from("esp32_google_accounts").select("calendar_id,connected_at").eq("owner_id", user.id).maybeSingle();
    if (error) throw new ApiError(500, "google_calendar_status_failed");
    return json({ connected: Boolean(data), calendar_id: data?.calendar_id ?? null, connected_at: data?.connected_at ?? null });
  }

  // Browser dashboard routes use the signed-in user's JWT. Google tokens remain server-side.
  if (path === "web-calendar-list" && req.method === "POST") {
    const user = await authenticatedUser(req);
    const { token } = await googleAccessToken(user.id);
    const response = await fetch("https://www.googleapis.com/calendar/v3/users/me/calendarList?maxResults=50", {
      headers: { Authorization: `Bearer ${token}` },
    });
    const result = await response.json();
    if (!response.ok) throw new ApiError(502, "google_calendar_list_failed");
    return json({ calendars: (result.items ?? []).map((item: Record<string, any>) => ({
      id: item.id, name: item.summary ?? item.id, primary: Boolean(item.primary), access_role: item.accessRole ?? "reader",
    })) });
  }

  if (path === "web-calendar-select" && req.method === "POST") {
    const user = await authenticatedUser(req);
    const body = await jsonBody(req);
    if (!text(body.calendar_id, 1024)) throw new ApiError(400, "calendar_id_required");
    const { error } = await db.from("esp32_google_accounts").update({ calendar_id: body.calendar_id.trim() }).eq("owner_id", user.id);
    if (error) throw new ApiError(500, "google_calendar_selection_failed");
    return json({ calendar_id: body.calendar_id.trim() });
  }

  if (path === "web-calendar-events" && req.method === "POST") {
    const user = await authenticatedUser(req);
    const body = await jsonBody(req);
    const { token, calendarId } = await googleAccessToken(user.id);
    const start = typeof body.time_min === "string" ? body.time_min : new Date().toISOString();
    const end = typeof body.time_max === "string" ? body.time_max : new Date(Date.now() + 14 * 86400_000).toISOString();
    if (!Number.isFinite(Date.parse(start)) || !Number.isFinite(Date.parse(end)) || Date.parse(end) <= Date.parse(start)) {
      throw new ApiError(400, "calendar_range_invalid");
    }
    const query = new URLSearchParams({ timeMin: start, timeMax: end, singleEvents: "true", orderBy: "startTime", maxResults: "50" });
    const response = await fetch(`https://www.googleapis.com/calendar/v3/calendars/${encodeURIComponent(calendarId)}/events?${query}`, {
      headers: { Authorization: `Bearer ${token}` },
    });
    const result = await response.json();
    if (!response.ok) throw new ApiError(502, "google_calendar_read_failed");
    return json({ calendar_id: calendarId, events: (result.items ?? []).map((event: Record<string, any>) => ({
      id: event.id, title: event.summary ?? "(Sin título)", start: event.start?.dateTime ?? event.start?.date,
      end: event.end?.dateTime ?? event.end?.date, all_day: Boolean(event.start?.date),
      timezone: event.start?.timeZone ?? "America/Argentina/Buenos_Aires", location: event.location ?? null,
      etag: event.etag, status: event.status,
    })) });
  }

  if (path === "web-calendar-event-create" && req.method === "POST") {
    const user = await authenticatedUser(req);
    const body = await jsonBody(req);
    if (!uuid(body.request_id) || !text(body.title, 300) || !text(body.start_at, 40) || !text(body.end_at, 40) ||
      !Number.isFinite(Date.parse(String(body.start_at))) || !Number.isFinite(Date.parse(String(body.end_at))) ||
      Date.parse(String(body.end_at)) <= Date.parse(String(body.start_at))) throw new ApiError(400, "event_fields_invalid");
    const { token, calendarId } = await googleAccessToken(user.id);
    const eventId = String(body.request_id).replaceAll("-", "").toLowerCase();
    const response = await fetch(`https://www.googleapis.com/calendar/v3/calendars/${encodeURIComponent(calendarId)}/events`, {
      method: "POST", headers: { Authorization: `Bearer ${token}`, "Content-Type": "application/json" },
      body: JSON.stringify({ id: eventId, summary: body.title.trim(), start: { dateTime: body.start_at, timeZone: "America/Argentina/Buenos_Aires" }, end: { dateTime: body.end_at, timeZone: "America/Argentina/Buenos_Aires" } }),
    });
    const result = await response.json();
    if (!response.ok) throw new ApiError(502, "google_calendar_create_failed");
    return json(result, 201);
  }

  if (path === "web-calendar-event-update" && req.method === "POST") {
    const user = await authenticatedUser(req);
    const body = await jsonBody(req);
    if (!text(body.event_id, 1024) || !text(body.title, 300)) throw new ApiError(400, "event_id_and_title_required");
    const { token, calendarId } = await googleAccessToken(user.id);
    const patch: Record<string, unknown> = { summary: body.title.trim() };
    if (text(body.start_at, 40) && Number.isFinite(Date.parse(body.start_at))) patch.start = { dateTime: body.start_at, timeZone: "America/Argentina/Buenos_Aires" };
    if (text(body.end_at, 40) && Number.isFinite(Date.parse(body.end_at))) patch.end = { dateTime: body.end_at, timeZone: "America/Argentina/Buenos_Aires" };
    const response = await fetch(`https://www.googleapis.com/calendar/v3/calendars/${encodeURIComponent(calendarId)}/events/${encodeURIComponent(body.event_id)}`, {
      method: "PATCH", headers: { Authorization: `Bearer ${token}`, "Content-Type": "application/json" }, body: JSON.stringify(patch),
    });
    const result = await response.json();
    if (!response.ok) throw new ApiError(502, "google_calendar_update_failed");
    return json(result);
  }

  if (path === "web-calendar-event-delete" && req.method === "POST") {
    const user = await authenticatedUser(req);
    const body = await jsonBody(req);
    if (!text(body.event_id, 1024)) throw new ApiError(400, "event_id_required");
    const { token, calendarId } = await googleAccessToken(user.id);
    const response = await fetch(`https://www.googleapis.com/calendar/v3/calendars/${encodeURIComponent(calendarId)}/events/${encodeURIComponent(body.event_id)}`, {
      method: "DELETE", headers: { Authorization: `Bearer ${token}` },
    });
    if (!response.ok && response.status !== 404) throw new ApiError(502, "google_calendar_delete_failed");
    return json({ deleted: true });
  }

  if ((path === "calendar-events" || path === "calendar-event-create" || path === "calendar-event-update") && req.method === "POST") {
    const device = await authenticatedDevice(req);
    const body = await jsonBody(req);
    const { token, calendarId } = await googleAccessToken(device.owner_id);
    const base = `https://www.googleapis.com/calendar/v3/calendars/${encodeURIComponent(calendarId)}/events`;
    if (path === "calendar-events") {
      const start = typeof body.time_min === "string" ? body.time_min : new Date().toISOString();
      const end = typeof body.time_max === "string" ? body.time_max : new Date(Date.now() + 14 * 86400_000).toISOString();
      const query = new URLSearchParams({ timeMin: start, timeMax: end, singleEvents: "true", orderBy: "startTime", maxResults: "50" });
      const response = await fetch(`${base}?${query}`, { headers: { Authorization: `Bearer ${token}` } });
      const result = await response.json();
      if (!response.ok) throw new ApiError(502, "google_calendar_read_failed");
      return json({ calendar_id: calendarId, events: (result.items ?? []).map((event: Record<string, any>) => ({
        id: event.id,
        title: event.summary ?? "(Sin título)",
        start: event.start?.dateTime ?? event.start?.date,
        end: event.end?.dateTime ?? event.end?.date,
        all_day: Boolean(event.start?.date),
        timezone: event.start?.timeZone ?? "America/Argentina/Buenos_Aires",
        location: event.location ?? null,
        etag: event.etag,
        status: event.status,
      })) });
    }
    if (path === "calendar-event-create") {
      if (!uuid(body.request_id) || !text(body.title, 300) || !text(body.start_at, 40) || !text(body.end_at, 40)) {
        throw new ApiError(400, "request_id_title_start_and_end_required");
      }
      const eventId = body.request_id.replaceAll("-", "").toLowerCase();
      const response = await fetch(base, {
        method: "POST",
        headers: { Authorization: `Bearer ${token}`, "Content-Type": "application/json" },
        body: JSON.stringify({
          id: eventId,
          summary: body.title.trim(),
          start: { dateTime: body.start_at, timeZone: "America/Argentina/Buenos_Aires" },
          end: { dateTime: body.end_at, timeZone: "America/Argentina/Buenos_Aires" },
          ...(text(body.location, 500) ? { location: body.location } : {}),
        }),
      });
      const result = await response.json();
      if (response.status === 409) {
        const existing = await fetch(`${base}/${eventId}`, { headers: { Authorization: `Bearer ${token}` } });
        if (existing.ok) return json(await existing.json());
      }
      if (!response.ok) throw new ApiError(502, "google_calendar_create_failed");
      return json(result, 201);
    }
    if (!text(body.event_id, 1024) || !text(body.title, 300)) throw new ApiError(400, "event_id_and_title_required");
    const patch: Record<string, unknown> = { summary: body.title.trim() };
    if (text(body.start_at, 40)) patch.start = { dateTime: body.start_at, timeZone: "America/Argentina/Buenos_Aires" };
    if (text(body.end_at, 40)) patch.end = { dateTime: body.end_at, timeZone: "America/Argentina/Buenos_Aires" };
    const response = await fetch(`${base}/${encodeURIComponent(body.event_id)}`, {
      method: "PATCH",
      headers: { Authorization: `Bearer ${token}`, "Content-Type": "application/json" },
      body: JSON.stringify(patch),
    });
    const result = await response.json();
    if (!response.ok) throw new ApiError(502, "google_calendar_update_failed");
    return json(result);
  }

  throw new ApiError(404, "route_not_found");
}

Deno.serve(async (req: Request) => {
  try {
    return await handle(req);
  } catch (error) {
    if (error instanceof ApiError) return json({ error: error.message }, error.status);
    console.error("esp32-api unexpected error", error);
    return json({ error: "internal_error" }, 500);
  }
});
