alter table public.esp32_device_settings drop constraint esp32_device_settings_voice_threshold_check;
alter table public.esp32_device_settings add constraint esp32_device_settings_voice_threshold_check check (voice_threshold in (500,800,1200));
