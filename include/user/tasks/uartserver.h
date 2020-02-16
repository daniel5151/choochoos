#pragma once

#include <cstddef>

namespace Uart {
extern const char* SERVER_ID;
void Server();
int Getc(int tid, int channel);
int Putc(int tid, int channel, char c);

// Putstr and Printf can atomically write up to 4096 bytes to the UART in a
// single call.
int Putstr(int tid, int channel, const char* msg);
int Printf(int tid, int channel, const char* format, ...)
    __attribute__((format(printf, 3, 4)));

void Getline(int tid, int channel, char* line, size_t len);

void Shutdown(int tid);
}  // namespace Uart
