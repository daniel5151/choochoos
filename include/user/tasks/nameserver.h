#pragma once

namespace NameServer {
enum MessageKind { WhoIs, RegisterAs, Shutdown };

struct Request {
    MessageKind kind;
    union {
        struct {
            const char* name;
        } who_is;
        struct {
            const char* name;
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

void NameServer();
}  // namespace NameServer
