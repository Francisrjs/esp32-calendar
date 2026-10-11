#pragma once
#include <stdint.h>
#if __cplusplus >= 201402L
#define RULE_CONSTEXPR constexpr
#else
#define RULE_CONSTEXPR
#endif
// Independent timing rules shared by firmware and host tests.
enum class KeyAction { None, Listen, Next, Previous };
constexpr KeyAction keyAction(uint8_t clicks) {
  return clicks==0?KeyAction::None:clicks==1?KeyAction::Listen:clicks==2?KeyAction::Next:KeyAction::Previous;
}
struct KeyClicks {
  bool raw=false, stable=false; uint8_t clicks=0;
  uint32_t edgeAt=0, lastClick=0;
  RULE_CONSTEXPR uint8_t update(bool down,uint32_t now) {
    if(down!=raw){raw=down;edgeAt=now;}
    if(stable!=raw && now-edgeAt>=45){stable=raw;if(!stable){if(clicks<3)++clicks;lastClick=now;}}
    if(clicks && !stable && !raw && now-lastClick>=380){uint8_t n=clicks;clicks=0;return n;}
    return 0;
  }
};
struct VoiceGate {
  unsigned consecutive=0;
  RULE_CONSTEXPR bool update(float rms,int threshold) {
    consecutive=rms>threshold?(consecutive<3?consecutive+1:3):0;
    return consecutive>=3;
  }
  RULE_CONSTEXPR void reset(){consecutive=0;}
};
RULE_CONSTEXPR inline bool captureFinished(uint32_t elapsed,bool heardSpeech,uint32_t silence) {
  return elapsed>=12000 || (heardSpeech && silence>=900) || (!heardSpeech && elapsed>=4500);
}

constexpr bool isRestView(uint8_t view){return view==7 || view==8 || view==9;}
constexpr uint8_t nextRestView(uint8_t view,bool hasTasks=true){return view==7?(hasTasks?8:9):view==8?9:7;}
constexpr bool restRotationDue(uint32_t now,uint32_t since){return uint32_t(now-since)>=180000UL;}
