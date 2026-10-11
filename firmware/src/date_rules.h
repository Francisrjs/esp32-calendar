#pragma once
#include <stdint.h>
#include "input_rules.h"
// Gregorian civil date to Unix seconds, independent of libc timezone state.
inline RULE_CONSTEXPR int64_t utcSeconds(int year,int month,int day,int hour,int minute,int second) {
  year-=month<=2;
  const int era=(year>=0?year:year-399)/400;
  const unsigned yoe=year-era*400;
  const unsigned doy=(153*(month+(month>2?-3:9))+2)/5+day-1;
  const unsigned doe=yoe*365+yoe/4-yoe/100+doy;
  return (int64_t(era)*146097+doe-719468)*86400+hour*3600+minute*60+second;
}
