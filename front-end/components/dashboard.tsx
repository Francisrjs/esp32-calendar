"use client";

import { useCallback, useEffect, useMemo, useState } from "react";
import { Activity, AlarmClock, ArrowDownToLine, ArrowUpRight, BatteryCharging, CalendarDays, Check, CheckCircle2, ChevronDown, ChevronLeft, ChevronRight, CircleHelp, Clock3, Command, Cpu, Disc3, LayoutDashboard, LoaderCircle, LogOut, Menu, Mic2, Music2, Pencil, Plus, Radio, RefreshCw, Settings2, ShieldCheck, Sparkles, Trash2, X } from "lucide-react";
import { backendUrl, isSupabaseConfigured, supabase } from "@/lib/supabase";
import { EnergyPanel } from "@/components/energy-panel";

type Device = { id: string; name: string; status: "pending" | "active" | "revoked"; last_seen_at: string | null };
type Task = { id: string; title: string; due_date: string | null; status: "pending" | "completed"; revision: number; updated_at: string };
type Alarm = { id: string; device_id: string; label: string; time: string; days: string; enabled: boolean; revision: number };
type Prefs = { device_id: string; owner_id: string; display_name: string; voice_threshold: number; auto_voice: boolean; idle_seconds: number; calendar_rest: boolean; revision: number };
type CalendarInfo = { id: string; name: string; primary: boolean; access_role: string };
type Tab = "overview" | "tasks" | "alarms" | "calendar" | "music" | "device" | "energy";
type Notice = { text: string; kind: "success" | "error" | "info" };
const sections: { id: Tab; label: string; icon: typeof LayoutDashboard }[] = [
  { id: "overview", label: "Resumen", icon: LayoutDashboard }, { id: "tasks", label: "Tareas", icon: CheckCircle2 },
  { id: "alarms", label: "Alarmas", icon: AlarmClock }, { id: "calendar", label: "Agenda", icon: CalendarDays },
  { id: "music", label: "Música", icon: Music2 }, { id: "device", label: "Dispositivo", icon: Cpu },
  { id: "energy", label: "Energía", icon: BatteryCharging },
];
const today = () => new Intl.DateTimeFormat("sv-SE", { timeZone: "America/Argentina/Buenos_Aires" }).format(new Date());
const localTime = (value: string) => new Intl.DateTimeFormat("es-AR", { dateStyle: "medium", timeStyle: "short", timeZone: "America/Argentina/Buenos_Aires" }).format(new Date(value));
const errorText = (error: unknown) => error instanceof Error ? error.message : "No se pudo completar la operación.";

