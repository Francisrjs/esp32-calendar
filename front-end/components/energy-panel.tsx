"use client";

import { useMemo, useState } from "react";
import { AlertTriangle, BatteryCharging, CalendarClock, Check, Clock3, Info, Moon, Plus, Radio, Trash2, Wifi, Zap } from "lucide-react";
import styles from "./energy-panel.module.css";
import {
  AbsenceRule,
  describeEnergyDraft,
  ENERGY_TIMEZONE,
  ENERGY_WEEKDAYS,
  EnergyProfile,
  findRuleOverlaps,
  formatRulePeriod,
  validateAbsenceRules,
} from "@/lib/energy";

type EnergyPanelProps = {
  deviceName: string | null;
  lastSeenAt: string | null;
  autoVoice: boolean;
};

const newRule = (): AbsenceRule => ({
  id: crypto.randomUUID(),
  enabled: false,
  label: "",
  weekdays: [],
  startTime: "",
  endTime: "",
  timezone: ENERGY_TIMEZONE,
});

export function EnergyPanel({ deviceName, lastSeenAt, autoVoice }: EnergyPanelProps) {
  const [profile, setProfile] = useState<EnergyProfile | "">("");
  const [microphoneIdle, setMicrophoneIdle] = useState<"continuous" | "off" | "">("");
  const [wifiMode, setWifiMode] = useState<"continuous" | "intervals" | "">("");
  const [wifiIntervalMinutes, setWifiIntervalMinutes] = useState<number | "">("");
  const [sleepMode, setSleepMode] = useState<"none" | "light" | "deep" | "">("");
  const [sleepAfterSeconds, setSleepAfterSeconds] = useState<number | "">("");
  const [remoteCheckMinutes, setRemoteCheckMinutes] = useState<30 | 60 | null>(null);
  const [wakeForAlarm, setWakeForAlarm] = useState(true);
  const [rules, setRules] = useState<AbsenceRule[]>([]);

  const issues = useMemo(() => validateAbsenceRules(rules), [rules]);
  const overlaps = useMemo(() => findRuleOverlaps(rules), [rules]);
  const summary = useMemo(() => describeEnergyDraft({
    profile,
    microphoneIdle,
    wifiMode,
    wifiIntervalMinutes,
    sleepMode,
    sleepAfterSeconds,
    remoteCheckMinutes,
    wakeForAlarm,
  }), [profile, microphoneIdle, wifiMode, wifiIntervalMinutes, sleepMode, sleepAfterSeconds, remoteCheckMinutes, wakeForAlarm]);

  const patchRule = (id: string, patch: Partial<AbsenceRule>) => {
    setRules((current) => current.map((rule) => rule.id === id ? { ...rule, ...patch } : rule));
  };
  const toggleDay = (rule: AbsenceRule, weekday: number) => {
    const weekdays = rule.weekdays.includes(weekday)
      ? rule.weekdays.filter((day) => day !== weekday)
      : [...rule.weekdays, weekday].sort((a, b) => a - b);
    patchRule(rule.id, { weekdays });
  };

  return (
    <div className={styles.page}>
      <section className={styles.intro} aria-labelledby="energy-title">
        <div className={styles.introCopy}>
          <span className={styles.eyebrow}>ENERGÍA · AJUSTES DEL DISPOSITIVO</span>
          <h2 id="energy-title">Más descanso, con límites claros.</h2>
          <p>Prepará un borrador para el consumo y los horarios de ausencia. El ESP32 ejecuta estas políticas localmente cuando el firmware y el backend las soporten.</p>
        </div>
        <div className={styles.statusBadge}><span /> No disponible para guardar</div>
      </section>

      <div className={styles.dependency} role="status">
        <Info size={18} aria-hidden="true" />
        <div><strong>Este dispositivo todavía no informa capacidades de energía.</strong><span>Podés explorar el borrador en esta sesión. No se guarda en Supabase ni se envía a la placa; al recargar la página se descarta.</span></div>
      </div>

      <section className={styles.telemetry} aria-label="Estado informado por el dispositivo">
        <article className={styles.telemetryCard}><span className={styles.telemetryIcon}><Radio size={17} /></span><div><small>DISPOSITIVO ACTIVO</small><strong>{deviceName || "Sin dispositivo vinculado"}</strong><span>{deviceName ? "La configuración corresponde a esta placa." : "Vinculá un ESP32 para consultar su estado."}</span></div></article>
        <article className={styles.telemetryCard}><span className={styles.telemetryIcon}><Clock3 size={17} /></span><div><small>ÚLTIMO CONTACTO</small><strong>{lastSeenAt ? formatDate(lastSeenAt) : "Sin dato"}</strong><span>Es una conexión, no una confirmación de aplicación.</span></div></article>
        <article className={styles.telemetryCard}><span className={styles.telemetryIcon}><Zap size={17} /></span><div><small>REVISIÓN APLICADA</small><strong>No informada</strong><span>La escritura y la recepción de la placa son estados distintos.</span></div></article>
      </section>

      <div className={styles.columns}>
        <section className={styles.card} aria-labelledby="energy-config-title">
          <div className={styles.cardHeading}><span className={styles.cardIcon}><BatteryCharging size={19} /></span><div><span className={styles.eyebrow}>BORRADOR DE ESTA SESIÓN</span><h3 id="energy-config-title">Energía del dispositivo</h3></div><span className={styles.draftTag}>No guardado</span></div>
          <div className={styles.fields}>
            <label className={styles.field}>Perfil energético
              <select value={profile} onChange={(event) => setProfile(event.target.value as EnergyProfile | "")}>
                <option value="">Elegí un perfil</option>
                <option value="always_available">Siempre disponible</option>
                <option value="balanced">Equilibrado</option>
                <option value="maximum_saving">Ahorro máximo</option>
                <option value="custom">Personalizado</option>
              </select>
            </label>
            <label className={styles.field}>Micrófono durante el reposo
              <select value={microphoneIdle} onChange={(event) => setMicrophoneIdle(event.target.value as "continuous" | "off" | "")}>
                <option value="">Elegí el comportamiento</option>
                <option value="continuous">Escucha continua</option>
                <option value="off">Micrófono apagado · se requiere KEY para hablar</option>
              </select>
            </label>
            <label className={styles.field}>Modo de Wi‑Fi
              <select value={wifiMode} onChange={(event) => { setWifiMode(event.target.value as "continuous" | "intervals" | ""); if (event.target.value !== "intervals") setWifiIntervalMinutes(""); }}>
                <option value="">Elegí el comportamiento</option>
                <option value="continuous">Conectado con ahorro</option>
                <option value="intervals">Conexión por intervalos</option>
              </select>
            </label>
            {wifiMode === "intervals" && <label className={styles.field}>Intervalo de conexión
              <select value={wifiIntervalMinutes} onChange={(event) => setWifiIntervalMinutes(event.target.value ? Number(event.target.value) : "")}>
                <option value="">Elegí una frecuencia</option><option value="5">Cada 5 minutos</option><option value="15">Cada 15 minutos</option><option value="30">Cada 30 minutos</option><option value="60">Cada 60 minutos</option>
              </select>
            </label>}
            <label className={styles.field}>Suspensión por inactividad
              <select value={sleepMode} onChange={(event) => { setSleepMode(event.target.value as "none" | "light" | "deep" | ""); if (event.target.value === "none" || !event.target.value) setSleepAfterSeconds(""); }}>
                <option value="">Elegí el comportamiento</option><option value="none">Ninguna</option><option value="light">Ligera</option><option value="deep">Profunda · apagado lógico</option>
              </select>
            </label>
            {sleepMode && sleepMode !== "none" && <label className={styles.field}>Suspender después de
              <select value={sleepAfterSeconds} onChange={(event) => setSleepAfterSeconds(event.target.value ? Number(event.target.value) : "")}>
                <option value="">Elegí una demora</option><option value="30">30 segundos</option><option value="60">60 segundos</option><option value="120">120 segundos</option><option value="300">300 segundos</option>
              </select>
            </label>}
            <label className={styles.field}>Comprobación remota durante ausencia
              <select value={remoteCheckMinutes ?? "none"} onChange={(event) => setRemoteCheckMinutes(event.target.value === "none" ? null : Number(event.target.value) as 30 | 60)}>
                <option value="none">Desactivada</option><option value="30">Cada 30 minutos</option><option value="60">Cada 60 minutos</option>
              </select>
            </label>
            <div className={styles.fieldGroup}>
              <span className={styles.fieldLabel}>Pantalla durante la suspensión</span>
              <span className={styles.staticValue}><Moon size={15} /> Apagada <span>Requisito actual del plan</span></span>
              <small>Conservar la imagen sólo se ofrecerá cuando el firmware confirme compatibilidad.</small>
            </div>
            <label className={styles.checkField}><input type="checkbox" checked={wakeForAlarm} onChange={(event) => setWakeForAlarm(event.target.checked)} /><span><b>Despertar por alarma</b><small>Propuesta activa en el borrador; pendiente de confirmar con el contrato.</small></span></label>
          </div>
          {microphoneIdle === "off" && autoVoice && <div className={styles.warning} role="alert"><AlertTriangle size={16} /><span>El ajuste actual mantiene la activación por voz. Apagar el micrófono sería incompatible; no se cambia esa preferencia automáticamente.</span></div>}
          <div className={styles.summaryBox}><span className={styles.eyebrow}>COMPORTAMIENTO PROPUESTO</span>{summary.length ? <ul>{summary.map((item) => <li key={item}>{item}</li>)}</ul> : <p>Elegí las opciones para construir un resumen. Todavía no hay una política guardada.</p>}<small>El resumen describe este borrador; no confirma actividad de Wi‑Fi, micrófono ni procesador.</small></div>
        </section>

        <section className={styles.card} aria-labelledby="absence-title">
          <div className={styles.cardHeading}><span className={styles.cardIcon}><CalendarClock size={19} /></span><div><span className={styles.eyebrow}>REGLAS LOCALES</span><h3 id="absence-title">Suspensión programada</h3></div></div>
          <p className={styles.cardLead}>Definí horarios de ausencia. Si cruzan medianoche, el resumen indica el día de salida. Una regla sólo sería efectiva después de guardarse y aplicarse en la placa.</p>
          <div className={styles.timezone}><span>Zona horaria</span><b>{ENERGY_TIMEZONE}</b><small>Es la única zona admitida en esta etapa.</small></div>
          <div className={styles.ruleList}>
            {!rules.length && <div className={styles.emptyRules}><CalendarClock size={20} /><b>Sin reglas de ausencia</b><span>El horario de lunes a viernes no se precarga. Agregá una regla y elegí expresamente sus días.</span></div>}
            {rules.map((rule) => {
              const ruleIssues = issues.filter((issue) => issue.ruleId === rule.id);
              const conflict = overlaps.some((overlap) => overlap.firstId === rule.id || overlap.secondId === rule.id);
              return <article className={styles.rule} key={rule.id}>
                <div className={styles.ruleTop}><label className={styles.ruleName}>Nombre de la regla<input value={rule.label} maxLength={60} onChange={(event) => patchRule(rule.id, { label: event.target.value })} placeholder="Ej. Horario de trabajo" /></label><label className={styles.ruleToggle}><input type="checkbox" checked={rule.enabled} onChange={(event) => patchRule(rule.id, { enabled: event.target.checked })} /><span>{rule.enabled ? "En borrador" : "Pausada"}</span></label><button className={styles.removeRule} type="button" aria-label={`Eliminar regla ${rule.label || "sin nombre"}`} onClick={() => setRules((current) => current.filter((item) => item.id !== rule.id))}><Trash2 size={16} /></button></div>
                <span className={styles.fieldLabel}>Días de la semana</span>
                <div className={styles.weekdayPicker}>{ENERGY_WEEKDAYS.map((day) => <button key={day.iso} type="button" className={rule.weekdays.includes(day.iso) ? styles.daySelected : ""} aria-pressed={rule.weekdays.includes(day.iso)} onClick={() => toggleDay(rule, day.iso)}>{day.short}</button>)}</div>
                <div className={styles.timeFields}><label>Inicio<input type="time" value={rule.startTime} onChange={(event) => patchRule(rule.id, { startTime: event.target.value })} /></label><span aria-hidden="true">→</span><label>Fin<input type="time" value={rule.endTime} onChange={(event) => patchRule(rule.id, { endTime: event.target.value })} /></label></div>
                <p className={styles.ruleSummary}>{formatRulePeriod(rule)}{rule.enabled ? " · activa en el borrador" : " · pausada"}</p>
                {ruleIssues.map((issue) => <p className={styles.inlineIssue} key={issue.message}><AlertTriangle size={13} />{issue.message}</p>)}
                {conflict && <p className={styles.inlineIssue}><AlertTriangle size={13} />Se solapa con otra regla. La combinación que aplicará el dispositivo queda pendiente del contrato de firmware.</p>}
              </article>;
            })}
          </div>
          <button className={styles.addRule} type="button" onClick={() => setRules((current) => [...current, newRule()])}><Plus size={16} /> Nueva regla</button>
          {rules.length > 0 && <div className={styles.weekSummary}><span className={styles.eyebrow}>RESUMEN SEMANAL · BORRADOR</span>{ENERGY_WEEKDAYS.map((day) => {
            const dayRules = rules.filter((rule) => rule.enabled && rule.weekdays.includes(day.iso));
            return <div key={day.iso}><b>{day.short}</b><span>{dayRules.length ? dayRules.map((rule) => `${rule.label || "Sin nombre"} · ${rule.startTime || "—"}–${rule.endTime || "—"}`).join(" / ") : "Sin suspensión programada"}</span></div>;
          })}<p>El próximo período y despertar se mostrarán cuando la placa los calcule e informe.</p></div>}
        </section>
      </div>

      <section className={styles.battery} aria-labelledby="battery-title">
        <div className={styles.batteryHeading}><span className={styles.cardIcon}><BatteryCharging size={18} /></span><div><span className={styles.eyebrow}>ALIMENTACIÓN</span><h3 id="battery-title">Batería y autonomía</h3></div><span className={styles.pendingTag}>Autonomía todavía no medida</span></div>
        <div className={styles.batteryFacts}><div><small>CAPACIDAD INFORMADA</small><strong>2,6 Ah</strong></div><div><small>TENSIÓN NOMINAL</small><strong>3,7 V</strong></div><div><small>ENERGÍA DE ETIQUETA</small><strong>9,26 Wh</strong></div><div><small>MEDICIÓN DE PLACA</small><strong>No disponible</strong></div></div>
        <div className={styles.batteryNote}><AlertTriangle size={16} /><span><b>Hay una diferencia en la etiqueta.</b> 2,6 Ah × 3,7 V da 9,62 Wh. Revisá el dato de origen antes de corregirlo; el panel no lo modifica. Sin consumo medido de la placa completa no se estiman horas restantes.</span></div>
        <p className={styles.telemetryFoot}><Wifi size={14} /> Voltaje, carga y estado de alimentación sólo aparecerán cuando el firmware documente y envíe una medición.</p>
      </section>

      <footer className={styles.footer}><span><Check size={14} /> Borrador aislado del dispositivo</span><span>Sin escritura en Supabase · Sin comandos remotos</span></footer>
    </div>
  );
}

function formatDate(value: string) {
  const date = new Date(value);
  if (!Number.isFinite(date.getTime())) return "Dato inválido";
  return new Intl.DateTimeFormat("es-AR", { dateStyle: "medium", timeStyle: "short", timeZone: ENERGY_TIMEZONE }).format(date);
}
