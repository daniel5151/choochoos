#include "kernel/tasks/nameserver.h"

#include <cstdio>
#include <cstring>

#include "common/bwio.h"
#include "user/debug.h"
#include "user/syscalls.h"

template <size_t N>
class StringArena {
   private:
    char buf[N];
    size_t index;

   public:
    StringArena() : buf{0}, index{0} {}

    // copies the provided string into the arena. returns idx
    size_t add(const char* s, const size_t n) {
        if (this->index + n >= N) {
            panic("nameserver has run out of string space");
        }
        memcpy(&this->buf[this->index], s, n);
        size_t idx = this->index;
        this->index += n + 1;
        return idx;
    }

    // returns string from index, or nullptr if the idx is out of bounds
    const char* get(const size_t idx) {
        if (idx >= N) {
            return nullptr;
        }
        return &this->buf[idx];
    }
};

namespace NameServer {

enum class MessageKind : size_t { WhoIs, RegisterAs, Shutdown };

struct Request {
    MessageKind kind;
    union {
        struct {
        } shutdown;
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
        } shutdown;
        struct {
            bool success;
            int tid;
        } who_is;
        struct {
            bool success;
        } register_as;
    };
};

void Task() {
    assert(MyTid() == TID);
    Request msg{};
    int tid;

    StringArena<2048> strings;
    struct {
        size_t idx;
        int tid;
    } names[128];  // indexes into the string arena
    size_t names_head = 0;

    for (;;) {
        Receive(&tid, (char*)&msg, sizeof(Request));
        switch (msg.kind) {
            case MessageKind::Shutdown: {
                Response res = {.kind = MessageKind::Shutdown, .shutdown = {}};
                Reply(tid, (char*)&res, sizeof(Response));
                debug("nameserver is shutting down");
                return;
            } break;
            case MessageKind::WhoIs: {
                int found_tid = -1;

                for (size_t i = 0; i < names_head; i++) {
                    auto& name = names[i];
                    assert(strings.get(name.idx) != nullptr);
                    if (strcmp(strings.get(name.idx), msg.who_is.name) == 0) {
                        found_tid = name.tid;
                        break;
                    }
                }

                Response res;
                if (found_tid == -1) {
                    res = {.kind = msg.kind, .who_is = {false, -1}};
                } else {
                    res = {.kind = msg.kind, .who_is = {true, found_tid}};
                }

                debug("NameServer returning %d for %s (to tid %d)", found_tid,
                      msg.who_is.name, tid);
                Reply(tid, (char*)&res, sizeof(Response));

            } break;
            case MessageKind::RegisterAs: {
                if (names_head >= 128) {
                    panic("nameserver has exceeded the registration limit");
                }
                // check if string already exists
                bool found_existing = false;
                for (size_t i = 0; i < names_head; i++) {
                    auto& name = names[i];
                    assert(strings.get(name.idx) != nullptr);
                    if (strcmp(strings.get(name.idx), msg.register_as.name) ==
                        0) {
                        found_existing = true;
                        name.tid = msg.register_as.tid;
                        break;
                    }
                }

                if (!found_existing) {
                    // create a new entry
                    auto idx =
                        strings.add(msg.register_as.name, msg.register_as.len);

                    names[names_head].idx = idx;
                    names[names_head].tid = tid;
                    names_head += 1;
                    debug("NameServer registered %d for %s",
                          msg.register_as.tid, msg.register_as.name);
                } else {
                    debug("NameServer already registered %s",
                          msg.register_as.name);
                }

                Response res = {.kind = msg.kind, .register_as = {true}};
                Reply(tid, (char*)&res, sizeof(Response));

            } break;
        }
    }
}

int RegisterAs(const char* name) {
    size_t len = strlen(name);
    assert(len < NAMESERVER_MAX_NAME_LEN);
    NameServer::Request req{
        .kind = MessageKind::RegisterAs,
        .register_as = {.name = {'\0'}, .len = len, .tid = MyTid()}};
    strcpy(req.register_as.name, name);

    NameServer::Response res;

    if (Send(NameServer::TID, (char*)&req, sizeof(req), (char*)&res,
             sizeof(res)) != sizeof(res)) {
        return INVALID_TID;
    }
    assert(res.kind == MessageKind::RegisterAs);
    return res.register_as.success ? 0 : UNKNOWN_NAME;
}

int WhoIs(const char* name) {
    size_t len = strlen(name);
    assert(len < NAMESERVER_MAX_NAME_LEN);
    NameServer::Request req{.kind = MessageKind::WhoIs,
                            .who_is = {.name = {'\0'}, .len = len}};
    strcpy(req.who_is.name, name);

    NameServer::Response res;

    const int res_len = Send(NameServer::TID, (char*)&req, sizeof(req),
                             (char*)&res, sizeof(res));
    if (res_len != sizeof(res)) {
        return INVALID_TID;
    }
    assert(res.kind == MessageKind::WhoIs);
    return res.who_is.success ? res.who_is.tid : -2;
}

void Shutdown() {
    NameServer::Request req{.kind = MessageKind::Shutdown, .shutdown = {}};
    NameServer::Response res;
    const int res_len = Send(NameServer::TID, (char*)&req, sizeof(req),
                             (char*)&res, sizeof(res));
    assert(res_len == sizeof(res));
    assert(res.kind == MessageKind::Shutdown);
}

}  // namespace NameServer

extern "C" int WhoIs(const char* name) { return NameServer::WhoIs(name); }
extern "C" int RegisterAs(const char* name) {
    return NameServer::RegisterAs(name);
}
