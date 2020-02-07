#pragma once

#define NAMESERVER_MAX_NAME_LEN 128

namespace NameServer {

const int TID = 1;

void Task();

int RegisterAs(const char* name);
int WhoIs(const char* name);

}  // namespace NameServer
