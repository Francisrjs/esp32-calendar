#include "ui_model.h"
#include <cJSON.h>
#include <algorithm>
#include "date_rules.h"
namespace {
cJSON* field(cJSON* o, const char* key) { return cJSON_GetObjectItemCaseSensitive(o, key); }
String str(cJSON* o, const char* key, size_t limit = 300) {
  auto v = field(o, key);
  return cJSON_IsString(v) ? String(v->valuestring).substring(0, limit) : String();
}
bool boolValue(cJSON* o, const char* key, bool fallback) {
  auto v = field(o, key); return cJSON_IsBool(v) ? cJSON_IsTrue(v) : fallback;
}
int number(cJSON* o, const char* key, int fallback) {
  auto v = field(o, key); return cJSON_IsNumber(v) ? v->valueint : fallback;
}
void events(ScreenModel& m, cJSON* list) {
  m.eventCount = 0;
  cJSON* item;
  cJSON_ArrayForEach(item, list) {
    if (m.eventCount == kMaxItems) { m.truncated = true; break; }
    auto& e = m.events[m.eventCount++];
    e.endDate="";e.endTime="";e.allDay=boolValue(item,"all_day",false);
    String endIso=str(item,"end",40);
    if(endIso.length()>=19){int y,mo,d,h,mi,sec;if(sscanf(endIso.c_str(),"%d-%d-%dT%d:%d:%d",&y,&mo,&d,&h,&mi,&sec)==6){time_t epoch=utcSeconds(y,mo,d,h,mi,sec);if(endIso.length()>=25 && (endIso[19]=='+' || endIso[19]=='-')){int oh=0,om=0;sscanf(endIso.c_str()+20,"%d:%d",&oh,&om);epoch-=(endIso[19]=='+'?1:-1)*(oh*3600+om*60);}tm t;localtime_r(&epoch,&t);char day[11],hour[6];strftime(day,11,"%Y-%m-%d",&t);strftime(hour,6,"%H:%M",&t);e.endDate=day;e.endTime=hour;}}else e.endDate=endIso.substring(0,10);
    e.title = str(item, "title"); e.date = str(item, "date", 10); e.time = str(item, "time", 5);
    String iso = str(item, "start", 40);
    if (iso.length()) {
      e.date = iso.substring(0, 10); e.time = "Todo el día";
      if (iso.length() >= 19 && iso[10] == 'T') {
        tm t = {}; int y, mo, d, h, mi, s;
        if (sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", &y,&mo,&d,&h,&mi,&s) == 6) {
          t.tm_year=y-1900; t.tm_mon=mo-1; t.tm_mday=d; t.tm_hour=h; t.tm_min=mi; t.tm_sec=s;
          time_t epoch = utcSeconds(y,mo,d,h,mi,s);
          if (iso.length() >= 25 && (iso[19]=='+' || iso[19]=='-')) {
            int oh=0, om=0; sscanf(iso.c_str()+20,"%d:%d",&oh,&om);
            epoch -= (iso[19]=='+'?1:-1)*(oh*3600+om*60);
          }
          localtime_r(&epoch, &t); char date[11], hour[6];
          strftime(date,sizeof(date),"%Y-%m-%d",&t); strftime(hour,sizeof(hour),"%H:%M",&t);
          e.date=date; e.time=hour;
        }
      }
    }
  }
  std::sort(m.events, m.events+m.eventCount, [](const EventData& a,const EventData& b) {
    return a.date+a.time < b.date+b.time;
  });
}
}
bool importScreenModel(ScreenModel& m, const String& json) {
  cJSON* root = cJSON_Parse(json.c_str());
  if (!cJSON_IsObject(root)) { cJSON_Delete(root); return false; }
  m.truncated = false;m.calendarAvailable=boolValue(root,"calendar_connected",m.calendarAvailable);
  auto settings = field(root,"settings");
  if (cJSON_IsObject(settings)) {
    String name=str(settings,"name",32); if(name.length()) m.name=name;
    int threshold=number(settings,"threshold",m.threshold);
    if(threshold==500 || threshold==800 || threshold==1200) m.threshold=threshold;
    int idle=number(settings,"idle",m.idleSeconds);
    if(idle==15 || idle==30 || idle==60 || idle==120) m.idleSeconds=idle;
    m.autoVoice=boolValue(settings,"autoVoice",m.autoVoice);
    m.speakResponses=boolValue(settings,"speak_responses",m.speakResponses);
    m.calendarRest=boolValue(settings,"calendarRest",m.calendarRest);
  }
  auto list=field(root,"tasks"); cJSON* item;
  if(cJSON_IsArray(list)) {
    m.taskCount=0;
    cJSON_ArrayForEach(item,list) {
      if(m.taskCount==kMaxItems) { m.truncated=true; break; }
      String status=str(item,"status",20); if(status=="cancelled") continue;
      auto& t=m.tasks[m.taskCount++]; t.id=str(item,"id",36); t.title=str(item,"title");
      t.date=str(item,"date",10); if(!t.date.length()) t.date=str(item,"due_date",10);
      t.done=boolValue(item,"done",status=="completed");
    }
    std::stable_sort(m.tasks,m.tasks+m.taskCount,[](const TaskData& a,const TaskData& b){return !a.done && b.done;});
  }
  list=field(root,"alarms");
  if(cJSON_IsArray(list)) {
    m.alarmCount=0;
    cJSON_ArrayForEach(item,list) {
      if(m.alarmCount==kMaxItems) {m.truncated=true;break;}
      auto& a=m.alarms[m.alarmCount++]; a.label=str(item,"label",50); a.time=str(item,"time",5);
      a.id=str(item,"id",36);a.dueDate=str(item,"due_date",10);a.days=str(item,"days",50); a.enabled=boolValue(item,"enabled",false);
    }
  }
  list=field(root,"events"); if(cJSON_IsArray(list)) events(m,list);
  auto calendar=field(field(root,"integrations"),"calendar");
  if(cJSON_IsObject(calendar)) m.showEvents=boolValue(calendar,"showOnDevice",m.showEvents);
  auto music=field(root,"music");
  if(cJSON_IsObject(music)) {
    m.musicAvailable=true; m.musicTitle=str(music,"title",60); m.musicArtist=str(music,"artist",60);
    m.duration=std::max(0,number(music,"duration",0)); m.position=std::max(0,std::min(m.duration,number(music,"position",0)));
    m.volume=std::max(0,std::min(100,number(music,"volume",0))); m.playing=boolValue(music,"playing",false);
  }
  cJSON_Delete(root); return true;
}
bool importCalendarEvents(ScreenModel& m,const String& json) {
  auto root=cJSON_Parse(json.c_str()); auto list=field(root,"events");
  bool ok=cJSON_IsArray(list); if(ok) events(m,list); cJSON_Delete(root); return ok;
}
