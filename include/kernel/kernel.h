#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void kexit(int status);
void kpanic(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif
