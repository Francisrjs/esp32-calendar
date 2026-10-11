-- Service-only atomic reservation: no more than 60 clips/day or 1000/month/owner.
-- Twelve-second clips cap submitted audio at 200 minutes/month.
create table public.esp32_voice_quota (
  owner_id uuid not null references auth.users(id) on delete cascade,
  period date not null,
  clips integer not null default 0 check (clips between 0 and 1000),
  primary key(owner_id, period)
);
alter table public.esp32_voice_quota enable row level security;
revoke all on public.esp32_voice_quota from anon, authenticated;
create function public.esp32_reserve_voice(p_owner uuid) returns boolean
language plpgsql security definer set search_path = '' as $$
declare daily integer; monthly integer;
begin
  perform pg_advisory_xact_lock(hashtextextended(p_owner::text, 42));
  select coalesce(sum(clips),0) into monthly from public.esp32_voice_quota
    where owner_id=p_owner and period>=date_trunc('month',now() at time zone 'UTC')::date;
  select clips into daily from public.esp32_voice_quota
    where owner_id=p_owner and period=(now() at time zone 'UTC')::date;
  if coalesce(daily,0)>=60 or monthly>=1000 then return false; end if;
  insert into public.esp32_voice_quota(owner_id,period,clips)
    values(p_owner,(now() at time zone 'UTC')::date,1)
    on conflict(owner_id,period) do update set clips=esp32_voice_quota.clips+1;
  return true;
end;
$$;
revoke all on function public.esp32_reserve_voice(uuid) from public, anon, authenticated;
grant execute on function public.esp32_reserve_voice(uuid) to service_role;
