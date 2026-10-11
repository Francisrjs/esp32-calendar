#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Preferences.h>
#include <nvs.h>
#include <time.h>
#include <sys/time.h>
#include <cJSON.h>
#include "display_bsp.h"
#include "ui_renderer.h"
#include "network_worker.h"
#include "input_rules.h"
#include "voice_input.h"
#include "alarm_rules.h"
#include "date_rules.h"
#include <esp_timer.h>
void powerInteractive();
bool powerCommand(const String& line);
bool powerAlarmsAllowed();
String pendingPowerJson;
bool powerCacheSave();
#if __has_include("device_secrets.h")
#include "device_secrets.h"
#endif

// Waveshare ESP32-S3-RLCD-4.2 wiring (manufacturer Arduino example).
DisplayPort display(12, 11, 5, 40, 41, kScreenWidth, kScreenHeight);
constexpr int kI2cSda = 13, kI2cScl = 14, kKeyPin = 18;
constexpr uint8_t kRtcAddress = 0x51, kShtAddress = 0x70;
constexpr uint32_t kSyncPeriodMs = 300000, kSensorPeriodMs = 30000;

Preferences prefs;
String wifiSsid, wifiPassword, deviceToken;
ScreenModel screenModel;
ScreenState screenState;
KeyClicks keyClicks;
uint32_t voiceGeneration=0;
String conversationId, actionRequestId, actionBody, completedActionId;
uint32_t actionGeneration=0;
bool actionPending=false, qaSilent=false, recoveryChecked=false;
uint32_t actionPollAt=0, conversationAt=0, sensorValidAt=0, alarmTick=0;
uint8_t actionPolls=0;
String alarmMinute;
void runAction(const String& text,uint32_t generation);
void checkAlarms(uint32_t now);
bool syncPending=false;
uint32_t lastRender=0, lastUiTick=0;
int lastClockMinute=-1;
uint32_t lastPageTick=0;
bool lastOnline=false, serialOverflow=false;
const char* viewName(uint8_t view) {
  static const char* names[]={"home","alarms","tasks","calendar","music","listening","response","rest","daily","weekly"};
  return view<10?names[view]:"unknown";
}
const char* phaseName(VoicePhase phase) {
  static const char* names[]={"waiting","recording","processing","response"};
  return names[unsigned(phase)];
}
uint8_t loggedView=255;
VoicePhase loggedPhase=VoicePhase::Waiting;
bool dirty=true;
uint8_t currentView = 0;
bool calendarConnected = false, clockValid = false, rtcAvailable = false;
bool haveEnvironment = false;
bool ntpConfigured = false;
float temperatureC = NAN, humidityPct = NAN;
uint32_t lastSync = 0, lastSensorRead = 0, ntpStartedAt = 0;
String serialLine;
bool audioDebug=false;
uint32_t lastAudioDebug=0;
uint32_t lastVoiceMeter=0;
void drawLiveData();
void fetchCalendarEvents();

String twoDigits(int value) { return value < 10 ? "0" + String(value) : String(value); }
String liveTime() {
  if (!clockValid) return "--:--";
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 5)) return "--:--";
  return twoDigits(timeinfo.tm_hour) + ":" + twoDigits(timeinfo.tm_min);
}

bool rtcRead(uint8_t first, uint8_t* out, size_t count) {
  Wire.beginTransmission(kRtcAddress);
  Wire.write(first);
  if (Wire.endTransmission(false) != 0 ||
      Wire.requestFrom(static_cast<int>(kRtcAddress), static_cast<int>(count)) != static_cast<int>(count)) return false;
  for (size_t i = 0; i < count; ++i) out[i] = Wire.read();
  return true;
}
int bcd(uint8_t v) { return (v & 0x0f) + 10 * ((v >> 4) & 0x0f); }
bool rtcSetFromSystem() {
  struct tm t;
  if (!getLocalTime(&t, 10)) return false;
  const uint8_t regs[] = {
    uint8_t((t.tm_sec / 10 << 4) | t.tm_sec % 10),
    uint8_t((t.tm_min / 10 << 4) | t.tm_min % 10),
    uint8_t((t.tm_hour / 10 << 4) | t.tm_hour % 10),
    uint8_t((t.tm_mday / 10 << 4) | t.tm_mday % 10),
    uint8_t(t.tm_wday),
    uint8_t(((t.tm_mon + 1) / 10 << 4) | (t.tm_mon + 1) % 10),
    uint8_t(((t.tm_year + 1900 - 2000) / 10 << 4) | (t.tm_year + 1900 - 2000) % 10),
  };
  Wire.beginTransmission(kRtcAddress); Wire.write(0x04); Wire.write(regs, sizeof(regs));
  return Wire.endTransmission() == 0;
}
bool rtcReadIntoSystem() {
  uint8_t r[7];
  if (!rtcRead(0x04, r, sizeof(r))) return false;
  if (r[0] & 0x80) return false; // OS flag: oscillator stopped.
  struct tm t = {};
  t.tm_sec = bcd(r[0] & 0x7f); t.tm_min = bcd(r[1] & 0x7f);
  t.tm_hour = bcd(r[2] & 0x3f); t.tm_mday = bcd(r[3] & 0x3f);
  t.tm_mon = bcd(r[5] & 0x1f) - 1; t.tm_year = bcd(r[6]) + 100;
  if (t.tm_mday < 1 || t.tm_mday > 31 || t.tm_mon < 0 || t.tm_mon > 11) return false;
  const time_t epoch = mktime(&t);
  if (epoch < 1735689600) return false;
  timeval tv = { epoch, 0 }; settimeofday(&tv, nullptr); return true;
}

