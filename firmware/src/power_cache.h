#pragma once
// Deep sleep loses RAM. Keep alarm identity/settings intact; bound task/calendar summaries.
String powerCacheRead() {
  size_t size=prefs.getBytesLength("pwr-model");if(!size || size>8192)return "";
  char* bytes=static_cast<char*>(malloc(size+1));if(!bytes)return "";
  size_t read=prefs.getBytes("pwr-model",bytes,size);bytes[size]=0;String out=read==size?String(bytes):String();free(bytes);return out;
}
bool powerCacheSave() {
  auto root=cJSON_CreateObject();cJSON_AddNumberToObject(root,"cached_at",time(nullptr));cJSON_AddBoolToObject(root,"calendar_connected",calendarConnected);
  auto settings=cJSON_AddObjectToObject(root,"settings");cJSON_AddStringToObject(settings,"name",screenModel.name.c_str());cJSON_AddNumberToObject(settings,"threshold",screenModel.threshold);cJSON_AddNumberToObject(settings,"idle",screenModel.idleSeconds);cJSON_AddBoolToObject(settings,"autoVoice",screenModel.autoVoice);cJSON_AddBoolToObject(settings,"speak_responses",screenModel.speakResponses);cJSON_AddBoolToObject(settings,"calendarRest",screenModel.calendarRest);
  auto alarms=cJSON_AddArrayToObject(root,"alarms");for(unsigned i=0;i<screenModel.alarmCount;++i){auto& a=screenModel.alarms[i];auto item=cJSON_CreateObject();cJSON_AddItemToArray(alarms,item);cJSON_AddStringToObject(item,"id",a.id.c_str());cJSON_AddStringToObject(item,"label",a.label.c_str());cJSON_AddStringToObject(item,"time",a.time.c_str());cJSON_AddStringToObject(item,"days",a.days.c_str());cJSON_AddStringToObject(item,"due_date",a.dueDate.c_str());cJSON_AddBoolToObject(item,"enabled",a.enabled);}
  auto tasks=cJSON_AddArrayToObject(root,"tasks");for(unsigned i=0;i<screenModel.taskCount;++i){auto& t=screenModel.tasks[i];auto item=cJSON_CreateObject();cJSON_AddItemToArray(tasks,item);cJSON_AddStringToObject(item,"id",t.id.c_str());cJSON_AddStringToObject(item,"title",t.title.c_str());cJSON_AddStringToObject(item,"date",t.date.c_str());cJSON_AddBoolToObject(item,"done",t.done);}
  auto events=cJSON_AddArrayToObject(root,"events");for(unsigned i=0;i<screenModel.eventCount;++i){auto& e=screenModel.events[i];auto item=cJSON_CreateObject();cJSON_AddItemToArray(events,item);cJSON_AddStringToObject(item,"title",e.title.c_str());cJSON_AddStringToObject(item,"date",e.date.c_str());cJSON_AddStringToObject(item,"time",e.time.c_str());String end=e.endDate;if(!e.allDay && e.endTime.length())end+="T"+e.endTime+":00-03:00";cJSON_AddStringToObject(item,"end",end.c_str());cJSON_AddBoolToObject(item,"all_day",e.allDay);if(e.allDay)cJSON_AddStringToObject(item,"start",e.date.c_str());}
  char* encoded=cJSON_PrintUnformatted(root);while(encoded && strlen(encoded)>8192 && (cJSON_GetArraySize(events) || cJSON_GetArraySize(tasks))){free(encoded);if(cJSON_GetArraySize(events))cJSON_DeleteItemFromArray(events,cJSON_GetArraySize(events)-1);else cJSON_DeleteItemFromArray(tasks,cJSON_GetArraySize(tasks)-1);encoded=cJSON_PrintUnformatted(root);}
  bool ok=encoded && strlen(encoded)<=8192;
  if(ok){String previous=powerCacheRead();auto old=cJSON_Parse(previous.c_str());cJSON_DeleteItemFromObjectCaseSensitive(old,"cached_at");cJSON_DeleteItemFromObjectCaseSensitive(root,"cached_at");char* before=cJSON_PrintUnformatted(old);char* after=cJSON_PrintUnformatted(root);bool unchanged=before && after && strcmp(before,after)==0;free(before);free(after);cJSON_Delete(old);
    if(!unchanged){nvs_handle_t h;esp_err_t err=nvs_open("esp32-mic",NVS_READWRITE,&h);if(err==ESP_OK){err=nvs_set_blob(h,"pwr-model",encoded,strlen(encoded));if(err==ESP_OK)err=nvs_commit(h);nvs_close(h);}ok=err==ESP_OK;if(!ok)Serial.printf("power cache failed: %d; deep sleep cancelled\n",err);}
  }
  if(!ok)Serial.println("power cannot persist alarms/settings; remaining awake");free(encoded);cJSON_Delete(root);return ok;
}
