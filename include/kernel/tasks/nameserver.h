#pragma once

#define NAMESERVER_MAX_NAME_LEN 128

namespace NameServer {

constexpr int INVALID_TID = -1;
constexpr int UNKNOWN_NAME = -2;

const int TID = 1;

void Task();

int RegisterAs(const char* name);
int WhoIs(const char* name);
void Shutdown();

}  // namespace NameServer