bool shtRead() {
  Wire.beginTransmission(kShtAddress); Wire.write(0x35); Wire.write(0x17);
  if (Wire.endTransmission() != 0) return false;
  delay(1);
  Wire.beginTransmission(kShtAddress); Wire.write(0x78); Wire.write(0x66);
  if (Wire.endTransmission() != 0) return false;
  delay(20);
  if (Wire.requestFrom(static_cast<int>(kShtAddress), 6) != 6) return false;
  uint8_t b[6]; for (auto &v : b) v = Wire.read();
  auto crc = [](uint8_t* data, int n) {
    uint8_t c = 0xff;
    for (int i = 0; i < n; ++i) { c ^= data[i]; for (int j = 0; j < 8; ++j) c = (c & 0x80) ? (c << 1) ^ 0x31 : c << 1; }
    return c;
  };
  if (crc(b, 2) != b[2] || crc(b + 3, 2) != b[5]) return false;
  uint16_t tr = (uint16_t(b[0]) << 8) | b[1], hr = (uint16_t(b[3]) << 8) | b[4];
  temperatureC = -45.f + 175.f * tr / 65535.f;
  humidityPct = 100.f * hr / 65535.f;
  haveEnvironment = true;sensorValidAt=millis(); return true;
}

bool submit(NetworkKind kind,const String& path,const String& method="GET",const String& body="") {
  auto req=new NetworkRequest;req->kind=kind;req->path=path;req->method=method;req->body=body;req->token=deviceToken;req->generation=kind==NetworkKind::Action?actionGeneration:voiceGeneration;
  if(networkSubmit(req))return true;
  delete req;Serial.println("Network queue full/unavailable");return false;
}
String jsonString(const String& json,const String& key) {
  cJSON* root=cJSON_Parse(json.c_str());
  cJSON* value=cJSON_GetObjectItemCaseSensitive(root,key.c_str());
  String result=cJSON_IsString(value)?value->valuestring:"";
  cJSON_Delete(root);return result;
}
void cachePut(const char* key,const String& data) {
  // NVS strings must fit in one page. Full Calendar responses can exceed it.
  if(data.length()>3500){Serial.printf("NVS cache skipped: %s (%u bytes); live data remains available\n",key,unsigned(data.length()));return;}
  if(prefs.isKey(key) && prefs.getString(key,"")==data)return;
  nvs_handle_t handle;
  esp_err_t err=nvs_open("esp32-mic",NVS_READWRITE,&handle);
  if(err==ESP_OK){err=nvs_set_str(handle,key,data.c_str());if(err==ESP_OK)err=nvs_commit(handle);nvs_close(handle);}
  // Avoid the framework's unsafe error formatting on a failed Preferences write.
  if(err!=ESP_OK)Serial.printf("NVS cache could not save %s (%u bytes), error=%d\n",key,unsigned(data.length()),int(err));
}
String jsonEscape(const String& value) {
  String out; out.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else out += c;
  }
  return out;
}
String requestId() {
  char out[37];
  snprintf(out, sizeof(out), "%08lx-%04lx-4%03lx-%04lx-%08lx%04lx",
    (unsigned long)esp_random(), (unsigned long)(esp_random() & 0xffff),
    (unsigned long)(esp_random() & 0xfff), (unsigned long)((esp_random() & 0x3fff) | 0x8000),
    (unsigned long)esp_random(),(unsigned long)(esp_random() & 0xffff));
  return String(out);
}
void syncSnapshot() {
  if(syncPending || !deviceToken.length())return;
  lastSync=millis();syncPending=submit(NetworkKind::Snapshot,"snapshot");
}
void fetchCalendarEvents(){
  tm local={}; if(!clockValid || !getLocalTime(&local,5))return;
  if(screenState.agendaDate.length()==10){int y,mo,d;if(sscanf(screenState.agendaDate.c_str(),"%d-%d-%d",&y,&mo,&d)==3){time_t epoch=utcSeconds(y,mo,d,12,0,0);gmtime_r(&epoch,&local);}}
  local.tm_mday-=(local.tm_wday+6)%7;local.tm_hour=0;local.tm_min=local.tm_sec=0;local.tm_isdst=-1;
  time_t start=mktime(&local);char first[40],last[40];strftime(first,sizeof(first),"%Y-%m-%dT00:00:00-03:00",&local);
  time_t end=start+14*86400;localtime_r(&end,&local);strftime(last,sizeof(last),"%Y-%m-%dT00:00:00-03:00",&local);
  submit(NetworkKind::Calendar,"calendar-events","POST",String("{\"time_min\":\"")+first+"\",\"time_max\":\""+last+"\"}");
}
void createCalendarEvent(const String& title,const String& startAt,const String& endAt) {
  submit(NetworkKind::Mutation,"calendar-event-create","POST",String("{\"request_id\":\"")+requestId()+"\",\"title\":\""+jsonEscape(title)+"\",\"start_at\":\""+jsonEscape(startAt)+"\",\"end_at\":\""+jsonEscape(endAt)+"\"}");
}
void createTask(const String& title) {
  submit(NetworkKind::Mutation,"task-create","POST",String("{\"request_id\":\"")+requestId()+"\",\"title\":\""+jsonEscape(title)+"\",\"timezone\":\"America/Argentina/Buenos_Aires\"}");
}
void claimPairing(const String& code){submit(NetworkKind::Pair,"pair-claim","POST",String("{\"pairing_code\":\"")+jsonEscape(code)+"\"}");}
void openListening() {
  if(voiceOccupied() || actionPending){Serial.println("assistant busy; request preserved");return;}
  if(currentView!=5 && currentView!=6)screenState.lastQuery=currentView;
  powerInteractive();
  ++voiceGeneration;currentView=5;screenState.phase=VoicePhase::Waiting;
  screenState.lastActivity=screenState.viewSince=millis();dirty=true;
  voiceAllowAutomatic(true);
  if(!voiceListen()) {screenState.answer="Audio ocupado o no disponible";currentView=6;}
}
void connectWifi() {
  if (wifiSsid.isEmpty()) return;
  WiFi.mode(WIFI_STA); WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  Serial.printf("Wi-Fi connecting to %s\n", wifiSsid.c_str());
}
void speakText(const String& value) {
  voiceHold(true);
  auto req=new NetworkRequest;req->kind=NetworkKind::Speech;req->path="voice-speak";req->method="POST";
  req->token=deviceToken;req->generation=voiceGeneration;
  cJSON* body=cJSON_CreateObject();cJSON_AddStringToObject(body,"text",value.c_str());
  char* encoded=cJSON_PrintUnformatted(body);if(encoded){req->body=encoded;free(encoded);}cJSON_Delete(body);
  if(!clockValid || !deviceToken.length() || !encoded || !networkSubmit(req)){delete req;voiceHold(false);Serial.println("speech request unavailable");}
  else Serial.println("speech requested: artificial voice");
}

