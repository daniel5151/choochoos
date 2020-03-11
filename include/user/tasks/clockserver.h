#pragma once

namespace Clock {
constexpr int TICKS_PER_SEC = 100;  // 10ms

void Server();

int Time(int tid);
int Delay(int tid, int ticks);
int DelayUntil(int tid, int ticks);
void Shutdown(int tid);

extern const char* SERVER_ID;

}  // namespace Clock
