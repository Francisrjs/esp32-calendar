const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");
const vm = require("node:vm");
const ts = require("typescript");

const source = fs.readFileSync(path.join(__dirname, "..", "lib", "energy.ts"), "utf8");
const compiled = { exports: {} };
const javascript = ts.transpileModule(source, {
  compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 },
}).outputText;
vm.runInNewContext(javascript, { exports: compiled.exports, module: compiled, require });
const { findRuleOverlaps, formatRulePeriod, parseClockTime, validateAbsenceRules } = compiled.exports;

const rule = (id, patch = {}) => ({
  id,
  enabled: true,
  label: id,
  weekdays: [1],
  startTime: "08:00",
  endTime: "17:00",
  timezone: "America/Argentina/Buenos_Aires",
  ...patch,
});

test("accepts only 24-hour HH:MM values", () => {
  assert.equal(parseClockTime("00:00"), 0);
  assert.equal(parseClockTime("23:59"), 1439);
  assert.equal(parseClockTime("24:00"), null);
  assert.equal(parseClockTime("8:00"), null);
});

test("rejects empty weekdays, same-time intervals, and unsupported zones", () => {
  const issues = validateAbsenceRules([rule("bad", { weekdays: [], endTime: "08:00", timezone: "UTC" })]);
  assert.equal(issues.length, 3);
});

test("formats each overnight day with its actual exit day", () => {
  assert.equal(
    formatRulePeriod(rule("overnight", { weekdays: [1, 3, 7], startTime: "22:00", endTime: "07:00" })),
    "Lun 22:00 → Mar 07:00 · Mié 22:00 → Jue 07:00 · Dom 22:00 → Lun 07:00",
  );
});

test("detects overlaps through midnight and across the Sunday/Monday boundary", () => {
  assert.equal(findRuleOverlaps([
    rule("monday", { weekdays: [1], startTime: "22:00", endTime: "07:00" }),
    rule("tuesday", { weekdays: [2], startTime: "06:00", endTime: "08:00" }),
  ]).length, 1);
  assert.equal(findRuleOverlaps([
    rule("sunday", { weekdays: [7], startTime: "22:00", endTime: "02:00" }),
    rule("monday", { weekdays: [1], startTime: "01:00", endTime: "03:00" }),
  ]).length, 1);
});

test("does not report adjacent, non-overlapping rules as conflicts", () => {
  assert.equal(findRuleOverlaps([
    rule("morning", { weekdays: [1], startTime: "08:00", endTime: "12:00" }),
    rule("afternoon", { weekdays: [1], startTime: "12:00", endTime: "17:00" }),
  ]).length, 0);
});