void serialCommand(String line) {
  if(powerCommand(line))return;
  if(line=="qa on" || line=="qa off"){qaSilent=line.endsWith("on");prefs.putBool("qa-silent",qaSilent);voiceAllowAutomatic(!qaSilent);Serial.println(qaSilent?"QA silent: audio output and automatic capture disabled":"QA complete: normal mode");return;}
  if(line=="calendar link"){submit(NetworkKind::Diagnostic,"calendar-link","POST","{}");return;}
  if(line=="action status"){if(actionRequestId.length())submit(NetworkKind::Action,"action-status","POST",String("{\"request_id\":\"")+actionRequestId+"\",\"conversation_id\":\""+conversationId+"\"}");return;}
  if(line=="action retry"){if(actionBody.length())submit(NetworkKind::Action,"action-run","POST",actionBody);return;}
  if(line.startsWith("action ")){if(voiceOccupied() || actionPending){Serial.println("assistant busy");return;}qaSilent=true;++voiceGeneration;currentView=5;screenState.phase=VoicePhase::Processing;runAction(line.substring(7),voiceGeneration);dirty=true;return;}
  if(line.startsWith("say ")) {if(qaSilent || voiceOccupied()){Serial.println("audio busy");return;}++voiceGeneration;currentView=6;screenState.answer=line.substring(4);screenState.lastActivity=millis();speakText(screenState.answer);dirty=true;return;}
  if(qaSilent && (line.startsWith("say ") || line=="audio tone")){Serial.println("QA output disabled");return;}
  if(line=="audio tone") {Serial.println(voiceSound()?"audio chime requested":"audio chime unavailable/busy");return;}
  if (line == "audio status") { voiceStatus(); return; }
  if (line == "audio regs") { voiceRegisters(); return; }
  if (line == "audio debug on" || line == "audio debug off") { audioDebug=line.endsWith("on"); return; }
  if (line == "voice status") {
    submit(NetworkKind::Diagnostic,"voice-status"); return;
  }
  if (line == "listen") { openListening(); return; }
  if (line == "clap on" || line == "clap off") { screenModel.autoVoice=line == "clap on"; voiceEnable(screenModel.autoVoice); voiceStatus(); return; }
  if (line == "wake voice" || line == "wake clap") { voiceActivity(line == "wake voice"); voiceStatus(); return; }
  line.trim();
  if(line.startsWith("config ")) {
    String data=line.substring(7);
    if(data.length()<=16000 && importScreenModel(screenModel,data)) {
      cachePut("ui-config",data);voiceConfigure(screenModel.threshold);voiceEnable(screenModel.autoVoice);dirty=true;
      Serial.println("Configuration imported");
    } else Serial.println("Invalid configuration (max 16000 bytes)");
    return;
  }
  if(line.startsWith("threshold ")) {
    int n=line.substring(10).toInt();if(n==500 || n==800 || n==1200){screenModel.threshold=n;voiceConfigure(n);dirty=true;}return;
  }
  if (line.startsWith("wifi ")) {
    const int split = line.indexOf('|', 5);
    if (split < 0) { Serial.println("usage: wifi SSID|PASSWORD"); return; }
    wifiSsid = line.substring(5, split); wifiPassword = line.substring(split + 1);
    prefs.putString("ssid", wifiSsid); prefs.putString("wifi_password", wifiPassword);
    connectWifi(); return;
  }
  if (line.startsWith("pair ")) { claimPairing(line.substring(5)); return; }
  if (line.startsWith("task ")) { createTask(line.substring(5)); return; }
  if (line.startsWith("event ")) {
    const int a = line.indexOf('|', 6), b = a < 0 ? -1 : line.indexOf('|', a + 1);
    if (a < 0 || b < 0) { Serial.println("usage: event TITLE|START_ISO8601|END_ISO8601"); return; }
    createCalendarEvent(line.substring(6, a), line.substring(a + 1, b), line.substring(b + 1)); return;
  }
  if (line == "sync") { syncSnapshot(); return; }
  if (line == "ping") {
    submit(NetworkKind::Diagnostic,"health"); return;
  }
  if (line.startsWith("view ")) {
    const char* names[] = {"home", "alarms", "tasks", "calendar", "music", "listening", "speaking", "rest", "daily", "weekly"};
    for (uint8_t i = 0; i < 10; ++i) {
      if (line.substring(5) == names[i]) { currentView = i; ++voiceGeneration; screenState.viewSince=screenState.lastActivity=millis(); if(i!=5 && i!=6)screenState.lastQuery=i; drawLiveData(); Serial.println("view=" + String(names[i])); return; }
    }
    Serial.println("usage: view home|alarms|tasks|calendar|music|listening|speaking|rest|daily|weekly"); return;
  }
  if (line == "status") {
    Serial.printf("ui: firmware=views-v2 view=%s phase=%s rms=%d threshold=%u autoVoice=%s idle=%us\n",viewName(currentView),phaseName(screenState.phase),voiceLevel(),currentView==6?max(800,int(screenModel.threshold)):screenModel.threshold,screenModel.autoVoice?"on":"off",screenModel.idleSeconds);
    Serial.printf("Wi-Fi=%s, RTC=%s, device=%s, tasks=%u, heap=%u\n",
      WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "offline",
      clockValid ? "valid" : "invalid", deviceToken.length() ? "paired" : "unpaired",
      screenModel.taskCount, ESP.getFreeHeap());
    Serial.printf("Local time=%s, sensor=%s, temperature=%.1fC, humidity=%.1f%%\n",
      liveTime().c_str(), haveEnvironment ? "ready" : "no reading", temperatureC, humidityPct); return;
  }
  if (line == "help") {
    Serial.println("Energy: power status | power profile available/balanced/maximum | power config JSON | power wake");
    Serial.println("Assistant: action TEXT (silent QA) | action status | action retry | calendar link | qa on/off");
    Serial.println("Commands: wifi SSID|PASSWORD | pair CODE | task TITLE | event TITLE|START_ISO8601|END_ISO8601 | sync | ping | status | view home/tasks/calendar | help. KEY 1x=voice; 2x=next; 3x+=previous. config JSON | threshold 500/800/1200 | listen | audio status | voice status.");
    return;
  }
  Serial.println("Unknown command; type help");
}

