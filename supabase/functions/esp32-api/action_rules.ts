export const actions = ["task.create", "task.list", "task.complete", "calendar.list", "calendar.create", "calendar.update", "calendar.delete", "clock.read", "environment.read", "view.change", "settings.update", "alarm.create", "alarm.list", "music.control", "answer", "clarify", "unsupported"] as const;
export type Intent = {
  action: typeof actions[number]; title: string | null; target: string | null;
  date: string | null; time: string | null; duration: number | null;
  view: string | null; setting: string | null; value: string | null;
  days: string | null; question: string | null; answer: string | null;
};
export const intentSchema = {
  type: "object", additionalProperties: false,
  properties: {
    action: { type: "string", enum: actions },
    ...Object.fromEntries(["title", "target", "date", "time", "view", "setting", "value", "days", "question", "answer"].map(key => [key, { type: ["string", "null"] }])),
    duration: { type: ["integer", "null"] },
  },
  required: ["action", "title", "target", "date", "time", "duration", "view", "setting", "value", "days", "question", "answer"],
};
export function validIntent(value: unknown): value is Intent {
  if (!value || typeof value !== "object" || Array.isArray(value)) return false;
  const v = value as Record<string, unknown>;
  if (!actions.includes(v.action as Intent["action"]) || Object.keys(v).length !== intentSchema.required.length) return false;
  for (const key of intentSchema.required) {
    if (key === "action") continue;
    if (key === "duration") { if (v[key] !== null && (!Number.isInteger(v[key]) || Number(v[key]) < 1 || Number(v[key]) > 1440)) return false; }
    else if (v[key] !== null && (typeof v[key] !== "string" || String(v[key]).length > 1200)) return false;
  }
  return true;
}
export function normalized(s: string): string { return s.normalize("NFD").replace(/[\u0300-\u036f]/g, "").toLowerCase().replace(/[^a-z0-9 ]/g, " ").replace(/\s+/g, " ").trim(); }
export function candidates<T extends { title: string }>(rows: T[], query: string | null): T[] {
  const q = normalized(query ?? "");
  if (!q) return [];
  const exact = rows.filter(r => normalized(r.title) === q);
  return exact.length ? exact : rows.filter(r => normalized(r.title).includes(q));
}
export function validDate(s: string | null): s is string {
  return !!s && /^\d{4}-\d{2}-\d{2}$/.test(s) && Number.isFinite(Date.parse(s + "T12:00:00Z")) && new Date(s + "T12:00:00Z").toISOString().slice(0, 10) === s;
}
export function validTime(s: string | null): s is string { return !!s && /^([01]\d|2[0-3]):[0-5]\d$/.test(s); }
export function localDay(now = new Date()): string { return new Intl.DateTimeFormat("sv-SE", { timeZone: "America/Argentina/Buenos_Aires" }).format(now); }
export function nextDay(date: string): string { return new Date(Date.parse(date + "T12:00:00Z") + 86400000).toISOString().slice(0, 10); }
export function noiseTranscript(text: string): boolean { return /subtitulos.*comunidad|amara org|gracias por ver|suscribete/.test(normalized(text)) || /^\s*\[[^\]]*\]\s*$/.test(text); }
export function confirmation(text: string): boolean { return /^(si|si confirmo|confirmo|si eliminar|si borralo|confirmar)$/.test(normalized(text)); }
export function cancellation(text: string): boolean { return /^(no|cancelar|cancela|cancelalo|no cancelar)$/.test(normalized(text)); }
export function preference(setting: string | null, value: string | null): Record<string, unknown> | null {
  if (["auto_voice", "calendar_rest", "speak_responses"].includes(setting ?? "") && ["true", "false"].includes(value ?? "")) return { [setting!]: value === "true" };
  if (setting === "voice_threshold" && ["500", "800", "1200"].includes(value ?? "")) return { voice_threshold: Number(value) };
  if (setting === "idle_seconds" && ["15", "30", "60", "120"].includes(value ?? "")) return { idle_seconds: Number(value) };
  if (setting === "display_name" && value && value.trim().length <= 32) return { display_name: value.trim() };
  return null;
}
