#pragma once
#include <Arduino.h>
#include <time.h>

constexpr int kScreenWidth = 400, kScreenHeight = 300;
constexpr size_t kMaxItems = 50;
enum class VoicePhase : uint8_t { Waiting, Recording, Processing, Response };
struct TaskData { String id, title, date; bool done = false; };
struct AlarmData { String id, label, time, days, dueDate; bool enabled = false; };
struct EventData { String title, date, time, endDate, endTime; bool allDay=false; };
struct ScreenModel {
  String name = "Compañero";
  TaskData tasks[kMaxItems]; AlarmData alarms[kMaxItems]; EventData events[kMaxItems];
  uint8_t taskCount = 0, alarmCount = 0, eventCount = 0;
  uint16_t threshold = 500, idleSeconds = 30;
  bool speakResponses = true, autoVoice = true, calendarRest = true, showEvents = true;
  bool calendarAvailable = false, stale = true, truncated = false, musicAvailable = false;
  String musicTitle, musicArtist;
  int duration = 0, position = 0, volume = 0;
  bool playing = false;
};
struct ScreenState {
  uint8_t view = 0, lastQuery = 0;
  uint32_t viewSince = 0, lastActivity = 0;
  VoicePhase phase = VoicePhase::Waiting;
  String agendaDate;
  String answer, processingText = "Transcribiendo tu voz.", actionStatus;
  int rms = 0;
  bool online = false, clockValid = false;
  tm local = {};
};
// Partial snapshots preserve fields absent from the current service contract.
bool importScreenModel(ScreenModel& model, const String& json);
bool importCalendarEvents(ScreenModel& model, const String& json);