void drawLiveData() { dirty=true; }
void transcribeVoice() {
  auto req=new NetworkRequest;req->kind=NetworkKind::Voice;req->path="voice-transcribe";req->method="POST";
  req->token=deviceToken;req->audio=voiceWav(req->bytes);req->generation=voiceGeneration;
  if(!req->audio || !clockValid || !deviceToken.length() || !networkSubmit(req)) {
    delete req;voiceRelease();
    if(currentView==5){currentView=6;screenState.answer="Servicio no disponible. Revisá red, hora y vinculación.";dirty=true;}
  }
}
void runAction(const String& text,uint32_t generation) {
  powerInteractive();
  actionGeneration=generation;completedActionId="";
  voiceHold(true);screenState.processingText="Interpretando y ejecutando tu pedido.";
  if(conversationId.length()!=36 || millis()-conversationAt>120000)conversationId=requestId();
  conversationAt=millis();actionRequestId=requestId();actionPending=true;actionPolls=0;
  cJSON* body=cJSON_CreateObject();cJSON_AddStringToObject(body,"text",text.c_str());
  cJSON_AddStringToObject(body,"request_id",actionRequestId.c_str());cJSON_AddStringToObject(body,"conversation_id",conversationId.c_str());
  cJSON_AddBoolToObject(body,"clock_valid",clockValid);
  tm t={};if(clockValid && getLocalTime(&t,5)){char date[30];strftime(date,sizeof(date),"%Y-%m-%d %H:%M",&t);cJSON_AddStringToObject(body,"local_time",date);}
  if(haveEnvironment){cJSON_AddNumberToObject(body,"temperature",temperatureC);cJSON_AddNumberToObject(body,"humidity",humidityPct);cJSON_AddNumberToObject(body,"sensor_age_seconds",(millis()-sensorValidAt)/1000);}
  char* encoded=cJSON_PrintUnformatted(body);actionBody=encoded?encoded:"";free(encoded);cJSON_Delete(body);
  prefs.putString("action-id",actionRequestId);prefs.putString("action-conv",conversationId);prefs.putString("action-body",actionBody);
  Serial.printf("assistant request: id=%s\n",actionRequestId.c_str());
  actionPollAt=millis()+10000;
  if(!actionBody.length() || !submit(NetworkKind::Action,"action-run","POST",actionBody)){actionPending=false;voiceHold(false);screenState.answer="No pude enviar el pedido. Consultá su estado.";currentView=6;}
}
void applyActionResult(NetworkResult* r) {
  bool ok=r->status>=200 && r->status<300;
  String status=ok?jsonString(r->body,"status"):(r->status>=400 && r->status<500?"failed":"running");
  if(qaSilent && !ok)Serial.println("assistant HTTP error: "+r->body);
  String message=ok?jsonString(r->body,"display_text"):"No pude confirmar el cambio. Consultando su estado.";
  if(status=="running" || !status.length()) {
    actionPending=true;voiceHold(true);actionPollAt=millis()+10000;
    if(++actionPolls>=6){actionPending=false;voiceHold(false);message="No pude confirmar el cambio. Usá action status antes de repetir.";}
    if(currentView==5){if(!actionPending){currentView=6;screenState.phase=VoicePhase::Response;screenState.answer=message;screenState.lastActivity=millis();}else screenState.processingText=message;dirty=true;}return;
  }
  String resultId=jsonString(r->body,"request_id");if(!ok && status=="failed")resultId=actionRequestId;
  if(resultId!=actionRequestId){Serial.println("obsolete action result ignored");return;}
  if(qaSilent)Serial.println("assistant QA result: "+r->body);
  if(completedActionId==resultId){Serial.println("duplicate action result: no repeated output");return;}
  completedActionId=resultId;actionPending=false;voiceHold(false);screenState.actionStatus=status;screenState.phase=VoicePhase::Response;
  cJSON* root=cJSON_Parse(r->body.c_str());
  cJSON* revision=cJSON_GetObjectItemCaseSensitive(root,"revision");
  Serial.printf("assistant result: id=%s action=%s status=%s revision=%d\n",actionRequestId.c_str(),jsonString(r->body,"action").c_str(),status.c_str(),cJSON_IsNumber(revision)?revision->valueint:0);
  cJSON* settings=cJSON_GetObjectItemCaseSensitive(root,"settings");
  if(cJSON_IsObject(settings)) {
    cJSON* v=cJSON_GetObjectItemCaseSensitive(settings,"auto_voice");if(cJSON_IsBool(v))screenModel.autoVoice=cJSON_IsTrue(v);
    v=cJSON_GetObjectItemCaseSensitive(settings,"speak_responses");if(cJSON_IsBool(v))screenModel.speakResponses=cJSON_IsTrue(v);
    v=cJSON_GetObjectItemCaseSensitive(settings,"voice_threshold");if(cJSON_IsNumber(v))screenModel.threshold=v->valueint;
    v=cJSON_GetObjectItemCaseSensitive(settings,"idle_seconds");if(cJSON_IsNumber(v))screenModel.idleSeconds=v->valueint;
    v=cJSON_GetObjectItemCaseSensitive(settings,"calendar_rest");if(cJSON_IsBool(v))screenModel.calendarRest=cJSON_IsTrue(v);
    v=cJSON_GetObjectItemCaseSensitive(settings,"display_name");if(cJSON_IsString(v))screenModel.name=v->valuestring;
    voiceEnable(screenModel.autoVoice);
  }
  bool refresh=cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(root,"refresh")) && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(root,"refresh"))>0;
  if(refresh)syncSnapshot();
  cJSON* local=cJSON_GetObjectItemCaseSensitive(root,"local");cJSON* view=cJSON_GetObjectItemCaseSensitive(local,"view");
  if(r->generation==voiceGeneration && (currentView==5 || currentView==6)) {
    currentView=6;screenState.phase=VoicePhase::Response;screenState.answer=message.length()?message:"No pude interpretar el resultado.";screenState.viewSince=screenState.lastActivity=millis();
    cJSON* agendaDate=cJSON_GetObjectItemCaseSensitive(local,"date");screenState.agendaDate=cJSON_IsString(agendaDate)?String(agendaDate->valuestring):String();
    if(cJSON_IsString(view)){for(uint8_t i=0;i<10;++i)if(String(viewName(i))==view->valuestring){currentView=i;screenState.lastQuery=i;break;}}
    if(!qaSilent){if(screenModel.speakResponses && message.length()){String spoken=message;if(spoken.length()>600){int newline=spoken.indexOf('\n');spoken=newline>0 && newline<450?spoken.substring(0,newline)+". El detalle está en la pantalla.":"El resultado está en la pantalla. Podés seguir hablando.";}speakText(spoken);}else voiceSound();}
  }
  cJSON_Delete(root);
}
void checkAlarms(uint32_t now) {
  if(now-alarmTick<1000)return;alarmTick=now;
  if(!clockValid || qaSilent || !powerAlarmsAllowed())return;
  tm t={};if(!getLocalTime(&t,5))return;
  char date[11],hour[6];strftime(date,sizeof(date),"%Y-%m-%d",&t);strftime(hour,sizeof(hour),"%H:%M",&t);
  String minute=String(date)+" "+hour;
  if(minute==alarmMinute || voiceOccupied() || actionPending)return;
  for(unsigned i=0;i<screenModel.alarmCount;++i){auto& a=screenModel.alarms[i];
    if(!a.enabled || a.time!=hour || !alarmDue(a.days.c_str(),a.dueDate.c_str(),date,t.tm_wday))continue;
    String firedKey="alarm-"+String(i);
    if(prefs.getString(firedKey.c_str(),"")==a.id+minute)continue;
    if(!voiceSound())return;
    powerInteractive();prefs.putString(firedKey.c_str(),a.id+minute);
    screenState.answer="Alarma: "+a.label+" · "+hour;currentView=6;screenState.viewSince=screenState.lastActivity=now;dirty=true;
    Serial.printf("alarm fired: id=%s at=%s\n",a.id.c_str(),minute.c_str());
    if(a.days=="Una vez"){a.enabled=false;submit(NetworkKind::Mutation,"alarm-ack","POST",String("{\"alarm_id\":\"")+a.id+"\"}");}
  }
  alarmMinute=minute;
}
void applyNetworkResults() {
  while(auto r=networkPoll()) {
    bool ok=r->status>=200 && r->status<300;
    if(r->kind==NetworkKind::Snapshot) {
      syncPending=false;
      if(ok && importScreenModel(screenModel,r->body)) {
        screenModel.stale=false;voiceConfigure(screenModel.threshold);voiceEnable(screenModel.autoVoice);
        cachePut("snapshot",r->body);
        auto energyRoot=cJSON_Parse(r->body.c_str());auto energy=cJSON_GetObjectItemCaseSensitive(energyRoot,"power_config");
        if(cJSON_IsObject(energy)){char* encoded=cJSON_PrintUnformatted(energy);if(encoded){pendingPowerJson=encoded;free(encoded);}}cJSON_Delete(energyRoot);
        calendarConnected=r->body.indexOf("\"calendar_connected\":true")>=0;
        if(calendarConnected)fetchCalendarEvents();
      } else screenModel.stale=true;
    } else if(r->kind==NetworkKind::Calendar) {
      if(ok && importCalendarEvents(screenModel,r->body)){cachePut("events",r->body);powerCacheSave();}
      else screenModel.stale=true;
    } else if(r->kind==NetworkKind::Pair && ok) {
      String token=jsonString(r->body,"device_token");
      if(token.length()){deviceToken=token;prefs.putString("device_token",deviceToken);syncSnapshot();}
    } else if(r->kind==NetworkKind::Mutation && ok) syncSnapshot();
    else if(r->kind==NetworkKind::Voice) {
      voiceRelease();
      if(ok) {
        String transcript=jsonString(r->body,"text");
        if(transcript.length()){runAction(transcript,r->generation);}
        else {voiceHold(false);screenState.answer="No entendí. Intentá otra vez.";if(currentView==5)currentView=6;}
      } else if(r->generation==voiceGeneration && currentView==5) {
        currentView=6;screenState.phase=VoicePhase::Response;screenState.lastActivity=millis();
        screenState.answer=ok?jsonString(r->body,"text"):r->status==429?"Límite de voz alcanzado":r->status==503?"Servicio de voz no disponible":"Error de voz HTTP "+String(r->status)+". Intentá otra vez.";
        if(!screenState.answer.length())screenState.answer="No entendí. Intentá otra vez.";
        if(ok && screenState.answer.length()<=600)speakText(screenState.answer);
        else if(ok)Serial.println("speech skipped: transcript exceeds 600 characters");
      }
    }
    if(r->kind==NetworkKind::Action)applyActionResult(r);
    if(r->kind==NetworkKind::Speech) {
      if(ok && !qaSilent && r->generation==voiceGeneration && r->audio && voicePlay(r->audio,r->bytes)){r->audio=nullptr;Serial.println("speech queued for speaker");}
      else {voiceHold(false);Serial.printf("speech unavailable: HTTP %d or obsolete response\n",r->status);}
      screenState.lastActivity=millis();
    }
    Serial.printf("network result: kind=%u HTTP %d\n",unsigned(r->kind),r->status);
    if(r->kind==NetworkKind::Diagnostic)Serial.println(r->body);
    dirty=true;delete r;
  }
}

