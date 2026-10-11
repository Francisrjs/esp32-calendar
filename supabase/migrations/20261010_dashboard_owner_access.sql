-- Dashboard-owned data. Existing rows are left in place and keep their owner.

alter table public.esp32_device_settings
  add column if not exists display_name text not null default 'Compañero'
    check (length(trim(display_name)) between 1 and 32),
  add column if not exists voice_threshold smallint not null default 500
    check (voice_threshold in (500, 800)),
  add column if not exists auto_voice boolean not null default true,
  add column if not exists idle_seconds smallint not null default 30
    check (idle_seconds in (15, 30, 60, 120)),
  add column if not exists calendar_rest boolean not null default true;

grant select, delete on public.esp32_tasks to authenticated;
grant insert (owner_id, title, due_date, status, timezone) on public.esp32_tasks to authenticated;
grant update (title, notes, status, priority, due_date, due_at, remind_at, timezone, completed_at) on public.esp32_tasks to authenticated;
create policy esp32_tasks_owner_insert on public.esp32_tasks
  for insert to authenticated with check ((select auth.uid()) = owner_id);
create policy esp32_tasks_owner_update on public.esp32_tasks
  for update to authenticated using ((select auth.uid()) = owner_id)
  with check ((select auth.uid()) = owner_id);
create policy esp32_tasks_owner_delete on public.esp32_tasks
  for delete to authenticated using ((select auth.uid()) = owner_id);

grant update (display_name, voice_threshold, auto_voice, idle_seconds, calendar_rest) on public.esp32_device_settings to authenticated;
create policy esp32_device_settings_owner_update on public.esp32_device_settings
  for update to authenticated using ((select auth.uid()) = owner_id)
  with check ((select auth.uid()) = owner_id);

create table public.esp32_device_alarms (
  id uuid primary key default gen_random_uuid(),
  owner_id uuid not null references auth.users(id) on delete cascade,
  device_id uuid not null,
  label text not null check (length(trim(label)) between 1 and 50),
  time time without time zone not null,
  days text not null check (days in ('Todos los días', 'Lun a vie', 'Fin de semana', 'Una vez')),
  enabled boolean not null default true,
  revision bigint not null default 1 check (revision > 0),
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  unique (id, owner_id),
  foreign key (device_id, owner_id) references public.esp32_devices(id, owner_id) on delete cascade
);

alter table public.esp32_device_alarms enable row level security;
revoke all on table public.esp32_device_alarms from public, anon, authenticated;
grant select, delete on table public.esp32_device_alarms to authenticated;
grant insert (owner_id, device_id, label, time, days, enabled) on public.esp32_device_alarms to authenticated;
grant update (label, time, days, enabled) on public.esp32_device_alarms to authenticated;
grant all on table public.esp32_device_alarms to service_role;
create policy esp32_device_alarms_owner_select on public.esp32_device_alarms
  for select to authenticated using ((select auth.uid()) = owner_id);
create policy esp32_device_alarms_owner_insert on public.esp32_device_alarms
  for insert to authenticated with check ((select auth.uid()) = owner_id);
create policy esp32_device_alarms_owner_update on public.esp32_device_alarms
  for update to authenticated using ((select auth.uid()) = owner_id)
  with check ((select auth.uid()) = owner_id);
create policy esp32_device_alarms_owner_delete on public.esp32_device_alarms
  for delete to authenticated using ((select auth.uid()) = owner_id);
create index esp32_device_alarms_owner_device_time_idx
  on public.esp32_device_alarms(owner_id, device_id, time);

create function public.esp32_touch_alarm_updated_at()
returns trigger language plpgsql security invoker set search_path = pg_catalog
as $$
begin
  new.updated_at = now();
  new.revision = old.revision + 1;
  return new;
end;
$$;
revoke all on function public.esp32_touch_alarm_updated_at() from public, anon, authenticated;
create trigger esp32_device_alarms_updated_at before update on public.esp32_device_alarms
  for each row execute function public.esp32_touch_alarm_updated_at();