export default function Dashboard() {
  const [session, setSession] = useState<any>(null);
  const [tab, setTab] = useState<Tab>("overview");
  const [devices, setDevices] = useState<Device[]>([]);
  const [tasks, setTasks] = useState<Task[]>([]);
  const [alarms, setAlarms] = useState<Alarm[]>([]);
  const [prefs, setPrefs] = useState<Prefs | null>(null);
  const [loading, setLoading] = useState(true);
  const [working, setWorking] = useState(false);
  const [notice, setNotice] = useState<Notice | null>(null);
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [authMode, setAuthMode] = useState<"in" | "up">("in");
  const [taskText, setTaskText] = useState("");
  const [taskDate, setTaskDate] = useState("");
  const [taskEditing, setTaskEditing] = useState<Task | null>(null);
  const [alarmLabel, setAlarmLabel] = useState("");
  const [alarmTime, setAlarmTime] = useState("07:30");
  const [alarmDays, setAlarmDays] = useState("Lun a vie");
  const [alarmEditing, setAlarmEditing] = useState<Alarm | null>(null);
  const [calendar, setCalendar] = useState<any[]>([]);
  const [calendars, setCalendars] = useState<CalendarInfo[]>([]);
  const [selectedCalendar, setSelectedCalendar] = useState("");
  const [calendarConnected, setCalendarConnected] = useState(false);
  const [calendarBusy, setCalendarBusy] = useState(false);
  const [eventTitle, setEventTitle] = useState("");
  const [eventStart, setEventStart] = useState("");
  const [eventEnd, setEventEnd] = useState("");
  const [month, setMonth] = useState(new Date(`${today()}T12:00:00-03:00`));
  const [todayDate, setTodayDate] = useState("");

  const notify = (text: string, kind: Notice["kind"] = "success") => { setNotice({ text, kind }); window.setTimeout(() => setNotice(null), 5000); };
  useEffect(() => {
    const date = today();
    setTodayDate(date); setMonth(new Date());
    setEventStart((value) => value || `${date}T16:00`); setEventEnd((value) => value || `${date}T17:00`);
  }, []);
  const reload = useCallback(async (userId: string) => {
    const [deviceResult, taskResult, alarmResult] = await Promise.all([
      supabase.from("esp32_devices").select("id,name,status,last_seen_at").eq("owner_id", userId).order("created_at", { ascending: true }),
      supabase.from("esp32_tasks").select("id,title,due_date,status,revision,updated_at").eq("owner_id", userId).order("updated_at", { ascending: false }).limit(100),
      supabase.from("esp32_device_alarms").select("id,device_id,label,time,days,enabled,revision").eq("owner_id", userId).order("time"),
    ]);
    if (deviceResult.error) throw deviceResult.error;
    if (taskResult.error) throw taskResult.error;
    if (alarmResult.error) throw alarmResult.error;
    const nextDevices = (deviceResult.data ?? []) as Device[];
    const active = nextDevices.find((d) => d.status === "active");
    setDevices(nextDevices); setTasks((taskResult.data ?? []) as Task[]); setAlarms(((alarmResult.data ?? []) as Alarm[]).map((alarm) => ({ ...alarm, time: alarm.time.slice(0, 5) })));
    if (active) {
      const result = await supabase.from("esp32_device_settings").select("device_id,owner_id,display_name,voice_threshold,auto_voice,idle_seconds,calendar_rest,revision").eq("device_id", active.id).maybeSingle();
      if (result.error) throw result.error;
      setPrefs(result.data as Prefs | null);
    } else setPrefs(null);
  }, []);

  useEffect(() => {
    if (!isSupabaseConfigured) { setLoading(false); return; }
    supabase.auth.getSession().then(({ data }) => { setSession(data.session); if (data.session) reload(data.session.user.id).catch((e) => notify(errorText(e), "error")).finally(() => setLoading(false)); else setLoading(false); });
    const { data: listener } = supabase.auth.onAuthStateChange((_event, next) => {
      setSession(next);
      if (next) { setLoading(true); window.setTimeout(() => reload(next.user.id).catch((e) => notify(errorText(e), "error")).finally(() => setLoading(false)), 0); }
      else { setDevices([]); setTasks([]); setAlarms([]); setPrefs(null); setLoading(false); }
    });
    return () => listener.subscription.unsubscribe();
  }, [reload]);

  const activeDevice = devices.find((device) => device.status === "active");
  const pendingCount = tasks.filter((task) => task.status === "pending").length;
  const activeAlarms = alarms.filter((alarm) => alarm.enabled).length;
  const completedCount = tasks.length - pendingCount;
  const weekDays = useMemo(() => Array.from({ length: 7 }, (_, i) => { const d = new Date(month); d.setDate(1); const offset = (d.getDay() + 6) % 7; d.setDate(1 - offset + i); return d; }), [month]);
  const run = async (action: () => Promise<void>, success?: string) => { setWorking(true); try { await action(); if (success) notify(success); } catch (e) { notify(errorText(e), "error"); } finally { setWorking(false); } };

  const submitAuth = async (event: React.FormEvent) => {
    event.preventDefault();
    await run(async () => {
      const result = authMode === "in" ? await supabase.auth.signInWithPassword({ email: email.trim(), password }) : await supabase.auth.signUp({ email: email.trim(), password, options: { emailRedirectTo: window.location.origin } });
      if (result.error) throw result.error;
      if (authMode === "up" && !result.data.session) notify("Cuenta creada. Revisá tu correo para confirmar el acceso.", "info");
    });
  };

  const createTask = async (event: React.FormEvent) => {
    event.preventDefault(); if (!session?.user) return;
    await run(async () => {
      if (taskEditing) {
        const result = await supabase.from("esp32_tasks").update({ title: taskText.trim(), due_date: taskDate || null }).eq("id", taskEditing.id).eq("owner_id", session.user.id).eq("revision", taskEditing.revision).select("id").maybeSingle();
        if (result.error) throw result.error; if (!result.data) throw new Error("La tarea cambió en otro lugar. Actualizá la lista antes de volver a guardar.");
      } else {
        const result = await supabase.from("esp32_tasks").insert({ owner_id: session.user.id, title: taskText.trim(), due_date: taskDate || null, status: "pending", timezone: "America/Argentina/Buenos_Aires" });
        if (result.error) throw result.error;
      }
      await reload(session.user.id); setTaskText(""); setTaskDate(""); setTaskEditing(null);
    }, taskEditing ? "Tarea actualizada." : "Tarea guardada en Supabase.");
  };
  const toggleTask = async (task: Task) => run(async () => {
    const status = task.status === "pending" ? "completed" : "pending";
    const result = await supabase.from("esp32_tasks").update({ status, completed_at: status === "completed" ? new Date().toISOString() : null }).eq("id", task.id).eq("owner_id", session.user.id).eq("revision", task.revision).select("id").maybeSingle();
    if (result.error) throw result.error; if (!result.data) throw new Error("La tarea cambió en otro lugar. Actualizá la lista antes de volver a guardar."); await reload(session.user.id);
  }, "Tarea actualizada.");
  const removeTask = async (task: Task) => run(async () => {
    if (!window.confirm(`¿Eliminar “${task.title}”?`)) return;
    const result = await supabase.from("esp32_tasks").delete().eq("id", task.id).eq("owner_id", session.user.id).eq("revision", task.revision).select("id").maybeSingle();
    if (result.error) throw result.error; if (!result.data) throw new Error("La tarea cambió en otro lugar. Actualizá la lista antes de volver a guardar."); await reload(session.user.id);
  }, "Tarea eliminada.");
  const startTaskEdit = (task: Task) => { setTaskEditing(task); setTaskText(task.title); setTaskDate(task.due_date ?? ""); setTab("tasks"); };

  const createAlarm = async (event: React.FormEvent) => {
    event.preventDefault(); if (!activeDevice) return;
    await run(async () => {
      if (alarmEditing) {
        const result = await supabase.from("esp32_device_alarms").update({ label: alarmLabel.trim(), time: alarmTime, days: alarmDays }).eq("id", alarmEditing.id).eq("owner_id", session.user.id).eq("revision", alarmEditing.revision).select("id").maybeSingle();
        if (result.error) throw result.error; if (!result.data) throw new Error("La alarma cambió en otro lugar. Actualizá antes de volver a guardar.");
      } else {
        const result = await supabase.from("esp32_device_alarms").insert({ device_id: activeDevice.id, owner_id: session.user.id, label: alarmLabel.trim(), time: alarmTime, days: alarmDays, enabled: true });
        if (result.error) throw result.error;
      }
      await reload(session.user.id); setAlarmLabel(""); setAlarmEditing(null);
    }, alarmEditing ? "Alarma actualizada." : "Alarma guardada.");
  };
  const changeAlarm = async (alarm: Alarm, patch: Partial<Alarm>) => run(async () => {
    const result = await supabase.from("esp32_device_alarms").update(patch).eq("id", alarm.id).eq("owner_id", session.user.id).eq("revision", alarm.revision).select("id").maybeSingle();
    if (result.error) throw result.error; if (!result.data) throw new Error("La alarma cambió en otro lugar. Actualizá antes de volver a guardar."); await reload(session.user.id);
  }, "Alarma actualizada.");
  const deleteAlarm = async (alarm: Alarm) => run(async () => {
    if (!window.confirm(`¿Eliminar la alarma “${alarm.label}”?`)) return;
    const result = await supabase.from("esp32_device_alarms").delete().eq("id", alarm.id).eq("owner_id", session.user.id).eq("revision", alarm.revision).select("id").maybeSingle();
    if (result.error) throw result.error; if (!result.data) throw new Error("La alarma cambió en otro lugar. Actualizá antes de volver a guardar."); await reload(session.user.id);
  }, "Alarma eliminada.");
  const startAlarmEdit = (alarm: Alarm) => { setAlarmEditing(alarm); setAlarmLabel(alarm.label); setAlarmTime(alarm.time); setAlarmDays(alarm.days); };

  const savePrefs = async (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault(); if (!prefs) return;
    const form = new FormData(event.currentTarget);
    await run(async () => {
      const result = await supabase.from("esp32_device_settings").update({ display_name: String(form.get("display_name")).trim(), voice_threshold: Number(form.get("voice_threshold")), auto_voice: form.has("auto_voice"), idle_seconds: Number(form.get("idle_seconds")), calendar_rest: form.has("calendar_rest") }).eq("device_id", prefs.device_id).eq("owner_id", session.user.id).eq("revision", prefs.revision).select("device_id").maybeSingle();
      if (result.error) throw result.error; if (!result.data) throw new Error("Las preferencias cambiaron en otro lugar. Actualizá antes de volver a guardar."); await reload(session.user.id);
    }, "Preferencias guardadas en Supabase. La recepción de la placa se confirma por separado.");
  };

  const edgePost = async (path: string, body: object = {}) => {
    const { data: { session: current } } = await supabase.auth.getSession();
    if (!current?.access_token) throw new Error("La sesión expiró. Volvé a iniciar sesión.");
    const response = await fetch(`${backendUrl()}/${path}`, { method: "POST", headers: { "Content-Type": "application/json", apikey: process.env.NEXT_PUBLIC_SUPABASE_PUBLISHABLE_KEY || "", Authorization: `Bearer ${current.access_token}` }, body: JSON.stringify(body) });
    const json = await response.json().catch(() => ({})); if (!response.ok) throw new Error(json.error || json.message || "El backend no pudo completar la solicitud."); return json;
  };
  const edgeGet = async (path: string) => {
    const { data: { session: current } } = await supabase.auth.getSession();
    if (!current?.access_token) throw new Error("La sesión expiró. Volvé a iniciar sesión.");
    const response = await fetch(`${backendUrl()}/${path}`, { headers: { apikey: process.env.NEXT_PUBLIC_SUPABASE_PUBLISHABLE_KEY || "", Authorization: `Bearer ${current.access_token}` } });
    const json = await response.json().catch(() => ({})); if (!response.ok) throw new Error(json.error || json.message || "El backend no pudo completar la solicitud."); return json;
  };
  useEffect(() => {
    if (!session) return;
    edgeGet("web-calendar-status").then(async (status) => {
      if (!status.connected) return;
      setCalendarConnected(true); setSelectedCalendar(status.calendar_id || "");
      const result = await edgePost("web-calendar-list"); setCalendars(result.calendars ?? []);
    }).catch(() => setCalendarConnected(false));
  }, [session?.user?.id]);
  const pairDevice = async () => run(async () => {
    const result = await edgePost("pair-start", { device_name: "ESP32-S3-RLCD-4.2" });
    window.prompt(`Código de vinculación (vence ${localTime(result.expires_at)}). Ingresalo en la consola serie del ESP32:`, result.pairing_code);
    await reload(session.user.id);
  });
  const connectCalendar = async () => run(async () => {
    const result = await edgePost("calendar-connect"); if (!result.authorize_url) throw new Error("El backend no devolvió el enlace de Google."); window.location.assign(result.authorize_url);
  });
  const loadCalendar = async () => run(async () => {
    setCalendarBusy(true);
    try { const start = new Date(month.getFullYear(), month.getMonth(), 1); const end = new Date(month.getFullYear(), month.getMonth() + 1, 1); const rangeStart = `${start.getFullYear()}-${String(start.getMonth() + 1).padStart(2, "0")}-01T00:00:00-03:00`; const rangeEnd = `${end.getFullYear()}-${String(end.getMonth() + 1).padStart(2, "0")}-01T00:00:00-03:00`; const result = await edgePost("web-calendar-events", { time_min: new Date(rangeStart).toISOString(), time_max: new Date(rangeEnd).toISOString() }); setCalendar(result.events ?? []); setCalendarConnected(true); }
    finally { setCalendarBusy(false); }
  }, "Agenda actualizada desde Google Calendar.");
  const createCalendarEvent = async (event: React.FormEvent) => {
    event.preventDefault();
    await run(async () => {
      await edgePost("web-calendar-event-create", { request_id: crypto.randomUUID(), title: eventTitle.trim(), start_at: new Date(`${eventStart}:00-03:00`).toISOString(), end_at: new Date(`${eventEnd}:00-03:00`).toISOString() });
      setEventTitle(""); await loadCalendar();
    }, "Evento creado en Google Calendar.");
  };
  const updateCalendarEvent = async (item: any) => {
    const title = window.prompt("Editar título del evento", item.title);
    if (!title?.trim() || title.trim() === item.title) return;
    await run(async () => { await edgePost("web-calendar-event-update", { event_id: item.id, title: title.trim() }); await loadCalendar(); }, "Evento actualizado.");
  };
  const deleteCalendarEvent = async (item: any) => {
    if (!window.confirm(`¿Eliminar “${item.title}” de Google Calendar?`)) return;
    await run(async () => { await edgePost("web-calendar-event-delete", { event_id: item.id }); await loadCalendar(); }, "Evento eliminado.");
  };
  const exportData = () => {
    const config = { version: 2, settings: { name: prefs?.display_name ?? "Compañero", threshold: prefs?.voice_threshold ?? 500, autoVoice: prefs?.auto_voice ?? true, idle: prefs?.idle_seconds ?? 30, calendarRest: prefs?.calendar_rest ?? true }, tasks: tasks.map((t) => ({ id: t.id, title: t.title, date: t.due_date ?? "", done: t.status === "completed" })), alarms: alarms.map((a) => ({ id: a.id, label: a.label, time: a.time, days: a.days, enabled: a.enabled })), events: calendar.map((event) => ({ id: event.id, title: event.title, date: event.start?.slice(0, 10) || "", time: event.start?.slice(11, 16) || "00:00" })), music: { title: "Sin pista", artist: "No conectado", duration: 1, position: 0, playing: false, volume: 0 }, integrations: { calendar: { connected: false, email: "", calendarId: "primary", showOnDevice: true, lastSync: null }, music: { provider: "demo", target: "Escritorio" } } };
    const blob = URL.createObjectURL(new Blob([JSON.stringify(config, null, 2)], { type: "application/json" })); const a = document.createElement("a"); a.href = blob; a.download = "companero-config-v2.json"; a.click(); URL.revokeObjectURL(blob);
  };

  if (loading) return <main className="loading-screen"><LoaderCircle className="spin" /><span>Conectando con tu espacio…</span></main>;
  if (!isSupabaseConfigured) return <main className="auth-screen"><section className="auth-card"><Brand /><h1>Conectá tu espacio</h1><p>Configurá la URL y la clave pública de Supabase en el entorno local o en Netlify para iniciar el panel.</p><code>NEXT_PUBLIC_SUPABASE_URL<br />NEXT_PUBLIC_SUPABASE_PUBLISHABLE_KEY</code><p className="hint">Copiá <b>.env.example</b> a <b>.env.local</b> y completá ambas variables.</p></section></main>;
  if (!session) return <main className="auth-screen"><section className="auth-card"><Brand /><span className="eyebrow">PANEL PERSONAL · ESP32</span><h1>{authMode === "in" ? "Qué bueno verte." : "Creá tu cuenta."}</h1><p>Tu configuración y tus dispositivos, en un mismo lugar.</p><form className="auth-form" onSubmit={submitAuth}><label>Correo electrónico<input type="email" autoComplete="email" required value={email} onChange={(e) => setEmail(e.target.value)} placeholder="vos@ejemplo.com" /></label><label>Contraseña<input type="password" autoComplete={authMode === "in" ? "current-password" : "new-password"} minLength={8} required value={password} onChange={(e) => setPassword(e.target.value)} placeholder="8 caracteres como mínimo" /></label><button className="button primary full" disabled={working}>{working ? <LoaderCircle className="spin" /> : null}{authMode === "in" ? "Iniciar sesión" : "Crear cuenta"}<ArrowUpRight size={17} /></button></form><button className="text-button" onClick={() => setAuthMode(authMode === "in" ? "up" : "in")}>{authMode === "in" ? "¿Primera vez? Crear una cuenta" : "Ya tengo cuenta · Iniciar sesión"}</button><p className="security-note"><ShieldCheck size={15} /> Sesión protegida por Supabase Auth y reglas por usuario.</p></section>{notice && <Toast notice={notice} onClose={() => setNotice(null)} />}</main>;

  return <div className="app-shell">
    <aside className="sidebar"><Brand /><div className="workspace-label">ESPACIO PERSONAL <ChevronDown size={13} /></div><nav>{sections.map(({ id, label, icon: Icon }) => <button key={id} aria-label={label} title={label} className={`nav-link ${tab === id ? "selected" : ""}`} onClick={() => setTab(id)}><Icon size={18} strokeWidth={1.8} /><span>{label}</span>{id === "tasks" && pendingCount > 0 && <small>{pendingCount}</small>}</button>)}</nav><div className="sidebar-bottom"><div className="device-mini"><div className={`device-led ${activeDevice ? "online" : ""}`} /><div><b>{prefs?.display_name || activeDevice?.name || "Sin dispositivo"}</b><span>{activeDevice ? (activeDevice.last_seen_at ? "Visto " + localTime(activeDevice.last_seen_at) : "Vinculado · esperando conexión") : "Vinculá tu ESP32"}</span></div></div><button className="nav-link" onClick={() => setTab("device")}><Settings2 size={18} /><span>Preferencias</span></button><button className="nav-link" onClick={() => supabase.auth.signOut()}><LogOut size={18} /><span>Cerrar sesión</span></button><div className="account"><div className="avatar">{session.user.email?.[0]?.toUpperCase()}</div><div><b>{session.user.email}</b><span>Cuenta personal</span></div><CircleHelp size={17} /></div></div></aside>
    <main className="main-content"><header className="topbar"><div className="breadcrumbs"><span>Compañero</span><ChevronRight size={14} /><b>{sections.find((s) => s.id === tab)?.label}</b></div><div className="top-actions"><span className="live-status"><i /> Backend conectado</span><button className="icon-button" title="Actualizar" onClick={() => reload(session.user.id)}><RefreshCw size={16} /></button><button className="user-chip" onClick={() => supabase.auth.signOut()}><div className="avatar small">{session.user.email?.[0]?.toUpperCase()}</div><span>{session.user.email}</span><LogOut size={15} /></button></div></header>
    <div className="page-wrap"><div className="page-heading"><div><span className="eyebrow">{todayDate ? new Intl.DateTimeFormat("es-AR", { weekday: "long", day: "numeric", month: "long", timeZone: "America/Argentina/Buenos_Aires" }).format(new Date(`${todayDate}T12:00:00-03:00`)).toLocaleUpperCase("es-AR") : ""}</span><h1>{tab === "overview" ? "Un día, más simple." : sections.find((s) => s.id === tab)?.label}</h1><p>{tab === "overview" ? "Todo lo que acompaña a tu ESP32, a tu manera." : pageDescription(tab)}</p></div><div className="heading-actions"><button className="button subtle" onClick={exportData}><ArrowDownToLine size={16} /> Exportar config</button><button className="button primary" onClick={pairDevice}><Plus size={17} /> Vincular ESP32</button></div></div>

    {!activeDevice && <div className="notice-banner"><div className="banner-icon"><Radio size={19} /></div><div><b>Tu ESP32 todavía no está vinculado</b><span>Iniciá sesión y generá un código de un solo uso para asociar la placa a esta cuenta.</span></div><button className="button dark" onClick={pairDevice}>Crear código <ArrowUpRight size={15} /></button></div>}
    {tab === "overview" && <><div className="welcome-grid"><section className="welcome-card"><div className="welcome-copy"><span className="eyebrow light">TU COMPAÑERO DE ESCRITORIO</span><h2>Un poco más de<br />orden, cada día.</h2><p>Tus tareas y ajustes están en un solo lugar, listos para acompañarte.</p><button className="button white" onClick={() => setTab("tasks")}>Ver mis tareas <ArrowUpRight size={15} /></button></div><div className="hero-art"><div className="orbit orbit-one" /><div className="orbit orbit-two" /><div className="hero-disc"><div className="disc-core"><Sparkles size={25} /></div></div><div className="hero-chip"><Activity size={14} /> conectado</div><span className="hero-star">✳</span></div></section><section className="date-card"><div className="date-title"><div><span className="eyebrow">TU SEMANA</span><h3>{new Intl.DateTimeFormat("es-AR", { month: "long", year: "numeric", timeZone: "America/Argentina/Buenos_Aires" }).format(month)}</h3></div><div className="month-buttons"><button onClick={() => setMonth(new Date(month.getFullYear(), month.getMonth() - 1, 1))}><ChevronLeft size={16} /></button><button onClick={() => setMonth(new Date(month.getFullYear(), month.getMonth() + 1, 1))}><ChevronRight size={16} /></button></div></div><div className="week-strip">{weekDays.map((d) => <button key={d.toISOString()} className={`week-day ${d.toDateString() === new Date(`${today()}T12:00:00-03:00`).toDateString() ? "today" : ""}`} onClick={() => setTab("calendar")}><span>{new Intl.DateTimeFormat("es-AR", { weekday: "short", timeZone: "America/Argentina/Buenos_Aires" }).format(d).replace(".", "")}</span><b>{d.getDate()}</b><i /></button>)}</div><button className="calendar-link" onClick={() => setTab("calendar")}>Abrir agenda <ArrowUpRight size={14} /></button></section></div>
    <div className="metrics-grid"><Metric icon={CheckCircle2} label="Pendientes" value={pendingCount} accent="mint" note={`${completedCount} completadas`} /><Metric icon={AlarmClock} label="Alarmas activas" value={activeAlarms} accent="peach" note={alarms.length ? `de ${alarms.length} configuradas` : "Sin horarios todavía"} /><Metric icon={Cpu} label="Dispositivo" value={activeDevice ? "Vinculado" : "Pendiente"} accent="lavender" note={activeDevice?.last_seen_at ? `Visto ${localTime(activeDevice.last_seen_at)}` : "Estado de conexión"} compact /><Metric icon={Mic2} label="Umbral de voz" value={prefs?.voice_threshold ?? "—"} accent="yellow" note="RMS · Ajustes del ESP32" /></div>
    <div className="content-grid"><section className="panel"><div className="panel-heading"><div><span className="eyebrow">EN TU LISTA</span><h2>Lo que sigue</h2></div><button className="text-button" onClick={() => setTab("tasks")}>Ver todas <ArrowUpRight size={14} /></button></div><TaskList tasks={tasks.slice(0, 5)} onToggle={toggleTask} onDelete={removeTask} onEdit={startTaskEdit} /></section><section className="panel side-panel"><div className="panel-heading"><div><span className="eyebrow">DISPOSITIVO</span><h2>{prefs?.display_name || "Tu Compañero"}</h2></div><Cpu size={20} className="muted-icon" /></div><div className="device-status-card"><div className={`device-illustration ${activeDevice ? "awake" : ""}`}><div className="device-screen"><span>{activeDevice ? "Hola 👋" : "…"}</span><i>{activeDevice ? "ESP32" : "offline"}</i></div><div className="device-knob" /></div><div><b>{activeDevice ? activeDevice.name : "Sin conexión"}</b><span>{activeDevice ? (activeDevice.last_seen_at ? `Última señal · ${localTime(activeDevice.last_seen_at)}` : "Conectado a tu cuenta") : "Vinculá la placa para empezar"}</span></div></div><div className="status-foot"><span><i className={activeDevice ? "green-dot" : "gray-dot"} />{activeDevice ? "Cuenta y dispositivo vinculados" : "Esperando vinculación"}</span><button onClick={() => setTab("device")}>Ajustes <ArrowUpRight size={13} /></button></div></section></div>
    <div className="content-grid lower-grid"><section className="panel"><div className="panel-heading"><div><span className="eyebrow">PRÓXIMA CONEXIÓN</span><h2>Tu agenda</h2></div><CalendarDays size={19} className="muted-icon" /></div><div className="integration-row"><div className="integration-logo google-logo">31</div><div><b>Google Calendar</b><span>{calendarConnected ? "Conectado · eventos confirmados" : "Conectá tu cuenta para sincronizar eventos reales"}</span></div><button className="button outline" onClick={calendarConnected ? loadCalendar : connectCalendar}>{calendarConnected ? "Actualizar" : "Conectar"}<ArrowUpRight size={14} /></button></div>{calendar.slice(0, 3).map((event) => <div className="event-line" key={event.id}><span>{event.start?.slice(11, 16) || "Todo el día"}</span><b>{event.title}</b></div>)}</section><section className="panel"><div className="panel-heading"><div><span className="eyebrow">REPRODUCTOR</span><h2>Una buena compañía</h2></div><Disc3 size={19} className="muted-icon" /></div><div className="music-card"><div className="album-art"><Music2 size={23} /></div><div><b>Música</b><span>La integración con Spotify aún requiere credenciales de proveedor.</span></div><span className="pending-tag">En preparación</span></div><button className="text-button music-settings" onClick={() => setTab("music")}>Ver detalles <ArrowUpRight size={14} /></button></section></div></>}

    {tab === "tasks" && <div className="content-grid"><section className="panel"><div className="panel-heading"><div><span className="eyebrow">TUS PENDIENTES</span><h2>{pendingCount} por hacer</h2></div><span className="sync-tag"><Check size={13} /> Guardadas en Supabase</span></div><TaskList tasks={tasks} onToggle={toggleTask} onDelete={removeTask} onEdit={startTaskEdit} /></section><section className="panel form-panel"><span className="eyebrow">{taskEditing ? "EDITAR TAREA" : "NUEVA TAREA"}</span><h2>{taskEditing ? "Ajustá el pendiente." : "Un paso a la vez."}</h2><p>Las tareas se guardan en tu cuenta y aparecen en el snapshot que consulta el ESP32.</p><form onSubmit={createTask} className="stack-form"><label>¿Qué querés hacer?<input value={taskText} onChange={(e) => setTaskText(e.target.value)} maxLength={300} required placeholder="Ej. Preparar la semana" /></label><label>Fecha límite <input type="date" value={taskDate} onChange={(e) => setTaskDate(e.target.value)} /></label><button className="button primary full" disabled={working}>{taskEditing ? <Check size={16} /> : <Plus size={16} />}{taskEditing ? "Guardar cambios" : "Agregar tarea"}</button>{taskEditing && <button type="button" className="text-button" onClick={() => { setTaskEditing(null); setTaskText(""); setTaskDate(""); }}>Cancelar edición</button>}</form><p className="form-foot"><ShieldCheck size={14} /> Solo tu cuenta puede acceder a estas tareas.</p></section></div>}

    {tab === "alarms" && <div className="content-grid"><section className="panel"><div className="panel-heading"><div><span className="eyebrow">HORARIOS DEL DISPOSITIVO</span><h2>{activeAlarms} activas</h2></div><span className="sync-tag"><Check size={13} /> Supabase</span></div>{alarms.length ? alarms.map((alarm) => <div className="alarm-row" key={alarm.id}><div className="alarm-time">{alarm.time}</div><div className="alarm-copy"><b>{alarm.label}</b><span>{alarm.days}</span></div><button className="icon-button" aria-label="Editar alarma" onClick={() => startAlarmEdit(alarm)}><Pencil size={14} /></button><button className={`toggle ${alarm.enabled ? "on" : ""}`} aria-label="Activar o pausar alarma" onClick={() => changeAlarm(alarm, { enabled: !alarm.enabled })}><i /></button><button className="icon-button danger" aria-label="Eliminar alarma" onClick={() => deleteAlarm(alarm)}><Trash2 size={16} /></button></div>) : <Empty icon={AlarmClock} title="Todavía no hay alarmas" detail="Agregá un horario para tu dispositivo." />}</section><section className="panel form-panel"><span className="eyebrow">{alarmEditing ? "EDITAR ALARMA" : "NUEVA ALARMA"}</span><h2>{alarmEditing ? "Ajustá el horario." : "Hacé espacio para una pausa."}</h2><p>Los cambios se guardan en la cuenta. La placa conserva su ejecución local cuando reciba una configuración.</p><form className="stack-form" onSubmit={createAlarm}><label>Nombre<input required maxLength={50} value={alarmLabel} onChange={(e) => setAlarmLabel(e.target.value)} placeholder="Un buen comienzo" /></label><label>Hora<input required type="time" value={alarmTime} onChange={(e) => setAlarmTime(e.target.value)} /></label><label>Repetición<select value={alarmDays} onChange={(e) => setAlarmDays(e.target.value)}>{["Todos los días", "Lun a vie", "Fin de semana", "Una vez"].map((x) => <option key={x}>{x}</option>)}</select></label><button className="button primary full" disabled={!activeDevice || working}>{alarmEditing ? <Check size={16} /> : <Plus size={16} />}{alarmEditing ? "Guardar cambios" : "Agregar alarma"}</button>{alarmEditing && <button type="button" className="text-button" onClick={() => { setAlarmEditing(null); setAlarmLabel(""); }}>Cancelar edición</button>}{!activeDevice && <span className="hint">Vinculá tu ESP32 para guardar horarios en el backend.</span>}</form></section></div>}

    {tab === "calendar" && <section className="panel event-editor"><div className="panel-heading"><div><span className="eyebrow">GESTIONAR EVENTOS</span><h2>Tu agenda, en orden.</h2></div><span className="sync-tag">Google Calendar</span></div>{calendars.length > 0 && <label className="calendar-select">Calendario<select value={selectedCalendar} onChange={async (e) => run(async () => { const id = e.target.value; await edgePost("web-calendar-select", { calendar_id: id }); setSelectedCalendar(id); setCalendar([]); }, "Calendario seleccionado.")}>{calendars.map((item) => <option key={item.id} value={item.id}>{item.name}{item.primary ? " · Principal" : ""}</option>)}</select></label>}<form className="event-form" onSubmit={createCalendarEvent}><label>Título<input value={eventTitle} onChange={(e) => setEventTitle(e.target.value)} required maxLength={300} placeholder="Un rato para crear" /></label><div className="event-fields"><label>Inicio<input type="datetime-local" value={eventStart} onChange={(e) => setEventStart(e.target.value)} required /></label><label>Fin<input type="datetime-local" value={eventEnd} onChange={(e) => setEventEnd(e.target.value)} required /></label></div><button className="button primary" disabled={!calendarConnected || working}><Plus size={15} /> Crear evento</button>{!calendarConnected && <span className="hint">Vinculá Google Calendar para administrar eventos reales.</span>}</form>{calendar.length > 0 && <div className="event-editor-list">{calendar.map((item) => <div className="event-editor-row" key={item.id}><span>{item.start?.slice(11, 16) || "Todo el día"}</span><b>{item.title}</b><button className="text-button" onClick={() => updateCalendarEvent(item)}>Editar</button><button className="icon-button danger" aria-label="Eliminar evento" onClick={() => deleteCalendarEvent(item)}><Trash2 size={14} /></button></div>)}</div>}</section>}    {tab === "calendar" && <div className="content-grid"><section className="panel"><div className="panel-heading"><div><span className="eyebrow">GOOGLE CALENDAR</span><h2>Agenda real</h2></div><button className="button outline" onClick={calendarConnected ? loadCalendar : connectCalendar} disabled={calendarBusy}>{calendarBusy ? <LoaderCircle className="spin" /> : calendarConnected ? <RefreshCw size={15} /> : <Plus size={15} />}{calendarConnected ? "Actualizar" : "Vincular Google"}</button></div><div className="month-header"><button className="icon-button" onClick={() => setMonth(new Date(month.getFullYear(), month.getMonth() - 1, 1))}><ChevronLeft size={17} /></button><b>{new Intl.DateTimeFormat("es-AR", { month: "long", year: "numeric" }).format(month)}</b><button className="icon-button" onClick={() => setMonth(new Date(month.getFullYear(), month.getMonth() + 1, 1))}><ChevronRight size={17} /></button></div><div className="month-grid">{["L", "M", "M", "J", "V", "S", "D"].map((x, i) => <span className="weekday" key={`${x}-${i}`}>{x}</span>)}{Array.from({ length: 35 }, (_, i) => { const d = new Date(month.getFullYear(), month.getMonth(), 1); const offset = (d.getDay() + 6) % 7; d.setDate(i - offset + 1); const isCurrent = d.getMonth() === month.getMonth(); return <button key={i} className={`month-day ${isCurrent ? "" : "outside"} ${d.toDateString() === new Date(`${today()}T12:00:00-03:00`).toDateString() ? "today" : ""}`} onClick={() => {}}>{d.getDate()}</button>; })}</div><p className="hint">Los eventos aparecen en esta agenda después de consultar Google. El ESP32 confirma su recepción por separado.</p></section><section className="panel"><div className="panel-heading"><div><span className="eyebrow">EVENTOS DE HOY</span><h2>{calendar.length ? `${calendar.length} en tu agenda` : "Tu tiempo, organizado."}</h2></div><CalendarDays size={19} className="muted-icon" /></div>{calendar.length ? calendar.map((event) => <div className="agenda-event" key={event.id}><div className="event-time">{event.start?.slice(11, 16) || "Todo el día"}</div><div><b>{event.title}</b><span>{event.location || "Google Calendar"}</span></div></div>) : <Empty icon={CalendarDays} title="No hay eventos cargados" detail="Vinculá Google y consultá la agenda del día." />}</section></div>}

    {tab === "music" && <section className="panel integration-page"><div className="integration-hero"><div className="music-orb"><Disc3 size={38} /></div><div><span className="eyebrow">MÚSICA</span><h2>Tu ritmo, a tu manera.</h2><p>La conexión con servicios externos se habilitará cuando se configuren las credenciales OAuth en el backend.</p></div></div><div className="provider-row"><div className="provider-logo spotify-logo">●</div><div><b>Spotify</b><span>El panel todavía no controla la reproducción del proveedor.</span></div><span className="pending-tag">Requiere OAuth</span></div><div className="provider-row"><div className="provider-logo">♫</div><div><b>Estado en la placa</b><span>El firmware actual no expone controles de reproducción al dashboard.</span></div><span className="pending-tag">No conectado</span></div></section>}

    {tab === "energy" && <EnergyPanel deviceName={activeDevice?.name ?? null} lastSeenAt={activeDevice?.last_seen_at ?? null} autoVoice={prefs?.auto_voice ?? false} />}

    {tab === "device" && <div className="device-page"><section className="panel"><div className="panel-heading"><div><span className="eyebrow">VINCULACIÓN</span><h2>Tu ESP32</h2></div><span className={`connection-pill ${activeDevice ? "connected" : ""}`}><i />{activeDevice ? "Vinculado" : "Sin vincular"}</span></div>{devices.map((device) => <div className="device-row" key={device.id}><div className="device-large"><Cpu size={22} /></div><div><b>{device.name}</b><span>{device.status === "active" ? device.last_seen_at ? `Último contacto · ${localTime(device.last_seen_at)}` : "Vinculado · aún sin señal" : `Estado · ${device.status}`}</span></div><button className="button outline" onClick={pairDevice}><RefreshCw size={14} /> Agregar placa</button></div>)}{!devices.length && <Empty icon={Cpu} title="Vinculá tu primera placa" detail="El código caduca a los 10 minutos y se ingresa una sola vez desde el dispositivo." />}<div className="pair-instructions"><span className="step-number">01</span><p>Generá un código desde acá y escribí <code>pair CÓDIGO</code> en la consola serie.</p><span className="step-number">02</span><p>El ESP32 inicia sesión como dispositivo y aparece en tu cuenta.</p></div></section><form className="panel prefs-panel" onSubmit={savePrefs}><div className="panel-heading"><div><span className="eyebrow">CONFIGURACIÓN</span><h2>Preferencias del dispositivo</h2></div><Settings2 size={19} className="muted-icon" /></div>{prefs ? <><label>Nombre<input name="display_name" key={prefs.revision} defaultValue={prefs.display_name} maxLength={32} required /></label><label>Reposo tras<select name="idle_seconds" defaultValue={prefs.idle_seconds}><option value="15">15 segundos</option><option value="30">30 segundos</option><option value="60">60 segundos</option><option value="120">120 segundos</option></select></label><label>Umbral de voz<select name="voice_threshold" defaultValue={prefs.voice_threshold}><option value="500">500 RMS · más sensible</option><option value="800">800 RMS · ambiente ruidoso</option><option value="1200">1200 RMS · menos sensible</option></select></label><label className="check-setting"><input type="checkbox" name="calendar_rest" defaultChecked={prefs.calendar_rest} /> Mostrar calendario en reposo</label><label className="check-setting"><input type="checkbox" name="auto_voice" defaultChecked={prefs.auto_voice} /> Activación automática por voz</label><button className="button primary" disabled={working}><Check size={15} /> Guardar ajustes</button><p className="hint">Guardado en Supabase con control de revisión. El servicio aún no informa la revisión aplicada por la placa.</p></> : <Empty icon={Settings2} title="Vinculá un dispositivo" detail="Las preferencias se guardan para una placa activa de tu cuenta." />}</form></div>}

    <footer className="page-footer"><span><ShieldCheck size={14} /> Datos privados de tu cuenta</span><span>Compañero · Panel de dispositivo</span></footer></div></main>
    {notice && <Toast notice={notice} onClose={() => setNotice(null)} />}{working && <div className="working-indicator"><LoaderCircle size={16} className="spin" /> Guardando</div>}
  </div>;
}

