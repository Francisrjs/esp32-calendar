-- Atomic, idempotent server-side operations used by the ESP32 API.

create or replace function public.esp32_claim_pairing(
  p_pair_code_hash text,
  p_device_token_hash text
)
returns table(device_id uuid, owner_id uuid, device_name text)
language plpgsql
security definer
set search_path = pg_catalog, public
as $$
declare
  v_pair public.esp32_device_pairings%rowtype;
  v_device_id uuid;
begin
  if length(p_device_token_hash) <> 64 then
    raise exception 'invalid_device_token_hash';
  end if;

  select * into v_pair
  from public.esp32_device_pairings
  where code_hash = p_pair_code_hash
    and consumed_at is null
    and expires_at > now()
  for update;

  if not found then
    raise exception 'invalid_or_expired_pairing';
  end if;

  update public.esp32_device_pairings
  set consumed_at = now()
  where id = v_pair.id;

  insert into public.esp32_devices(owner_id, name, status, token_hash)
  values (v_pair.owner_id, v_pair.device_name, 'active', p_device_token_hash)
  returning id into v_device_id;

  insert into public.esp32_device_settings(device_id, owner_id)
  values (v_device_id, v_pair.owner_id);

  return query select v_device_id, v_pair.owner_id, v_pair.device_name;
end;
$$;
revoke all on function public.esp32_claim_pairing(text, text) from public, anon, authenticated;
grant execute on function public.esp32_claim_pairing(text, text) to service_role;

create or replace function public.esp32_create_task(
  p_device_id uuid,
  p_request_id uuid,
  p_title text,
  p_project_id uuid default null,
  p_notes text default '',
  p_priority smallint default 0,
  p_due_date date default null,
  p_due_at timestamptz default null,
  p_remind_at timestamptz default null,
  p_timezone text default 'America/Argentina/Buenos_Aires'
)
returns jsonb
language plpgsql
security definer
set search_path = pg_catalog, public
as $$
declare
  v_owner_id uuid;
  v_command_id uuid;
  v_existing public.esp32_commands%rowtype;
  v_task public.esp32_tasks%rowtype;
  v_result jsonb;
begin
  select d.owner_id into v_owner_id
  from public.esp32_devices d
  where d.id = p_device_id and d.status = 'active' and d.revoked_at is null;
  if not found then raise exception 'device_not_active'; end if;
  if p_due_date is not null and p_due_at is not null then
    raise exception 'choose_due_date_or_due_at';
  end if;
  if p_priority not between 0 and 3 then raise exception 'invalid_priority'; end if;

  insert into public.esp32_commands(owner_id, device_id, request_id, action, arguments, expires_at)
  values (v_owner_id, p_device_id, p_request_id, 'create_task',
    jsonb_build_object('title', p_title, 'project_id', p_project_id), now() + interval '1 day')
  on conflict (owner_id, request_id) do nothing
  returning id into v_command_id;

  if v_command_id is null then
    select * into v_existing from public.esp32_commands
    where owner_id = v_owner_id and request_id = p_request_id;
    if v_existing.status = 'succeeded' then return v_existing.result; end if;
    raise exception 'duplicate_request_in_progress';
  end if;

  insert into public.esp32_tasks(owner_id, project_id, title, notes, priority,
    due_date, due_at, remind_at, timezone)
  values (v_owner_id, p_project_id, p_title, coalesce(p_notes, ''), p_priority,
    p_due_date, p_due_at, p_remind_at, coalesce(p_timezone, 'America/Argentina/Buenos_Aires'))
  returning * into v_task;

  v_result := jsonb_build_object('task', to_jsonb(v_task));
  update public.esp32_commands
  set status = 'succeeded', result = v_result, completed_at = now()
  where id = v_command_id;
  return v_result;
end;
$$;
revoke all on function public.esp32_create_task(uuid, uuid, text, uuid, text, smallint, date, timestamptz, timestamptz, text)
  from public, anon, authenticated;
grant execute on function public.esp32_create_task(uuid, uuid, text, uuid, text, smallint, date, timestamptz, timestamptz, text)
  to service_role;

create or replace function public.esp32_complete_task(
  p_device_id uuid,
  p_request_id uuid,
  p_task_id uuid
)
returns jsonb
language plpgsql
security definer
set search_path = pg_catalog, public
as $$
declare
  v_owner_id uuid;
  v_command_id uuid;
  v_existing public.esp32_commands%rowtype;
  v_task public.esp32_tasks%rowtype;
  v_result jsonb;
begin
  select d.owner_id into v_owner_id
  from public.esp32_devices d
  where d.id = p_device_id and d.status = 'active' and d.revoked_at is null;
  if not found then raise exception 'device_not_active'; end if;

  insert into public.esp32_commands(owner_id, device_id, request_id, action, arguments, expires_at)
  values (v_owner_id, p_device_id, p_request_id, 'complete_task',
    jsonb_build_object('task_id', p_task_id), now() + interval '1 day')
  on conflict (owner_id, request_id) do nothing
  returning id into v_command_id;

  if v_command_id is null then
    select * into v_existing from public.esp32_commands
    where owner_id = v_owner_id and request_id = p_request_id;
    if v_existing.status = 'succeeded' then return v_existing.result; end if;
    raise exception 'duplicate_request_in_progress';
  end if;

  update public.esp32_tasks
  set status = 'completed', completed_at = coalesce(completed_at, now())
  where id = p_task_id and owner_id = v_owner_id
  returning * into v_task;
  if not found then raise exception 'task_not_found'; end if;

  v_result := jsonb_build_object('task', to_jsonb(v_task));
  update public.esp32_commands
  set status = 'succeeded', result = v_result, completed_at = now()
  where id = v_command_id;
  return v_result;
end;
$$;
revoke all on function public.esp32_complete_task(uuid, uuid, uuid) from public, anon, authenticated;
grant execute on function public.esp32_complete_task(uuid, uuid, uuid) to service_role;

