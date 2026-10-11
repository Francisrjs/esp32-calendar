-- Restrict dashboard writes to editable fields; identity and ownership columns stay immutable.
revoke insert, update, delete on table public.esp32_tasks from authenticated;
grant delete on table public.esp32_tasks to authenticated;
grant insert (owner_id, title, due_date, status, timezone) on public.esp32_tasks to authenticated;
grant update (title, notes, status, priority, due_date, due_at, remind_at, timezone, completed_at) on public.esp32_tasks to authenticated;

revoke update on table public.esp32_device_settings from authenticated;
grant update (display_name, voice_threshold, auto_voice, idle_seconds, calendar_rest) on public.esp32_device_settings to authenticated;

revoke insert, update, delete on table public.esp32_device_alarms from authenticated;
grant delete on table public.esp32_device_alarms to authenticated;
grant insert (owner_id, device_id, label, time, days, enabled) on public.esp32_device_alarms to authenticated;
grant update (label, time, days, enabled) on public.esp32_device_alarms to authenticated;
