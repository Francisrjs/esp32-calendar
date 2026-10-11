-- Additive server-only tables for device pairing and Google Calendar OAuth.
-- Existing Pomodoro tables are intentionally untouched.

alter table public.esp32_devices
  add column token_hash text unique,
  add column revoked_at timestamptz;

create table public.esp32_device_pairings (
  id uuid primary key default gen_random_uuid(),
  owner_id uuid not null references auth.users(id) on delete cascade,
  code_hash text not null unique,
  device_name text not null default 'ESP32-S3-RLCD-4.2',
  expires_at timestamptz not null,
  consumed_at timestamptz,
  created_at timestamptz not null default now(),
  check (expires_at > created_at)
);

create index esp32_device_pairings_owner_idx
  on public.esp32_device_pairings(owner_id, created_at desc);

create table public.esp32_google_oauth_states (
  state_hash text primary key,
  owner_id uuid not null references auth.users(id) on delete cascade,
  calendar_id text not null,
  expires_at timestamptz not null,
  consumed_at timestamptz,
  created_at timestamptz not null default now()
);

create table public.esp32_google_accounts (
  owner_id uuid primary key references auth.users(id) on delete cascade,
  calendar_id text not null,
  refresh_token text not null,
  access_token text,
  access_token_expires_at timestamptz,
  granted_scope text not null,
  connected_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);

alter table public.esp32_device_pairings enable row level security;
revoke all on table public.esp32_device_pairings from public, anon, authenticated;
grant all on table public.esp32_device_pairings to service_role;

alter table public.esp32_google_oauth_states enable row level security;
revoke all on table public.esp32_google_oauth_states from public, anon, authenticated;
grant all on table public.esp32_google_oauth_states to service_role;

alter table public.esp32_google_accounts enable row level security;
revoke all on table public.esp32_google_accounts from public, anon, authenticated;
grant all on table public.esp32_google_accounts to service_role;

create trigger esp32_google_accounts_updated_at
  before update on public.esp32_google_accounts
  for each row execute function public.esp32_touch_updated_at();

