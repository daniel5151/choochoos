#pragma once

namespace Clock {
void Server();

int Time(int tid);
int Delay(int tid, int ticks);
int DelayUntil(int tid, int ticks);

static const char* SERVER_ID = "ClockServer";
}  // namespace Clock
