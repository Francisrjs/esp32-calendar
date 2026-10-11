#pragma once
#include <Arduino.h>
enum class VoiceEvent : uint8_t { Listening, Ready, NoSpeech, Error, PlaybackDone, PlaybackError };
bool voiceBegin();
bool voicePoll(VoiceEvent& event);
bool voiceListen();
void voiceEnable(bool enabled);
void voiceStatus();
void voiceActivity(bool enabled);
int voiceLevel();
void voiceRegisters();
// Buffer owned by audio task until Ready; main owns it until voiceRelease.
uint8_t* voiceWav(size_t& bytes);
void voiceRelease();

void voiceConfigure(int threshold);
void voiceAllowAutomatic(bool allowed);
bool voiceSound();

void voiceHold(bool value);
bool voicePlay(uint8_t* pcm,size_t bytes); // Ownership transfers on success.
bool voiceOccupied();

// Hardware changes are acknowledged by the owning capture task.
bool voiceSetPower(bool enabled,uint32_t timeoutMs=1000);
bool voicePowered();