function Brand() { return <div className="brand"><div className="brand-mark"><span /><span /><span /><span /></div><div><b>compañero</b><small>ESP32 · DESKTOP</small></div></div>; }
function Metric({ icon: Icon, label, value, accent, note, compact = false }: { icon: typeof LayoutDashboard; label: string; value: string | number; accent: string; note: string; compact?: boolean }) { return <div className="metric-card"><div className={`metric-icon ${accent}`}><Icon size={18} /></div><span className="metric-label">{label}</span><strong className={compact ? "compact-value" : ""}>{value}</strong><small>{note}</small></div>; }
function TaskList({ tasks, onToggle, onDelete, onEdit }: { tasks: Task[]; onToggle: (task: Task) => void; onDelete: (task: Task) => void; onEdit: (task: Task) => void }) { return tasks.length ? <div className="task-list">{tasks.map((task) => <div className={`task-row ${task.status === "completed" ? "is-done" : ""}`} key={task.id}><button className={`task-check ${task.status === "completed" ? "checked" : ""}`} aria-label="Completar tarea" onClick={() => onToggle(task)}>{task.status === "completed" && <Check size={13} />}</button><div className="task-info"><b>{task.title}</b><span>{task.due_date ? `Fecha · ${new Intl.DateTimeFormat("es-AR", { dateStyle: "medium", timeZone: "UTC" }).format(new Date(`${task.due_date}T12:00:00Z`))}` : "Sin fecha límite"}</span></div><button className="icon-button" aria-label="Editar tarea" onClick={() => onEdit(task)}><Pencil size={14} /></button><button className="icon-button danger" aria-label="Eliminar tarea" onClick={() => onDelete(task)}><Trash2 size={15} /></button></div>)}</div> : <Empty icon={CheckCircle2} title="Un espacio para empezar" detail="Agregá tu primera tarea. Se guardará en tu cuenta." />; }
function Empty({ icon: Icon, title, detail }: { icon: typeof LayoutDashboard; title: string; detail: string }) { return <div className="empty-state"><div><Icon size={19} /></div><b>{title}</b><span>{detail}</span></div>; }
function Toast({ notice, onClose }: { notice: Notice; onClose: () => void }) { return <div className={`toast ${notice.kind}`}><span>{notice.kind === "error" ? <X size={16} /> : <Check size={16} />}</span>{notice.text}<button aria-label="Cerrar aviso" onClick={onClose}><X size={15} /></button></div>; }
function pageDescription(tab: Tab) { return ({ overview: "", tasks: "Organizá lo pendiente y compartilo con la placa.", alarms: "Prepará horarios para tu compañero de escritorio.", calendar: "Consultá tus eventos de Google Calendar.", music: "Configurá el proveedor cuando el servicio esté listo.", device: "Vinculación, conexión y preferencias.", energy: "Diseñá un borrador de reposo y autonomía; todavía no está disponible para guardar." } as Record<Tab, string>)[tab]; }
