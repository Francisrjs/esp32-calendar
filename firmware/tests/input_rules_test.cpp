// Evaluated by the ESP32 compiler itself; requires no hardware or host compiler.
#include "../src/input_rules.h"
#include "../src/date_rules.h"
constexpr bool thresholdRules(int threshold) {
  VoiceGate g;
  if(g.update(threshold,threshold))return false;
  if(g.update(threshold+1,threshold))return false;
  if(g.update(threshold+1,threshold))return false;
  if(!g.update(threshold+1,threshold))return false;
  if(g.update(threshold,threshold))return false;
  if(g.update(threshold+100,threshold))return false;
  if(g.update(threshold-1,threshold))return false;
  if(g.update(threshold+1,threshold))return false;
  if(g.update(threshold+1,threshold))return false;
  return g.update(threshold+1,threshold);
}
constexpr bool clicks(unsigned count,uint32_t origin=0) {
  KeyClicks k;
  for(unsigned i=0;i<count;++i) {
    uint32_t t=origin+i*160;
    if(k.update(true,t))return false;
    if(k.update(true,t+45))return false;
    if(k.update(false,t+60))return false;
    if(k.update(false,t+105))return false;
  }
  uint32_t released=origin+(count-1)*160+105;
  if(k.update(false,released+379))return false;
  return k.update(false,released+380)==(count>3?3:count) && k.update(false,released+800)==0;
}
constexpr bool bounce() {
  KeyClicks k;
  k.update(true,0);k.update(false,10);k.update(true,20);k.update(false,30);
  return k.update(false,500)==0;
}
static_assert(thresholdRules(500),"strict threshold 500 and isolated peaks");
static_assert(thresholdRules(800),"strict threshold 800 and isolated peaks");
static_assert(clicks(1),"single click executes after 380 ms");
static_assert(clicks(2),"double click never executes a single first");
static_assert(clicks(3),"triple click never executes a double first");
static_assert(clicks(5),"four or more clicks go back");
static_assert(clicks(3,0xffffff00U),"millis wraparound");
static_assert(bounce(),"reject contact bounce");
static_assert(!captureFinished(2000,true,899),"keep speech before silence limit");
static_assert(captureFinished(2000,true,900),"finish at 900 ms silence");
static_assert(!captureFinished(4499,false,4499),"manual no-speech grace period");
static_assert(captureFinished(4500,false,4500),"reject empty manual capture");
static_assert(captureFinished(12000,true,0),"maximum 12 seconds");
static_assert(utcSeconds(1970,1,1,0,0,0)==0,"Unix epoch");
static_assert(utcSeconds(2024,3,1,0,0,0)-utcSeconds(2024,2,28,0,0,0)==172800,"leap day");
static_assert(utcSeconds(2026,1,1,0,0,0)-utcSeconds(2025,12,31,0,0,0)==86400,"year transition");

#include "../src/alarm_rules.h"
static_assert(alarmDue("Lun a vie","","2026-10-12",1),"Monday alarm");
static_assert(!alarmDue("Lun a vie","","2026-10-10",6),"No weekday alarm on Saturday");
static_assert(alarmDue("Fin de semana","","2026-10-11",0),"Sunday alarm");
static_assert(!alarmDue("Fin de semana","","2026-10-12",1),"No weekend alarm on Monday");
static_assert(alarmDue("Una vez","2026-10-11","2026-10-11",0),"One-shot date matches");
static_assert(!alarmDue("Una vez","2026-10-11","2026-10-12",1),"One-shot not repeated");
static_assert(!alarmDue("Una vez","","2026-10-12",1),"No undated one-shot");

static_assert(keyAction(0)==KeyAction::None,"no press does nothing");
static_assert(keyAction(1)==KeyAction::Listen,"single press listens");
static_assert(keyAction(2)==KeyAction::Next,"double press advances");
static_assert(keyAction(3)==KeyAction::Previous,"triple press goes back");

static_assert(!restRotationDue(179999,0),"rest lasts full three minutes");
static_assert(restRotationDue(180000,0),"rest rotates at three minutes");
static_assert(restRotationDue(179899,0xffffff9bU),"rotation survives millis overflow");
static_assert(nextRestView(7)==8 && nextRestView(8)==9 && nextRestView(9)==7,"rest daily weekly cycle");

static_assert(nextRestView(7,false)==9 && nextRestView(9,false)==7,"no tasks skips daily even with calendar events");
static_assert(nextRestView(8,false)==9,"empty daily exits to weekly");

static_assert(thresholdRules(1200),"strict threshold 1200 rejects distant lower-level sound");
