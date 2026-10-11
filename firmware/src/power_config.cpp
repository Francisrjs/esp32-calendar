#include "power_config.h"
#include <cJSON.h>
#include <math.h>
namespace {
bool number(cJSON* root,const char* key,int low,int high,int& out) {
  auto v=cJSON_GetObjectItemCaseSensitive(root,key);
  if(!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || floor(v->valuedouble)!=v->valuedouble || v->valuedouble<low || v->valuedouble>high)return false;
  out=v->valueint;return true;
}
bool hour(cJSON* root,const char* key,uint16_t& out) {
  auto v=cJSON_GetObjectItemCaseSensitive(root,key);
  if(!cJSON_IsString(v) || strlen(v->valuestring)!=5)return false;
  auto s=v->valuestring;
  if(s[2]!=':' || s[0]<'0' || s[0]>'2' || s[1]<'0' || s[1]>'9' || s[3]<'0' || s[3]>'5' || s[4]<'0' || s[4]>'9')return false;
  int h=(s[0]-'0')*10+s[1]-'0';if(h>23)return false;
  out=h*60+(s[3]-'0')*10+s[4]-'0';return true;
}
}
bool parsePowerConfig(const String& json,PowerConfig& output,String& error) {
  if(json.length()>4096){error="maximum 4096 bytes";return false;}
  auto root=cJSON_ParseWithOpts(json.c_str(),nullptr,true);PowerConfig c;int n=0;bool ok=cJSON_IsObject(root);
  ok=ok && number(root,"version",1,1,n) && number(root,"revision",0,2147483647,n);c.revision=n;
  auto profile=cJSON_GetObjectItemCaseSensitive(root,"profile");
  if(cJSON_IsString(profile)) {String p=profile->valuestring;c.profile=p=="available"?0:p=="balanced"?1:p=="maximum"?2:255;}else c.profile=255;
  c.micOff=c.profile!=0;c.intervalWifi=c.profile!=0;c.sleepMode=c.profile;
  auto mic=cJSON_GetObjectItemCaseSensitive(root,"mic_idle");if(mic){ok=ok && cJSON_IsString(mic) && (String(mic->valuestring)=="off" || String(mic->valuestring)=="continuous");if(cJSON_IsString(mic))c.micOff=String(mic->valuestring)=="off";}
  auto wifi=cJSON_GetObjectItemCaseSensitive(root,"wifi_mode");if(wifi){ok=ok && cJSON_IsString(wifi) && (String(wifi->valuestring)=="interval" || String(wifi->valuestring)=="connected");if(cJSON_IsString(wifi))c.intervalWifi=String(wifi->valuestring)=="interval";}
  auto sleep=cJSON_GetObjectItemCaseSensitive(root,"sleep_mode");if(sleep){String v=cJSON_IsString(sleep)?sleep->valuestring:"";c.sleepMode=v=="none"?0:v=="light"?1:v=="deep"?2:255;ok=ok && c.sleepMode<3;}
  ok=ok && (c.sleepMode==0 || (c.micOff && c.intervalWifi));
  ok=ok && c.profile<3 && number(root,"idle_seconds",30,300,n);c.idleSeconds=n;
  ok=ok && (n==30 || n==60 || n==120 || n==300) && number(root,"wifi_interval_minutes",5,60,n);c.wifiMinutes=n;
  ok=ok && (n==5 || n==15 || n==30 || n==60) && number(root,"absence_check_minutes",0,60,n);c.absenceCheckMinutes=n;
  ok=ok && (n==0 || n==30 || n==60);
  auto alarm=cJSON_GetObjectItemCaseSensitive(root,"wake_alarm");ok=ok && cJSON_IsBool(alarm);c.wakeAlarm=cJSON_IsTrue(alarm);
  auto tz=cJSON_GetObjectItemCaseSensitive(root,"timezone");ok=ok && cJSON_IsString(tz) && String(tz->valuestring)=="America/Argentina/Buenos_Aires";
  auto rules=cJSON_GetObjectItemCaseSensitive(root,"absence_rules");ok=ok && cJSON_IsArray(rules) && cJSON_GetArraySize(rules)<=8;
  if(ok) {c.count=cJSON_GetArraySize(rules);for(int i=0;i<c.count && ok;++i){auto r=cJSON_GetArrayItem(rules,i);auto on=cJSON_GetObjectItemCaseSensitive(r,"enabled");auto days=cJSON_GetObjectItemCaseSensitive(r,"days");ok=cJSON_IsBool(on) && cJSON_IsArray(days) && cJSON_GetArraySize(days)>0 && cJSON_GetArraySize(days)<=7;c.rules[i].enabled=cJSON_IsTrue(on);for(int j=0;ok && j<cJSON_GetArraySize(days);++j){auto d=cJSON_GetArrayItem(days,j);ok=cJSON_IsNumber(d) && d->valuedouble==d->valueint && d->valueint>=1 && d->valueint<=7;if(ok)c.rules[i].days|=1<<(d->valueint-1);}ok=ok && hour(r,"start",c.rules[i].start) && hour(r,"end",c.rules[i].end) && validAbsence(c.rules[i]);}}
  cJSON_Delete(root);if(!ok){error="invalid power schema/options/timezone/schedule";return false;}output=c;return true;
}
String encodePowerConfig(const PowerConfig& c) {
  auto root=cJSON_CreateObject();cJSON_AddNumberToObject(root,"version",1);cJSON_AddNumberToObject(root,"revision",c.revision);
  cJSON_AddStringToObject(root,"profile",c.profile==0?"available":c.profile==1?"balanced":"maximum");
  cJSON_AddStringToObject(root,"mic_idle",c.micOff?"off":"continuous");cJSON_AddStringToObject(root,"wifi_mode",c.intervalWifi?"interval":"connected");cJSON_AddStringToObject(root,"sleep_mode",c.sleepMode==0?"none":c.sleepMode==1?"light":"deep");
  cJSON_AddNumberToObject(root,"idle_seconds",c.idleSeconds);cJSON_AddNumberToObject(root,"wifi_interval_minutes",c.wifiMinutes);cJSON_AddNumberToObject(root,"absence_check_minutes",c.absenceCheckMinutes);cJSON_AddBoolToObject(root,"wake_alarm",c.wakeAlarm);cJSON_AddStringToObject(root,"timezone","America/Argentina/Buenos_Aires");
  auto rules=cJSON_AddArrayToObject(root,"absence_rules");for(int i=0;i<c.count;++i){auto r=cJSON_CreateObject();cJSON_AddItemToArray(rules,r);cJSON_AddBoolToObject(r,"enabled",c.rules[i].enabled);auto days=cJSON_AddArrayToObject(r,"days");for(int d=0;d<7;++d)if(c.rules[i].days&(1<<d))cJSON_AddItemToArray(days,cJSON_CreateNumber(d+1));char hour[6];snprintf(hour,sizeof(hour),"%02u:%02u",c.rules[i].start/60,c.rules[i].start%60);cJSON_AddStringToObject(r,"start",hour);snprintf(hour,sizeof(hour),"%02u:%02u",c.rules[i].end/60,c.rules[i].end%60);cJSON_AddStringToObject(r,"end",hour);}
  char* encoded=cJSON_PrintUnformatted(root);String out=encoded?encoded:"";free(encoded);cJSON_Delete(root);return out;
}
