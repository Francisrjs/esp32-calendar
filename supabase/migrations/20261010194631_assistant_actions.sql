-- Service-only request ledger and expiring per-device conversation context.
create table public.esp32_action_requests (
  request_id uuid primary key, device_id uuid not null, owner_id uuid not null,
  conversation_id uuid not null, input_hash text not null,
  status text not null default 'running' check (status in ('running','succeeded','failed','unsupported','needs_clarification','needs_confirmation')),
  intent jsonb, result jsonb, created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  foreign key (device_id,owner_id) references public.esp32_devices(id,owner_id) on delete cascade
);
create unique index esp32_action_one_running on public.esp32_action_requests(device_id) where status='running';
create index esp32_action_owner_created on public.esp32_action_requests(owner_id,created_at);
create table public.esp32_action_context (
  device_id uuid primary key, owner_id uuid not null, conversation_id uuid not null,
  pending jsonb not null, version uuid not null, expires_at timestamptz not null,
  foreign key (device_id,owner_id) references public.esp32_devices(id,owner_id) on delete cascade
);
alter table public.esp32_action_requests enable row level security;
alter table public.esp32_action_context enable row level security;
revoke all on public.esp32_action_requests,public.esp32_action_context from public,anon,authenticated;
grant all on public.esp32_action_requests,public.esp32_action_context to service_role;
-- New alarm IDs are deterministic request IDs, and due_date persists one-shot schedules.
alter table public.esp32_device_alarms add column due_date date;
