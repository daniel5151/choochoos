#pragma once

namespace Clock {
void Server();

int Time(int tid);
int Delay(int tid, int ticks);
int DelayUntil(int tid, int ticks);
void Shutdown(int tid);

extern const char* SERVER_ID;

}  // namespace Clock
