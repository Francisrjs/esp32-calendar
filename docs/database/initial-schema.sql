-- ESP32 initial schema; target: pomodoro (jlravhhuzpodggjjspwk).
-- No sample tasks, users, credentials or external integrations are inserted.
-- Apply only after confirming the project's existing schema.
create table public.esp32_projects (
 id uuid primary key default gen_random_uuid(),
 owner_id uuid not null references auth.users(id) on delete cascade,
 name text not null check (length(trim(name)) between 1 and 120),
 status text not null default 'active' check (status in ('active','archived')),
 revision bigint not null default 1 check (revision > 0),
 created_at timestamptz not null default now(),
 updated_at timestamptz not null default now(),
 unique (id, owner_id)
);
create unique index esp32_projects_active_name_idx
 on public.esp32_projects(owner_id, lower(trim(name))) where status = 'active';

create table public.esp32_devices (
 id uuid primary key default gen_random_uuid(),
 owner_id uuid not null references auth.users(id) on delete cascade,
 name text not null check (length(trim(name)) between 1 and 120),
 status text not null default 'pending' check (status in ('pending','active','revoked')),
 last_seen_at timestamptz,
 created_at timestamptz not null default now(),
 updated_at timestamptz not null default now(),
 unique (id, owner_id)
);

create table public.esp32_device_settings (
 device_id uuid primary key,
 owner_id uuid not null references auth.users(id) on delete cascade,
 language text not null default 'es-AR' check (language = 'es-AR'),
 activation_method text not null default 'button'
  check (activation_method in ('button','double_clap')),
 timezone text not null default 'America/Argentina/Buenos_Aires',
 conversation_mode text not null default 'economy' check (conversation_mode in ('economy','fluid')),
 fluid_enabled boolean not null default false,
 max_fluid_session_seconds integer not null default 180
  check (max_fluid_session_seconds between 15 and 600),
 max_utterance_seconds integer not null default 30
  check (max_utterance_seconds between 5 and 60),
 daily_budget_usd numeric(10,4) check (daily_budget_usd > 0),
 monthly_budget_usd numeric(10,4) not null default 4.0000
  check (monthly_budget_usd > 0 and monthly_budget_usd < 5),
 speak_responses boolean not null default true,
 revision bigint not null default 1 check (revision > 0),
 created_at timestamptz not null default now(),
 updated_at timestamptz not null default now(),
 foreign key (device_id, owner_id) references public.esp32_devices(id, owner_id) on delete cascade,
 check (conversation_mode <> 'fluid' or fluid_enabled)
);

create table public.esp32_tasks (
 id uuid primary key default gen_random_uuid(),
 owner_id uuid not null references auth.users(id) on delete cascade,
 project_id uuid,
 title text not null check (length(trim(title)) between 1 and 300),
 notes text not null default '' check (length(notes) <= 8000),
 status text not null default 'pending' check (status in ('pending','completed')),
 priority smallint not null default 0 check (priority between 0 and 3),
 due_date date,
 due_at timestamptz,
 remind_at timestamptz,
 timezone text not null default 'America/Argentina/Buenos_Aires',
 completed_at timestamptz,
 calendar_id text,
 calendar_event_id text,
 revision bigint not null default 1 check (revision > 0),
 created_at timestamptz not null default now(),
 updated_at timestamptz not null default now(),
 unique (id, owner_id),
 foreign key (project_id, owner_id) references public.esp32_projects(id, owner_id),
 check (due_date is null or due_at is null),
 check ((status = 'completed') = (completed_at is not null)),
 check (calendar_event_id is null or calendar_id is not null)
);
create index esp32_tasks_pending_due_idx
 on public.esp32_tasks(owner_id, due_at) where status = 'pending';
create index esp32_tasks_project_idx on public.esp32_tasks(project_id, owner_id);
create index esp32_tasks_remind_idx on public.esp32_tasks(remind_at)
 where status = 'pending' and remind_at is not null;

create table public.esp32_reminders (
 id uuid primary key default gen_random_uuid(),
 owner_id uuid not null references auth.users(id) on delete cascade,
 task_id uuid not null,
 device_id uuid,
 task_revision bigint not null check (task_revision > 0),
 scheduled_at timestamptz not null,
 expires_at timestamptz not null,
 status text not null default 'pending'
  check (status in ('pending','processing','delivered','cancelled','expired','failed')),
 attempts integer not null default 0 check (attempts >= 0),
 delivered_at timestamptz,
 last_error text,
 created_at timestamptz not null default now(),
 updated_at timestamptz not null default now(),
 foreign key (task_id, owner_id) references public.esp32_tasks(id, owner_id) on delete cascade,
 foreign key (device_id, owner_id) references public.esp32_devices(id, owner_id),
 unique (task_id, task_revision),
 check (expires_at >= scheduled_at),
 check ((status = 'delivered') = (delivered_at is not null))
);
create index esp32_reminders_due_idx on public.esp32_reminders(scheduled_at)
 where status = 'pending';
create index esp32_reminders_device_idx on public.esp32_reminders(device_id, owner_id);

create table public.esp32_commands (
 id uuid primary key default gen_random_uuid(),
 owner_id uuid not null references auth.users(id) on delete cascade,
 device_id uuid not null,
 request_id uuid not null,
 action text not null check (length(trim(action)) between 1 and 100),
 arguments jsonb not null default '{}'::jsonb check (jsonb_typeof(arguments) = 'object'),
 result jsonb,
 status text not null default 'pending'
  check (status in ('pending','processing','succeeded','failed','cancelled','expired')),
 expected_revision bigint check (expected_revision > 0),
 expires_at timestamptz not null,
 completed_at timestamptz,
 created_at timestamptz not null default now(),
 updated_at timestamptz not null default now(),
 foreign key (device_id, owner_id) references public.esp32_devices(id, owner_id),
 unique (owner_id, request_id),
 unique (id, owner_id),
 check (expires_at >= created_at)
);
create index esp32_commands_device_status_idx
 on public.esp32_commands(device_id, owner_id, status);

