// Included by main.cpp: the main loop exclusively owns policy, display, NVS and Wi-Fi.
#pragma once
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <driver/rtc_io.h>
#include <hal/usb_serial_jtag_ll.h>
#include "power_config.h"
#include "power_cache.h"

PowerConfig powerConfig;
RTC_DATA_ATTR time_t powerNextSync=0;
RTC_DATA_ATTR uint32_t powerDeepWakes=0;
RTC_DATA_ATTR uint64_t powerSleepUs=0;
RTC_DATA_ATTR time_t powerSleepStarted=0;
uint32_t powerWindowAt=0,powerRetryAt=0,powerOverrideAt=0,powerSampleAt=0;
uint32_t powerSleepRetryAt=0;
RTC_DATA_ATTR uint32_t powerAudioMs=0,powerRadioMs=0,powerWakeCount=0;
bool powerRadio=false,powerOverride=false,powerConsumeKey=false;
String powerState="active";
bool powerSleep(bool deep,uint32_t seconds,bool fast);

int powerWeekMinute(const tm& t){return ((t.tm_wday+6)%7)*1440+t.tm_hour*60+t.tm_min;}
bool powerAbsent() {
  tm t={};return clockValid && getLocalTime(&t,5) && anyAbsence(powerConfig.rules,powerConfig.count,powerWeekMinute(t));
}
bool powerOverrideActive(){return powerOverride && millis()-powerOverrideAt<600000;}
bool powerAlarmDueNow() {
  if(!clockValid || !powerConfig.wakeAlarm || qaSilent)return false;
  tm t={};if(!getLocalTime(&t,5))return false;char date[11],hour[6];strftime(date,sizeof(date),"%Y-%m-%d",&t);strftime(hour,sizeof(hour),"%H:%M",&t);
  for(unsigned i=0;i<screenModel.alarmCount;++i){auto& a=screenModel.alarms[i];if(a.enabled && a.time==hour && alarmDue(a.days.c_str(),a.dueDate.c_str(),date,t.tm_wday) && prefs.getString(("alarm-"+String(i)).c_str(),"")!=a.id+String(date)+" "+hour)return true;}
  return false;
}
time_t powerAlarmDeadline(time_t now) {
  if(!clockValid || !powerConfig.wakeAlarm || qaSilent)return 0;
  time_t best=0;tm local={};localtime_r(&now,&local);
  for(unsigned i=0;i<screenModel.alarmCount;++i){auto& a=screenModel.alarms[i];if(!a.enabled)continue;
    int h,m;if(sscanf(a.time.c_str(),"%d:%d",&h,&m)!=2 || h<0 || h>23 || m<0 || m>59)continue;
    for(int day=0;day<8;++day){tm candidate=local;candidate.tm_mday+=day;candidate.tm_hour=h;candidate.tm_min=m;candidate.tm_sec=0;candidate.tm_isdst=-1;time_t epoch=mktime(&candidate);char date[11];strftime(date,sizeof(date),"%Y-%m-%d",&candidate);
      if(epoch<=now || !alarmDue(a.days.c_str(),a.dueDate.c_str(),date,candidate.tm_wday))continue;
      if(!best || epoch<best)best=epoch;break;
    }
    if(a.days=="Una vez" && a.dueDate.length()==10){int y,mo,d;if(sscanf(a.dueDate.c_str(),"%d-%d-%d",&y,&mo,&d)==3){tm c={};c.tm_year=y-1900;c.tm_mon=mo-1;c.tm_mday=d;c.tm_hour=h;c.tm_min=m;c.tm_isdst=-1;time_t epoch=mktime(&c);if(epoch>now && (!best || epoch<best))best=epoch;}}
  }return best;
}
void powerNetworkUse() {
  uint32_t now=millis();powerWindowAt=now;
  if(!powerRadio || (WiFi.status()!=WL_CONNECTED && int32_t(now-powerRetryAt)>=0)) {
    powerRadio=true;powerRetryAt=now+60000;
    WiFi.setAutoReconnect(false);connectWifi();esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  }
}
void powerInteractive() {
  powerOverride=true;powerOverrideAt=millis();screenState.lastActivity=millis();
  voiceSetPower(true);powerNetworkUse();
}
bool powerAlarmsAllowed(){return !powerAbsent() || powerOverrideActive() || powerConfig.wakeAlarm;}
void powerStatus() {
  Serial.println("power config: "+encodePowerConfig(powerConfig));
  Serial.printf("power state=%s revision_applied=%lu audio=%s radio=%s clock=%s absence=%s next_sync=%lld override=%s wakes=%lu deep_wakes=%lu audio_ms=%lu radio_ms=%lu sleep_us=%llu battery=unmeasured remote_ack=not_configured\n",powerState.c_str(),(unsigned long)powerConfig.revision,voicePowered()?"on":"off",powerRadio?"on":"off",clockValid?"valid":"invalid",powerAbsent()?"yes":"no",(long long)powerNextSync,powerOverrideActive()?"yes":"no",(unsigned long)powerWakeCount,(unsigned long)powerDeepWakes,(unsigned long)powerAudioMs,(unsigned long)powerRadioMs,(unsigned long long)powerSleepUs);
}
bool powerApply(const String& json,bool remote=false) {
  PowerConfig proposed;String error;if(!parsePowerConfig(json,proposed,error)){Serial.println("power rejected: "+error);return false;}
  if(voiceOccupied() || actionPending || networkBusy()){Serial.println("power rejected: busy; retry when idle");return false;}
  if(remote && proposed.revision<=powerConfig.revision){Serial.println("power stale revision ignored");return false;}
  if(!remote){if(powerConfig.revision>=2147483647UL){Serial.println("power revision exhausted");return false;}proposed.revision=powerConfig.revision+1;}
  String encoded=encodePowerConfig(proposed);
  if(encoded.isEmpty() || prefs.putString("power-config",encoded)!=encoded.length()){Serial.println("power persistence failed; previous policy preserved");return false;}
  powerConfig=proposed;powerNextSync=time(nullptr)+powerConfig.wifiMinutes*60;screenState.lastActivity=millis();
  powerOverride=false;Serial.println("power saved and applied locally");powerStatus();return true;
}
bool powerCommand(const String& line) {
  if(line=="power status"){powerStatus();return true;}
  if(line=="power wake"){powerInteractive();currentView=6;screenState.answer="Listo, KEY para hablar";dirty=true;return true;}
  if(line.startsWith("power test ")){char mode[8]={};int seconds=0;char extra=0;if(!qaSilent || sscanf(line.c_str(),"power test %7s %d %c",mode,&seconds,&extra)!=2 || seconds<5 || seconds>60 || (String(mode)!="light" && String(mode)!="deep")){Serial.println("power test requires qa on; light|deep 5..60 seconds");return true;}if(!powerSleep(String(mode)=="deep",seconds,false))Serial.println("power test refused: busy/key/peripheral error");return true;}
  if(line.startsWith("power config ")){powerApply(line.substring(13));return true;}
  if(line.startsWith("power profile ")){String p=line.substring(14);if(p!="available" && p!="balanced" && p!="maximum"){Serial.println("power profile available|balanced|maximum");return true;}PowerConfig c=powerConfig;c.profile=p=="available"?0:p=="balanced"?1:2;c.micOff=c.profile!=0;c.intervalWifi=c.profile!=0;c.sleepMode=c.profile;powerApply(encodePowerConfig(c));return true;}
  return false;
}
void powerRadioOff() {
  if(!powerRadio)return;
  WiFi.setAutoReconnect(false);WiFi.disconnect(false,false);WiFi.mode(WIFI_OFF);powerRadio=false;
  Serial.println("power radio: off; credentials preserved");
}
bool powerSleep(bool deep,uint32_t seconds,bool fast=false) {
  if(!rtc_gpio_is_valid_gpio(GPIO_NUM_18) || digitalRead(kKeyPin)==LOW || networkBusy() || voiceOccupied() || actionPending)return false;
  if(deep && !fast && !powerCacheSave())return false;
  if(!voiceSetPower(false)){Serial.println("power sleep cancelled: audio did not stop");return false;}
  powerState="preparing";powerRadioOff();
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  rtc_gpio_pullup_en(GPIO_NUM_18);rtc_gpio_pulldown_dis(GPIO_NUM_18);
  esp_err_t key=esp_sleep_enable_ext0_wakeup(GPIO_NUM_18,0);
  esp_err_t timer=esp_sleep_enable_timer_wakeup(uint64_t(seconds)*1000000ULL);
  if(key!=ESP_OK || timer!=ESP_OK){rtc_gpio_deinit(GPIO_NUM_18);voiceSetPower(true);powerState="active";Serial.printf("power wake setup failed: key=%d timer=%d\n",key,timer);return false;}
  if(!fast)display.RLCD_Sleep();
  Serial.printf("power sleep: mode=%s seconds=%lu\n",deep?"deep":"light",(unsigned long)seconds);Serial.flush();
  if(deep){powerSleepStarted=time(nullptr);esp_deep_sleep_start();return true;}
  // S3 USB Serial/JTAG can keep D+ high in sleep, leaving a stale Windows COM handle.
  // Detach explicitly and restart the Arduino driver/PHY on return.
  Serial.end();usb_serial_jtag_ll_enable_pad(false);delay(200);
  uint64_t started=esp_timer_get_time();esp_err_t slept=esp_light_sleep_start();powerSleepUs+=esp_timer_get_time()-started;
  usb_serial_jtag_ll_enable_bus_clock(true);usb_serial_jtag_ll_reset_register();usb_serial_jtag_ll_enable_pad(true);Serial.setRxBufferSize(16384);Serial.begin(115200);
  rtc_gpio_deinit(GPIO_NUM_18);pinMode(kKeyPin,INPUT_PULLUP);++powerWakeCount;
  display.RLCD_Init();dirty=true;powerState="awake";
  if(!powerConfig.intervalWifi && !powerAbsent())powerNetworkUse();
  if(slept!=ESP_OK){screenState.lastActivity=millis();voiceSetPower(true);Serial.printf("power light sleep failed: %d\n",slept);return false;}
  if(esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_EXT0){powerConsumeKey=true;powerInteractive();currentView=6;screenState.answer="Listo, KEY para hablar";}
  return true;
}
uint32_t powerSleepSeconds(bool absent) {
  time_t now=time(nullptr),next=now+powerConfig.wifiMinutes*60;tm t={};localtime_r(&now,&t);
  if(absent){int remaining=absenceRemaining(powerConfig.rules,powerConfig.count,powerWeekMinute(t));next=now+remaining*60-t.tm_sec;if(powerConfig.absenceCheckMinutes && powerNextSync>now && powerNextSync<next)next=powerNextSync;}
  else if(powerNextSync>now)next=powerNextSync;
  time_t alarm=powerAlarmDeadline(now);if(alarm && alarm<next)next=alarm;
  if(!absent && powerConfig.sleepMode==1 && next>now+60)next=now+60; // refresh clock and evaluate visual rotation
  return uint32_t(next>now?next-now:1);
}
void powerTick() {
  // Key wake is consumed until release; never allow repeated level-triggered sleeps.
  if(powerConsumeKey)return;
  if(pendingPowerJson.length() && !networkBusy() && !voiceOccupied() && !actionPending){String requested=pendingPowerJson;pendingPowerJson="";powerApply(requested,true);}
  uint32_t now=millis(),elapsed=now-powerSampleAt;powerSampleAt=now;
  if(voicePowered())powerAudioMs+=elapsed;if(powerRadio)powerRadioMs+=elapsed;
  bool scheduled=powerAbsent(),absent=scheduled && !powerOverrideActive();
  bool idle=now-screenState.lastActivity>=uint32_t(powerConfig.idleSeconds)*1000 && !(scheduled && powerOverrideActive());
  bool busy=voiceOccupied() || actionPending || networkBusy() || currentView==5;
  if(!clockValid && !powerRadio && int32_t(now-powerRetryAt)>=0)powerNetworkUse();
  uint16_t interval=absent?powerConfig.absenceCheckMinutes:powerConfig.wifiMinutes;
  if(clockValid && time(nullptr)>=powerNextSync && (!absent || interval)) {
    powerNextSync=time(nullptr)+interval*60;powerNetworkUse();lastSync=0;
  }
  if(powerRadio && WiFi.status()!=WL_CONNECTED && now-powerWindowAt>=30000 && !networkBusy() && !actionPending){powerRadioOff();powerRetryAt=now+60000;}
  if(!powerConfig.intervalWifi && !absent && !powerRadio && int32_t(now-powerRetryAt)>=0)powerNetworkUse();
  if(!busy && (idle || absent) && (powerConfig.micOff || absent))voiceSetPower(false);
  else if(!absent && (!powerConfig.micOff || !idle))voiceSetPower(true);
  if((powerConfig.intervalWifi || absent) && powerRadio && !busy && now-powerWindowAt>=30000 && (idle || absent))powerRadioOff();
  if(!busy && (idle || absent) && (powerConfig.sleepMode || absent) && clockValid && !powerRadio && !keyClicks.raw && !keyClicks.stable && !keyClicks.clicks && serialLine.isEmpty() && int32_t(now-powerSleepRetryAt)>=0) {
    if(!powerSleep(absent || powerConfig.sleepMode==2,powerSleepSeconds(absent)))powerSleepRetryAt=millis()+30000;
    powerSampleAt=millis();
  } else powerState=busy?"conversation":absent?"absence":idle?"idle":"active";
}
void powerBoot() {
  String model=powerCacheRead();if(model.length()){importScreenModel(screenModel,model);calendarConnected=screenModel.calendarAvailable;}
  String saved=prefs.getString("power-config","");String error;if(saved.length() && !parsePowerConfig(saved,powerConfig,error))Serial.println("power saved policy invalid; using available");
  rtc_gpio_deinit(GPIO_NUM_18);pinMode(kKeyPin,INPUT_PULLUP);
  auto cause=esp_sleep_get_wakeup_cause();Serial.printf("power boot: wake_cause=%d\n",int(cause));
  if(cause==ESP_SLEEP_WAKEUP_TIMER || cause==ESP_SLEEP_WAKEUP_EXT0){++powerDeepWakes;if(powerSleepStarted && time(nullptr)>powerSleepStarted)powerSleepUs+=uint64_t(time(nullptr)-powerSleepStarted)*1000000;powerSleepStarted=0;}
  if(cause==ESP_SLEEP_WAKEUP_EXT0){powerConsumeKey=true;powerOverride=true;powerOverrideAt=millis();}
  if(!powerNextSync || cause==ESP_SLEEP_WAKEUP_UNDEFINED)powerNextSync=time(nullptr)+powerConfig.wifiMinutes*60;
  // A timer-only absence wake can return directly to sleep before codecs/radio/display initialization.
  if(cause==ESP_SLEEP_WAKEUP_TIMER && powerAbsent() && !powerOverrideActive() && (powerConfig.absenceCheckMinutes==0 || time(nullptr)<powerNextSync) && actionRequestId.isEmpty()) {
    if(!powerAlarmDueNow())powerSleep(true,powerSleepSeconds(true),true);
  }
  if(powerAbsent() && powerConfig.absenceCheckMinutes)powerNextSync=time(nullptr)+powerConfig.absenceCheckMinutes*60;
  powerSampleAt=millis();
}
