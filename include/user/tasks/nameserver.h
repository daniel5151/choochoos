#pragma once

#include <cstddef>

#define NAMESERVER_MAX_NAME_LEN 128

namespace NameServer {

const int TID = 1;

enum class MessageKind : size_t { WhoIs, RegisterAs, Shutdown };

struct Request {
    MessageKind kind;
    union {
        struct {
            char name[NAMESERVER_MAX_NAME_LEN];
            size_t len;
        } who_is;
        struct {
            char name[NAMESERVER_MAX_NAME_LEN];
            size_t len;
            int tid;
        } register_as;
    };
};

struct Response {
    MessageKind kind;
    union {
        struct {
            bool success;
            int tid;
        } who_is;
        struct {
            bool success;
        } register_as;
    };
};

void Task();

int RegisterAs(const char* name);
int WhoIs(const char* name);

}  // namespace NameServer
