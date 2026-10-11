#pragma once
constexpr bool alarmTextEqual(const char* a,const char* b) {
  return *a==*b && (!*a || alarmTextEqual(a+1,b+1));
}
constexpr bool alarmDue(const char* days,const char* dueDate,const char* date,int weekday) {
  return alarmTextEqual(days,"Todos los días") ||
    (alarmTextEqual(days,"Lun a vie") && weekday>=1 && weekday<=5) ||
    (alarmTextEqual(days,"Fin de semana") && (weekday==0 || weekday==6)) ||
    (alarmTextEqual(days,"Una vez") && dueDate[0] && alarmTextEqual(dueDate,date));
}
