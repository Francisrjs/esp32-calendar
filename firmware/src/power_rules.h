#pragma once
#include <stdint.h>
#include "input_rules.h"

struct AbsenceRule {
  bool enabled=false;
  uint8_t days=0; // Monday bit 0, Sunday bit 6; overnight belongs to start day.
  uint16_t start=0, end=0;
};
constexpr bool validAbsence(const AbsenceRule& r) {
  return r.days && !(r.days&128) && r.start<1440 && r.end<1440 && r.start!=r.end;
}
RULE_CONSTEXPR inline bool absenceAt(const AbsenceRule& r,int weekMinute) {
  if(!r.enabled || !validAbsence(r))return false;
  int w=(weekMinute%10080+10080)%10080;
  int day=w/1440, minute=w%1440;
  if(r.start<r.end)return (r.days&(1<<day)) && minute>=r.start && minute<r.end;
  return ((r.days&(1<<day)) && minute>=r.start) ||
         ((r.days&(1<<((day+6)%7))) && minute<r.end);
}
RULE_CONSTEXPR inline bool anyAbsence(const AbsenceRule* rules,int count,int weekMinute) {
  for(int i=0;i<count;++i)if(absenceAt(rules[i],weekMinute))return true;
  return false;
}
// Scan the union, so adjacent/overlapping rules cannot produce brief wakeups.
RULE_CONSTEXPR inline int absenceRemaining(const AbsenceRule* rules,int count,int weekMinute) {
  if(!anyAbsence(rules,count,weekMinute))return 0;
  for(int n=1;n<=10080;++n)if(!anyAbsence(rules,count,weekMinute+n))return n;
  return 10080;
}
