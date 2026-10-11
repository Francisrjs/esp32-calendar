export const ENERGY_TIMEZONE = "America/Argentina/Buenos_Aires" as const;

export const ENERGY_WEEKDAYS = [
  { iso: 1, short: "Lun", long: "lunes" },
  { iso: 2, short: "Mar", long: "martes" },
  { iso: 3, short: "Mié", long: "miércoles" },
  { iso: 4, short: "Jue", long: "jueves" },
  { iso: 5, short: "Vie", long: "viernes" },
  { iso: 6, short: "Sáb", long: "sábado" },
  { iso: 7, short: "Dom", long: "domingo" },
] as const;

export type EnergyProfile = "always_available" | "balanced" | "maximum_saving" | "custom";
export type AbsenceRule = {
  id: string;
  enabled: boolean;
  label: string;
  weekdays: number[];
  startTime: string;
  endTime: string;
  timezone: string;
};

export type RuleIssue = { ruleId: string; message: string };
export type RuleOverlap = { firstId: string; secondId: string; weekday: number };

const WEEK_MINUTES = 7 * 24 * 60;

export function parseClockTime(value: string): number | null {
  const match = /^(?:([01]\d|2[0-3])):([0-5]\d)$/.exec(value);
  if (!match) return null;
  return Number(match[1]) * 60 + Number(match[2]);
}

export function validateAbsenceRules(rules: AbsenceRule[]): RuleIssue[] {
  return rules.flatMap((rule) => {
    const issues: RuleIssue[] = [];
    if (!rule.label.trim()) issues.push({ ruleId: rule.id, message: "Escribí un nombre para la regla." });
    if (!rule.weekdays.length) issues.push({ ruleId: rule.id, message: "Elegí al menos un día." });
    if (rule.weekdays.some((day) => !Number.isInteger(day) || day < 1 || day > 7)) {
      issues.push({ ruleId: rule.id, message: "Hay un día que no pertenece a la semana ISO." });
    }

    const start = parseClockTime(rule.startTime);
    const end = parseClockTime(rule.endTime);
    if (start === null) issues.push({ ruleId: rule.id, message: "La hora inicial debe usar formato HH:MM de 24 horas." });
    if (end === null) issues.push({ ruleId: rule.id, message: "La hora final debe usar formato HH:MM de 24 horas." });
    if (start !== null && end !== null && start === end) {
      issues.push({ ruleId: rule.id, message: "El inicio y el fin no pueden ser iguales. Para un día completo hace falta un contrato explícito." });
    }
    if (rule.timezone !== ENERGY_TIMEZONE) {
      issues.push({ ruleId: rule.id, message: `Por ahora solo está admitida la zona ${ENERGY_TIMEZONE}.` });
    }
    return issues;
  });
}

function weeklyIntervals(rule: AbsenceRule) {
  const start = parseClockTime(rule.startTime);
  const end = parseClockTime(rule.endTime);
  if (start === null || end === null || start === end) return [];

  return rule.weekdays.flatMap((weekday) => {
    if (weekday < 1 || weekday > 7) return [];
    const startOfDay = (weekday - 1) * 24 * 60;
    const absoluteStart = startOfDay + start;
    const absoluteEnd = startOfDay + end + (end <= start ? 24 * 60 : 0);
    if (absoluteEnd <= WEEK_MINUTES) return [{ start: absoluteStart, end: absoluteEnd, weekday }];
    return [
      { start: absoluteStart, end: WEEK_MINUTES, weekday },
      { start: 0, end: absoluteEnd - WEEK_MINUTES, weekday: 1 },
    ];
  });
}