#include "power_runtime.h"

void configureClock() {
  if (WiFi.status() != WL_CONNECTED || ntpConfigured) return;
  configTzTime("ART3", "time.google.com", "pool.ntp.org");
  ntpConfigured = true; ntpStartedAt = millis();
}

void setup() {
  Serial.setRxBufferSize(16384);Serial.begin(115200);
  pinMode(kKeyPin, INPUT_PULLUP);
  Wire.begin(kI2cSda, kI2cScl, 100000);
  setenv("TZ", "ART3", 1); tzset();
  prefs.begin("esp32-mic", false);
  qaSilent=prefs.getBool("qa-silent",true);voiceAllowAutomatic(!qaSilent);
  importScreenModel(screenModel,prefs.getString("ui-config","{}"));
  importScreenModel(screenModel,prefs.getString("snapshot","{}"));
  importCalendarEvents(screenModel,prefs.getString("events","{}"));
  screenModel.stale=true;
  voiceConfigure(screenModel.threshold);voiceEnable(screenModel.autoVoice);
  screenState.lastActivity=screenState.viewSince=millis();
  wifiSsid = prefs.isKey("ssid") ? prefs.getString("ssid", "") : "";
  wifiPassword = prefs.isKey("wifi_password") ? prefs.getString("wifi_password", "") : "";
  deviceToken = prefs.isKey("device_token") ? prefs.getString("device_token", "") : "";
  conversationId=prefs.getString("action-conv","");actionRequestId=prefs.getString("action-id","");actionBody=prefs.getString("action-body","");
  rtcAvailable = rtcReadIntoSystem(); clockValid = rtcAvailable;
  powerBoot();
  if(!networkBegin())Serial.println("Network worker FAILED");
  Serial.printf("Microphone initialization: %s\n", voiceBegin() ? "OK" : "FAILED");
  voiceAllowAutomatic(!qaSilent);voiceConfigure(screenModel.threshold);voiceEnable(screenModel.autoVoice);
  networkWakeHook(powerNetworkUse);
  display.RLCD_Init();
  drawLiveData();
  Serial.println("\nESP32-S3-RLCD-4.2 ready. Type help for setup.");
  Serial.printf("firmware=views-v2 built=%s %s; KEY=1x voice / 2x next / 3x back\n",__DATE__,__TIME__);
  Wire.beginTransmission(kRtcAddress); const bool rtcFound = Wire.endTransmission() == 0;
  Wire.beginTransmission(kShtAddress); const bool shtFound = Wire.endTransmission() == 0;
  Serial.printf("I2C RTC=%s SHTC3=%s; Supabase=%s\n",
    rtcFound ? "present" : "missing", shtFound ? "present" : "missing",
    apiConfigured() ? "configured" : "needs device_secrets.h");
  if (wifiSsid.length()) powerNetworkUse();
  shtRead(); drawLiveData();
}

