#pragma once

#include <cstddef>

namespace Uart {
extern const char* SERVER_ID;
void Server();
int Getc(int tid, int channel);
int Putc(int tid, int channel, char c);

// Getn blocks until n bytes are received from the UART, writing the bytes to
// buf.
int Getn(int tid, int channel, size_t n, char* buf);

// Putstr and Printf can atomically write up to 4096 bytes to the UART in a
// single call.
int Putstr(int tid, int channel, const char* msg);
int Printf(int tid, int channel, const char* format, ...)
    __attribute__((format(printf, 3, 4)));

// Drain empties the RX FIFO for the given channel.
void Drain(int tid, int channel);

// Getline reads a line of input from the uart into `line`, treating the
// backspace and enter keys appropriately.
void Getline(int tid, int channel, char* line, size_t len);
}  // namespace Uart
