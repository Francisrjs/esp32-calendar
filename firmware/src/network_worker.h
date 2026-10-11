#pragma once
#include <Arduino.h>
enum class NetworkKind : uint8_t { Snapshot, Calendar, Pair, Mutation, Voice, Speech, Diagnostic, Action };
struct NetworkRequest {
  NetworkKind kind;
  String path, method, body, token;
  uint32_t generation = 0;
  uint8_t* audio = nullptr; size_t bytes = 0;
};
struct NetworkResult {
  NetworkKind kind; String body; int status = -1; uint32_t generation = 0;
  uint8_t* audio=nullptr; size_t bytes=0;
  ~NetworkResult(){free(audio);}
};
bool networkBegin();
// Ownership transfers on success; on failure caller retains the request.
bool networkSubmit(NetworkRequest* request);
NetworkResult* networkPoll();
bool apiConfigured();

bool networkBusy();
void networkWakeHook(void (*hook)());