create table public.esp32_ai_usage (
 id uuid primary key default gen_random_uuid(),
 owner_id uuid not null references auth.users(id) on delete cascade,
 device_id uuid not null,
 request_id uuid not null,
 provider text not null,
 model text not null,
 stage text not null check (stage in ('transcription','interpretation','speech','fluid')),
 audio_seconds numeric(12,3) check (audio_seconds >= 0),
 input_tokens bigint check (input_tokens >= 0),
 output_tokens bigint check (output_tokens >= 0),
 estimated_cost_usd numeric(14,6) check (estimated_cost_usd >= 0),
 created_at timestamptz not null default now(),
 foreign key (device_id, owner_id) references public.esp32_devices(id, owner_id)
);
create index esp32_ai_usage_request_idx on public.esp32_ai_usage(owner_id, request_id);
create index esp32_ai_usage_budget_idx on public.esp32_ai_usage(owner_id, created_at);

create function public.esp32_touch_updated_at()
returns trigger language plpgsql security invoker set search_path = pg_catalog
as $$
begin
 new.updated_at = now();
 if tg_table_name in ('esp32_projects','esp32_tasks','esp32_device_settings') then
  new.revision = old.revision + 1;
 end if;
 return new;
end;
$$;
revoke all on function public.esp32_touch_updated_at() from public, anon, authenticated;

alter table public.esp32_projects enable row level security;
revoke all on table public.esp32_projects from public, anon, authenticated;
grant select on table public.esp32_projects to authenticated;
grant all on table public.esp32_projects to service_role;
create policy esp32_projects_owner_read on public.esp32_projects
 for select to authenticated using ((select auth.uid()) = owner_id);
create index esp32_projects_owner_idx on public.esp32_projects(owner_id);
create trigger esp32_projects_updated_at before update on public.esp32_projects
 for each row execute function public.esp32_touch_updated_at();

alter table public.esp32_devices enable row level security;
revoke all on table public.esp32_devices from public, anon, authenticated;
grant select on table public.esp32_devices to authenticated;
grant all on table public.esp32_devices to service_role;
create policy esp32_devices_owner_read on public.esp32_devices
 for select to authenticated using ((select auth.uid()) = owner_id);
create index esp32_devices_owner_idx on public.esp32_devices(owner_id);
create trigger esp32_devices_updated_at before update on public.esp32_devices
 for each row execute function public.esp32_touch_updated_at();

alter table public.esp32_device_settings enable row level security;
revoke all on table public.esp32_device_settings from public, anon, authenticated;
grant select on table public.esp32_device_settings to authenticated;
grant all on table public.esp32_device_settings to service_role;
create policy esp32_device_settings_owner_read on public.esp32_device_settings
 for select to authenticated using ((select auth.uid()) = owner_id);
create index esp32_device_settings_owner_idx on public.esp32_device_settings(owner_id);
create trigger esp32_device_settings_updated_at before update on public.esp32_device_settings
 for each row execute function public.esp32_touch_updated_at();

alter table public.esp32_tasks enable row level security;
revoke all on table public.esp32_tasks from public, anon, authenticated;
grant select on table public.esp32_tasks to authenticated;
grant all on table public.esp32_tasks to service_role;
create policy esp32_tasks_owner_read on public.esp32_tasks
 for select to authenticated using ((select auth.uid()) = owner_id);
create index esp32_tasks_owner_idx on public.esp32_tasks(owner_id);
create trigger esp32_tasks_updated_at before update on public.esp32_tasks
 for each row execute function public.esp32_touch_updated_at();

alter table public.esp32_reminders enable row level security;
revoke all on table public.esp32_reminders from public, anon, authenticated;
grant select on table public.esp32_reminders to authenticated;
grant all on table public.esp32_reminders to service_role;
create policy esp32_reminders_owner_read on public.esp32_reminders
 for select to authenticated using ((select auth.uid()) = owner_id);
create index esp32_reminders_owner_idx on public.esp32_reminders(owner_id);
create trigger esp32_reminders_updated_at before update on public.esp32_reminders
 for each row execute function public.esp32_touch_updated_at();

alter table public.esp32_commands enable row level security;
revoke all on table public.esp32_commands from public, anon, authenticated;
grant select on table public.esp32_commands to authenticated;
grant all on table public.esp32_commands to service_role;
create policy esp32_commands_owner_read on public.esp32_commands
 for select to authenticated using ((select auth.uid()) = owner_id);
create index esp32_commands_owner_idx on public.esp32_commands(owner_id);
create trigger esp32_commands_updated_at before update on public.esp32_commands
 for each row execute function public.esp32_touch_updated_at();

alter table public.esp32_ai_usage enable row level security;
revoke all on table public.esp32_ai_usage from public, anon, authenticated;
grant select on table public.esp32_ai_usage to authenticated;
grant all on table public.esp32_ai_usage to service_role;
create policy esp32_ai_usage_owner_read on public.esp32_ai_usage
 for select to authenticated using ((select auth.uid()) = owner_id);
create index esp32_ai_usage_owner_idx on public.esp32_ai_usage(owner_id);