void loop() {
  uint32_t now=millis();
  applyNetworkResults();
  VoiceEvent event;
  while(voicePoll(event)) {
    if(event==VoiceEvent::Listening) {
      if(currentView!=5 && currentView!=6)screenState.lastQuery=currentView;
      ++voiceGeneration;currentView=5;screenState.phase=VoicePhase::Recording;
      screenState.lastActivity=now;
    } else if(event==VoiceEvent::Ready) {
      screenState.processingText="Transcribiendo tu voz.";if(currentView==5)screenState.phase=VoicePhase::Processing;
      voiceAllowAutomatic(false);transcribeVoice();
    } else if(event==VoiceEvent::PlaybackDone || event==VoiceEvent::PlaybackError) {
      screenState.lastActivity=now;Serial.println(event==VoiceEvent::PlaybackDone?"assistant ready: listening again":"assistant ready: playback failed");
    } else if(currentView==5) {
      currentView=6;screenState.answer=event==VoiceEvent::NoSpeech?"No detecté voz. KEY para repetir.":"Micrófono no disponible";
      screenState.lastActivity=now;
    }
    dirty=true;
  }
  uint8_t clicks=0;
  if(powerConsumeKey){if(digitalRead(kKeyPin)==HIGH){powerConsumeKey=false;keyClicks=KeyClicks{};screenState.lastActivity=now;}}
  else clicks=keyClicks.update(digitalRead(kKeyPin)==LOW,now);
  if(clicks) {
    Serial.printf("ui key: clicks=%u from=%s\n",clicks,viewName(currentView));
    powerInteractive();screenState.lastActivity=now;
    if(keyAction(clicks)==KeyAction::Listen)openListening();
    else {
      constexpr uint8_t cycle[]={0,1,2,3,4,7,8,9};
      uint8_t base=currentView==5 || currentView==6?screenState.lastQuery:currentView;
      int index=0;while(index<7 && cycle[index]!=base)++index;
      currentView=cycle[(index+(keyAction(clicks)==KeyAction::Next?1:7))%8];screenState.lastQuery=currentView;
      ++voiceGeneration;screenState.viewSince=now;dirty=true;
    }
  }
  // Bound serial work per loop so an imported configuration cannot starve KEY.
  for(int n=0;n<256 && Serial.available();++n) {
    char c=Serial.read();
    if(c=='\n' || c=='\r') {
      if(serialOverflow)Serial.println("Command too long; discarded");
      else if(serialLine.length())serialCommand(serialLine);
      serialLine="";serialOverflow=false;
    }
    else if(serialLine.length()<16007 && !serialOverflow)serialLine+=c;
    else serialOverflow=true;
  }
  now=millis(); // Commands/results may record activity after this iteration began.
  if(now-screenState.lastActivity>=screenModel.idleSeconds*1000UL && currentView!=5 && currentView!=7 && currentView!=8 && currentView!=9 && !voiceOccupied() && !actionPending) {
    currentView=7;screenState.agendaDate="";screenState.lastQuery=7;screenState.viewSince=now;dirty=true;
  }
  tm restDate={};char restDay[11]={};if(clockValid && getLocalTime(&restDate,5))strftime(restDay,sizeof(restDay),"%Y-%m-%d",&restDate);
  bool dailyTasks=false;for(int i=0;i<screenModel.taskCount;++i){auto& t=screenModel.tasks[i];if(t.date==restDay || (!t.done && (t.date.isEmpty() || t.date<String(restDay)))){dailyTasks=true;break;}}
  if(isRestView(currentView) && restRotationDue(now,screenState.viewSince) && !voiceOccupied() && !actionPending){currentView=nextRestView(currentView,dailyTasks);screenState.agendaDate="";screenState.lastQuery=currentView;screenState.viewSince=now;dirty=true;}
  // Follow-up speech remains possible from the result, with a less sensitive gate.
  voiceConfigure(currentView==6?max(800,int(screenModel.threshold)):screenModel.threshold);
  voiceAllowAutomatic(!qaSilent && !actionPending && !(currentView==5 && screenState.phase==VoicePhase::Processing));
  if(actionPending && int32_t(now-actionPollAt)>=0){actionPollAt=now+10000;auto req=new NetworkRequest;req->kind=NetworkKind::Action;req->path="action-status";req->method="POST";req->token=deviceToken;req->generation=actionGeneration;req->body=String("{\"request_id\":\"")+actionRequestId+"\",\"conversation_id\":\""+conversationId+"\"}";if(!networkSubmit(req))delete req;}
  checkAlarms(now);
  if(WiFi.status()==WL_CONNECTED && !ntpConfigured)configureClock();
  if(!clockValid && ntpConfigured) {tm t;if(getLocalTime(&t,5)){clockValid=true;rtcSetFromSystem();dirty=true;}}
  if(!recoveryChecked && WiFi.status()==WL_CONNECTED && clockValid && deviceToken.length()){recoveryChecked=true;if(actionRequestId.length()==36 && conversationId.length()==36)submit(NetworkKind::Action,"action-status","POST",String("{\"request_id\":\"")+actionRequestId+"\",\"conversation_id\":\""+conversationId+"\"}");}
  if(WiFi.status()==WL_CONNECTED && deviceToken.length() && (lastSync==0 || now-lastSync>=kSyncPeriodMs))syncSnapshot();
  if(now-lastSensorRead>=kSensorPeriodMs) {lastSensorRead=now;shtRead();if(clockValid && ntpConfigured)rtcSetFromSystem();}
  if(audioDebug && now-lastAudioDebug>=500){lastAudioDebug=now;voiceStatus();}
  if(currentView!=loggedView || screenState.phase!=loggedPhase) {
    Serial.printf("ui transition: %s -> %s phase=%s rms=%d threshold=%u generation=%lu\n",viewName(loggedView),viewName(currentView),phaseName(screenState.phase),voiceLevel(),screenModel.threshold,static_cast<unsigned long>(voiceGeneration));
    loggedView=currentView;loggedPhase=screenState.phase;
  }
  if(now-lastUiTick>=1000) {
    lastUiTick=now;
    tm t={};int minute=clockValid && getLocalTime(&t,5)?t.tm_yday*1440+t.tm_hour*60+t.tm_min:-1;
    if(minute!=lastClockMinute){lastClockMinute=minute;if(currentView==0 || currentView==3 || currentView>=7)dirty=true;}
    uint32_t pageTick=(now-screenState.viewSince)/6000;
    if(pageTick!=lastPageTick){lastPageTick=pageTick;if(currentView==1 || currentView==2 || currentView==3 || currentView==6 || currentView==8)dirty=true;}
    bool online=WiFi.status()==WL_CONNECTED;
    if(online!=lastOnline){lastOnline=online;dirty=true;}
  }
  if(currentView==5 && now-lastVoiceMeter>=200){lastVoiceMeter=now;dirty=true;}
  if(dirty && now-lastRender>=100) {
    screenState.view=currentView;screenState.rms=voiceLevel();screenState.online=WiFi.status()==WL_CONNECTED;
    screenState.clockValid=clockValid && getLocalTime(&screenState.local,5);
    renderScreen(display,screenModel,screenState,now);lastRender=now;dirty=false;
  }
  powerTick();
  delay(5);
}
