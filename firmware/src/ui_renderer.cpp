#include "ui_renderer.h"
#include "ui_assets.h"
#include "date_rules.h"
#include <cmath>
namespace {
DisplayPort* lcd;
void pixel(int x,int y) { if(x>=0 && x<400 && y>=0 && y<300) lcd->RLCD_SetPixel(x,y,ColorBlack); }
void line(int x,int y,int width) { for(int i=0;i<width;++i) pixel(x+i,y); }
void box(int x,int y,int w,int h) { line(x,y,w);line(x,y+h-1,w);for(int j=0;j<h;++j){pixel(x,y+j);pixel(x+w-1,y+j);} }
uint16_t nextCode(const String& s,size_t& i) {
  uint8_t a=s[i++]; if(a<128) return a;
  if(a>=0xc2 && a<=0xdf && i<s.length()) return ((a&31)<<6)|(uint8_t(s[i++])&63);
  while(i<s.length() && (uint8_t(s[i])&0xc0)==0x80) ++i;
  return '?';
}
int text(int x,int y,const String& s,int size=12,int width=356,int lines=1) {
  const UiGlyph* glyphs=size==40?uiGlyph40:size==18?uiGlyph18:uiGlyph12;
  const uint8_t* data=size==40?uiFont40:size==18?uiFont18:uiFont12;
  const int origin=x; int row=0;
  for(size_t i=0;i<s.length();) {
    uint16_t code=nextCode(s,i);
    if(code=='\n') {x=origin;y+=size+4;if(++row>=lines)break;continue;}
    int index=code>=32 && code<=126?code-32:code>=160 && code<=255?95+code-160:code==8230?191:'?'-32;
    UiGlyph g; memcpy_P(&g,glyphs+index,sizeof(g));
    if(x+g.width>origin+width) {if(++row>=lines)break;x=origin;y+=size+4;}
    for(int py=0;py<size+4;++py)for(int px=0;px<g.width;++px)
      if(pgm_read_byte(data+g.offset+py*((g.width+7)/8)+px/8)&(128>>(px&7)))pixel(x+px,y+py);
    x+=g.width;
  }
  return x;
}
String answerPage(const String& answer,uint32_t now,uint32_t since,int& number,int& total) {
  size_t starts[64]={};total=1;int x=0,row=0;
  for(size_t i=0;i<answer.length();){size_t at=i;uint16_t code=nextCode(answer,i);
    int index=code>=32 && code<=126?code-32:code>=160 && code<=255?95+code-160:code==8230?191:'?'-32;
    UiGlyph glyph;memcpy_P(&glyph,uiGlyph18+index,sizeof(glyph));
    if(code=='\n' || x+glyph.width>356){x=0;++row;
      if(row>=8 && total<64){starts[total++]=code=='\n'?i:at;row=0;}
      if(code=='\n')continue;
    }
    x+=glyph.width;
  }
  number=((now-since)/10000)%total;
  return answer.substring(starts[number],number+1<total?starts[number+1]:answer.length());
}
String clock(const ScreenState& s) {char b[6];if(!s.clockValid)return "--:--";strftime(b,sizeof(b),"%H:%M",&s.local);return b;}
String date(const ScreenState& s) {char b[11];if(!s.clockValid)return "";strftime(b,sizeof(b),"%Y-%m-%d",&s.local);return b;}
void dog(int x,int y) {for(int py=0;py<202;++py)for(int px=0;px<210;++px)if(pgm_read_byte(uiMascot+py*27+px/8)&(128>>(px&7)))pixel(x+px,y+py);}
const char* months[]={"Enero","Febrero","Marzo","Abril","Mayo","Junio","Julio","Agosto","Septiembre","Octubre","Noviembre","Diciembre"};
void calendar(const ScreenState& s,int x,int y,int cell) {
  if(!s.clockValid){text(x,y,"Esperando hora",12,cell*7,2);return;}
  text(x,y,String(months[s.local.tm_mon])+" "+String(s.local.tm_year+1900),12,cell*7);
  const char* days[]={"L","M","M","J","V","S","D"};
  for(int i=0;i<7;++i)text(x+i*cell,y+22,days[i]);
  tm first=s.local;first.tm_mday=1;first.tm_isdst=-1;mktime(&first);
  tm last=first;++last.tm_mon;last.tm_mday=0;mktime(&last);
  for(int d=1;d<=last.tm_mday;++d) {
    int c=(first.tm_wday+6)%7+d-1,px=x+(c%7)*cell,py=y+44+(c/7)*21;
    text(px,py,String(d),12,cell);if(d==s.local.tm_mday)box(px-2,py,cell-1,19);
  }
}
int page(uint32_t now,uint32_t since,int count,int size){return ((now-since)/6000)%max(1,(count+size-1)/size);}
String pagination(int p,int count,int size){return count>size?" · "+String(p+1)+"/"+String((count+size-1)/size):"";}
int minutes(const String& value){return value.length()>=5?value.substring(0,2).toInt()*60+value.substring(3,5).toInt():0;}
String dayShift(const String& day,int offset){int y,mo,d;if(sscanf(day.c_str(),"%d-%d-%d",&y,&mo,&d)!=3)return "";tm t={};t.tm_year=y-1900;t.tm_mon=mo-1;t.tm_mday=d+offset;t.tm_hour=12;t.tm_isdst=-1;time_t epoch=mktime(&t);localtime_r(&epoch,&t);char out[11];strftime(out,11,"%Y-%m-%d",&t);return out;}
void agenda(const ScreenModel& m,const ScreenState& s,uint32_t now,bool week){
  if(!s.clockValid){text(12,32,"Esperando una hora válida",18);return;}
  String selected=s.agendaDate.length()==10?s.agendaDate:date(s),first=selected;
  if(week){int y,mo,d;sscanf(selected.c_str(),"%d-%d-%d",&y,&mo,&d);tm t={};t.tm_year=y-1900;t.tm_mon=mo-1;t.tm_mday=d;t.tm_hour=12;t.tm_isdst=-1;mktime(&t);first=dayShift(selected,-((t.tm_wday+6)%7));}
  String last=dayShift(first,week?6:0);int from=24,to=0,events=0;
  for(int i=0;i<m.eventCount;++i){auto& e=m.events[i];if(!m.showEvents || e.date>last || (e.endDate.length() && e.endDate<first) || (!e.endDate.length() && e.date<first))continue;++events;if(!e.allDay){from=min(from,e.date<first?0:minutes(e.time)/60);to=max(to,e.endDate>e.date?24:(minutes(e.endTime)+59)/60);}}
  if(from>=to){from=8;to=22;}from=max(0,from);to=min(24,max(from+4,to));
  text(8,8,(week?String("SEMANA "):String("DIARIO "))+first.substring(5)+(week?" / "+last.substring(5):""),18,384);
  if(!m.calendarAvailable)text(8,32,"Google sin vincular",12);
  else if(!events)text(8,32,"Sin eventos en este período",12);
  if(week){
    const char* names[]={"L","M","X","J","V","S","D"};
    for(int d=0;d<7;++d){int x=30+d*52;String day=dayShift(first,d);text(x+2,36,String(names[d])+day.substring(8),12,50);if(day==date(s))line(x+2,53,48);box(x,58,52,182);
      int total=0;for(int i=0;i<m.taskCount;++i)if(!m.tasks[i].done && m.tasks[i].date==day)++total;text(x+2,244,String(total)+" T",12,50);
    }
    for(int h=from;h<=to;h+=max(1,(to-from)/5)){int y=62+(h-from)*172/(to-from);text(1,y-4,String(h),12,27);}
    for(int i=0;i<m.eventCount;++i){auto& e=m.events[i];if(!m.showEvents)continue;for(int d=0;d<7;++d){String day=dayShift(first,d);if(day<e.date || day>(e.endDate.length()?e.endDate:e.date) || (e.allDay && e.endDate.length() && day==e.endDate))continue;
      int start=e.allDay?from*60:day==e.date?minutes(e.time):from*60,end=e.allDay?from*60+60:day==e.endDate?minutes(e.endTime):to*60;if(end<=start)end=start+30;
      int y=62+(max(from*60,start)-from*60)*172/((to-from)*60),h=max(12,(min(to*60,end)-max(from*60,start))*172/((to-from)*60));h=min(h,238-y);if(h<=0)continue;int x=32+d*52;box(x,y,48,h);text(x+6,y+1,e.title.substring(0,3),12,40);if(h>=30)text(x+6,y+16,e.allDay?"Día":e.time,12,40);
    }}
  }else{
    uint8_t ids[kMaxItems],n=0;for(int i=0;i<m.eventCount;++i){auto& e=m.events[i];if(m.showEvents && e.date<=selected && (e.endDate.length()?e.endDate>=selected:e.date==selected) && !(e.allDay && e.endDate==selected))ids[n++]=i;}
    int p=page(now,s.viewSince,n,4);for(int h=from;h<=to;h+=max(1,(to-from)/5)){int x=170+(h-from)*218/(to-from);text(x,36,String(h),12,25);for(int y=55;y<146;y+=4)pixel(x,y);}
    for(int row=0;row<4 && p*4+row<n;++row){auto& e=m.events[ids[p*4+row]];int y=56+row*23;text(6,y,e.allDay?"DÍA":e.time,12,45);text(54,y,e.title,12,108);int start=e.allDay?from*60:max(from*60,minutes(e.time)),end=e.allDay?to*60:e.endDate>selected?to*60:minutes(e.endTime);if(end<=start)end=start+30;int x=170+(start-from*60)*218/((to-from)*60),w=max(4,(min(end,to*60)-start)*218/((to-from)*60));box(x,y,min(w,398-x),16);for(int py=y+2;py<y+14;py+=3)line(x+1,py,max(0,min(w-2,396-x)));}
    if(selected==date(s)){int at=s.local.tm_hour*60+s.local.tm_min;if(at>=from*60 && at<=to*60){int x=170+(at-from*60)*218/((to-from)*60);for(int y=52;y<148;++y)pixel(x,y);}}
    line(0,154,400);text(6,160,"TAREAS DEL DÍA"+pagination(p,n,4),12);
    uint8_t tasks[kMaxItems],nt=0;for(int pass=0;pass<2;++pass)for(int i=0;i<m.taskCount;++i){auto& t=m.tasks[i];if(t.done==(pass==1) && (t.date==selected || (!t.done && (t.date=="" || t.date<selected))))tasks[nt++]=i;}
    int tp=page(now,s.viewSince,nt,4);for(int row=0;row<4 && tp*4+row<nt;++row){auto& t=m.tasks[tasks[tp*4+row]];int y=183+row*19;box(6,y,11,11);if(t.done)line(8,y+5,7);text(24,y-2,t.title,12,302);text(334,y-2,t.date==selected?"HOY":t.date.length()?t.date.substring(5):"S/F",12,62);}
    if(!nt)text(8,190,"Sin tareas para este día",12);
  }
}
void circle(int cx,int cy,int r){for(int i=0;i<360;++i){double a=i*3.141592653589793/180;pixel(cx+int(r*cos(a)),cy+int(r*sin(a)));}}
}
void renderScreen(DisplayPort& display,const ScreenModel& m,const ScreenState& s,uint32_t now) {
  lcd=&display;display.RLCD_ColorClear(ColorWhite);
  uint8_t pending=0;for(int i=0;i<m.taskCount;++i)if(!m.tasks[i].done)++pending;
  uint8_t upcoming[kMaxItems],count=0;
  if(m.showEvents)for(int i=0;i<m.eventCount;++i)
    if(!s.clockValid || m.events[i].date>date(s) || (m.events[i].date==date(s) && (m.events[i].time=="Todo el día" || m.events[i].time>=clock(s))))upcoming[count++]=i;
  String footer;
  if(s.view==0 || s.view==7) {
    if(s.view==0)text(22,18,m.name+" · "+(s.clockValid?date(s):"Sin hora"));
    dog(22,41);text(244,s.view==0?78:42,clock(s),40,140);
    if(s.view==7 && m.calendarRest)calendar(s,244,100,19);
    else {
      text(244,135,String(pending)+" pendientes",12,134);
      text(244,165,"PRÓXIMO MOMENTO",12,134);
      if(count){auto& e=m.events[upcoming[0]];text(244,187,e.time+" "+e.title,12,134,4);}
      else text(244,187,m.calendarAvailable?"Tu agenda está libre":"Google sin vincular",12,134,3);
    }
    footer=s.view==7?"Reposo":"Un día, más simple.";
  } else if(s.view==8 || s.view==9) {
    agenda(m,s,now,s.view==9);footer="Reposo · rota cada 3 min";
  } else if(s.view==1) {
    text(22,18,"TUS HORARIOS");text(22,37,"Alarmas",18);
    int p=page(now,s.viewSince,m.alarmCount,2);
    for(int n=0;n<2 && p*2+n<m.alarmCount;++n){auto& a=m.alarms[p*2+n];int y=85+n*68;
      text(22,y,a.time,18,80);text(114,y,a.label,12,176,2);text(114,y+36,a.days,12,176);text(302,y,a.enabled?"ACTIVA":"APAGADA",12,76);line(22,y+63,356);}
    if(!m.alarmCount)text(22,100,"Sin alarmas",18);
    footer=String(m.alarmCount)+" horarios"+pagination(p,m.alarmCount,2);
  } else if(s.view==2) {
    text(22,18,"UNA COSA A LA VEZ");text(22,37,"Tareas / "+String(pending)+" pendientes",18);
    int p=page(now,s.viewSince,m.taskCount,4);
    for(int n=0;n<4 && p*4+n<m.taskCount;++n){auto& t=m.tasks[p*4+n];int y=85+n*44;
      box(22,y+4,14,14);if(t.done){line(25,y+10,8);line(25,y+11,8);}text(46,y,t.title,12,260,2);text(316,y,t.date.substring(5),12,62);}
    if(!m.taskCount)text(22,100,"Todo listo. No hay pendientes.",18);
    footer="Tareas"+pagination(p,m.taskCount,4);
  } else if(s.view==3) {
    text(22,18,"ESPACIO PARA TU DÍA");calendar(s,22,44,26);text(224,40,"Próximos",18,154);
    int p=page(now,s.viewSince,count,3);
    for(int n=0;n<3 && p*3+n<count;++n){auto& e=m.events[upcoming[p*3+n]];int y=82+n*54;
      text(224,y,e.date.substring(5)+" · "+e.time,12,154);text(224,y+18,e.title,12,154,2);}
    if(!count)text(224,92,m.showEvents?(m.calendarAvailable?"Agenda libre":"Google sin vincular"):"Eventos ocultos",12,154,2);
    footer="Agenda"+pagination(p,count,3);
  } else if(s.view==4) {
    text(22,18,"TU BANDA SONORA");for(int r=20;r<=68;r+=4)circle(94,135,r);circle(94,135,8);
    text(184,68,m.musicAvailable?m.musicTitle:"Sin reproductor",18,194,3);text(184,137,m.musicArtist,12,194,2);
    text(184,180,m.musicAvailable?(m.playing?"Reproduciendo":"En pausa"):"Conectar desde la web",12,194);
    text(22,222,"Volumen "+String(m.volume)+"%",12);box(184,224,194,8);
    if(m.duration)for(int y=225;y<231;++y)line(185,y,192LL*m.position/m.duration);
    footer=String(m.position/60)+":"+(m.position%60<10?"0":"")+String(m.position%60)+" / "+String(m.duration/60)+":"+(m.duration%60<10?"0":"")+String(m.duration%60);
  } else if(s.view==5) {
    text(22,18,"TU VOZ");text(22,62,s.phase==VoicePhase::Recording?"Capturando tu voz.":s.phase==VoicePhase::Processing?"Un momento…":"Te escucho.",18);
    text(22,106,s.phase==VoicePhase::Recording?"Terminá tu frase. El silencio cierra la captura.":s.phase==VoicePhase::Processing?s.processingText:"Hablá para superar "+String(m.threshold)+" RMS.",12,356,3);
    box(22,203,356,14);for(int y=205;y<215;++y)line(24,y,min(352,s.rms*352/1400));
    footer=String(s.rms)+" RMS";
  } else {
    int p=0,total=1;String visible=answerPage(s.answer,now,s.viewSince,p,total);
    text(22,18,"TU ASISTENTE"+(total>1?String(" · ")+String(p+1)+"/"+String(total):String()));text(22,62,visible,18,356,8);
    text(22,246,m.autoVoice?"Resultado · podés seguir hablando":"Resultado · KEY para hablar",12);
    footer=m.autoVoice?String("Otra frase: >")+String(max(800,int(m.threshold)))+" RMS":"KEY para escuchar";
  }
  line(22,263,356);
  text(22,269,(!s.online || m.stale?"Sin conexión / datos guardados":m.truncated?"Lista limitada a 50":footer),12,245);
  text(286,269,"KEY VOZ · 2× VISTA",12,92);
  display.RLCD_Display();
}
