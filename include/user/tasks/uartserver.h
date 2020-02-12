#pragma once

namespace Uart {
void Server();
int Getc(int tid, int channel);
int Putc(int tid, int channel, char c);
int Printf(int tid, int channel, const char* format, ...)
    __attribute__((format(printf, 3, 4)));
void Shutdown(int tid);
}  // namespace Uart