export function findRuleOverlaps(rules: AbsenceRule[]): RuleOverlap[] {
  const applicable = rules.filter((rule) => rule.enabled && validateAbsenceRules([rule]).length === 0);
  const conflicts = new Map<string, RuleOverlap>();

  applicable.forEach((rule, index) => {
    const intervals = weeklyIntervals(rule);
    const followingWeek = intervals.map((interval) => ({ ...interval, start: interval.start + WEEK_MINUTES, end: interval.end + WEEK_MINUTES }));
    for (const other of applicable.slice(index + 1)) {
      const otherIntervals = [...weeklyIntervals(other), ...weeklyIntervals(other).map((interval) => ({ ...interval, start: interval.start + WEEK_MINUTES, end: interval.end + WEEK_MINUTES }))];
      for (const left of [...intervals, ...followingWeek]) {
        for (const right of otherIntervals) {
          if (left.start < right.end && right.start < left.end) {
            const startWeekMinute = left.start % WEEK_MINUTES;
            const weekday = Math.floor(startWeekMinute / (24 * 60)) + 1;
            const key = [rule.id, other.id].sort().join(":");
            conflicts.set(key, { firstId: rule.id, secondId: other.id, weekday });
          }
        }
      }
    }
  });
  return [...conflicts.values()];
}

export function formatRulePeriod(rule: AbsenceRule): string {
  if (!rule.weekdays.length) return "Elegí días para ver el resumen.";
  const start = parseClockTime(rule.startTime);
  const end = parseClockTime(rule.endTime);
  const days = rule.weekdays
    .filter((day) => day >= 1 && day <= 7)
    .sort((a, b) => a - b)
    .map((day) => ENERGY_WEEKDAYS[day - 1].short);
  if (start === null || end === null || start === end) return `${days.join(", ")} · completá un intervalo válido`;
  if (end <= start) {
    return rule.weekdays
      .filter((day) => day >= 1 && day <= 7)
      .sort((a, b) => a - b)
      .map((day) => `${ENERGY_WEEKDAYS[day - 1].short} ${rule.startTime} → ${ENERGY_WEEKDAYS[day % 7].short} ${rule.endTime}`)
      .join(" · ");
  }
  return `${days.join(", ")} ${rule.startTime}–${rule.endTime}`;
}

export function describeEnergyDraft(input: {
  profile: EnergyProfile | "";
  microphoneIdle: "continuous" | "off" | "";
  wifiMode: "continuous" | "intervals" | "";
  wifiIntervalMinutes: number | "";
  sleepMode: "none" | "light" | "deep" | "";
  sleepAfterSeconds: number | "";
  remoteCheckMinutes: 30 | 60 | null;
  wakeForAlarm: boolean;
}): string[] {
  const summary: string[] = [];
  const profileNames: Record<EnergyProfile, string> = {
    always_available: "Siempre disponible",
    balanced: "Equilibrado",
    maximum_saving: "Ahorro máximo",
    custom: "Personalizado",
  };
  if (input.profile) summary.push(`Perfil ${profileNames[input.profile]}.`);
  if (input.microphoneIdle) {
    summary.push(input.microphoneIdle === "continuous"
      ? "El micrófono permanece activo durante el reposo."
      : "El micrófono se apaga durante el reposo; se necesitaría KEY para hablar.");
  }
  if (input.wifiMode === "continuous") summary.push("Wi‑Fi conectado con ahorro, sujeto a soporte del firmware.");
  if (input.wifiMode === "intervals") summary.push(`Wi‑Fi por intervalos${input.wifiIntervalMinutes ? ` de ${input.wifiIntervalMinutes} min` : " (falta elegir intervalo)"}.`);
  if (input.sleepMode) {
    const sleepNames = { none: "sin suspensión", light: "suspensión ligera", deep: "suspensión profunda" };
    const delay = input.sleepAfterSeconds ? ` tras ${input.sleepAfterSeconds} s` : "";
    summary.push(`Procesador: ${sleepNames[input.sleepMode]}${delay}.`);
  }
  if (input.sleepMode && input.sleepMode !== "none") summary.push("La pantalla se apagaría durante la suspensión.");
  summary.push(input.remoteCheckMinutes
    ? `Comprobación remota cada ${input.remoteCheckMinutes} min; los cambios podrían demorarse hasta ese intervalo.`
    : "Sin comprobaciones remotas durante la ausencia; los cambios esperarían al fin del horario o a KEY.");
  summary.push(`Despertar por alarma: ${input.wakeForAlarm ? "propuesto" : "desactivado"}; falta confirmar el contrato.`);
  return summary;
}
