#include <cstdio>
#include <cstring>

#include "bwio.h"
#include "user/syscalls.h"

namespace NameServer {
enum MessageKind : size_t { WhoIs, RegisterAs, Shutdown };

struct Request {
    MessageKind kind;
    union {
        struct {
            const char* buf;
        } who_is;
        struct {
            const char* buf;
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

template <size_t N>
class StringArena {
   private:
    char buf[N];
    size_t index;

   public:
    struct Result {
        enum { Ok, OutOfSpace } kind;
        union {
            struct {
                size_t idx;
            } ok;
        };
    };

    // copies the provided string into the arena. returns idx
    size_t add(const char* s, const size_t n) {
        if (this->index + n >= N) {
            panic("nameserver has run out of string space");
        }
        memcpy(&this->buf[this->index], s, n);
        size_t idx = this->index;
        this->index += n;
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

void NameServer() {
    Request msg;
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
            case MessageKind::Shutdown:
                return;
            case MessageKind::WhoIs: {
                int found_tid = -1;

                for (size_t i = 0; i < names_head; i++) {
                    auto& name = names[i];
                    assert(strings.get(name.idx) != nullptr);
                    if (strcmp(strings.get(name.idx), msg.who_is.buf) == 0) {
                        found_tid = name.tid;
                        break;
                    }
                }

                Response res;
                if (found_tid == -1) {
                    res = {msg.kind, .who_is = {false, -1}};
                } else {
                    res = {msg.kind, .who_is = {true, found_tid}};
                }

                debug("NameServer returning %d for %s (to tid %d)", found_tid,
                      msg.who_is.buf, tid);
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
                    if (strcmp(strings.get(name.idx), msg.register_as.buf) ==
                        0) {
                        found_existing = true;
                        name.tid = msg.register_as.tid;
                        break;
                    }
                }

                if (!found_existing) {
                    // create a new entry
                    auto idx = strings.add(msg.register_as.buf,
                                           strlen(msg.register_as.buf) + 1);

                    names[names_head].idx = idx;
                    names[names_head].tid = tid;
                    names_head += 1;
                }

                Response res = {msg.kind, .register_as = {true}};
                debug("NameServer registered %d for %s", msg.register_as.tid,
                      msg.register_as.buf);
                Reply(tid, (char*)&res, sizeof(Response));

            } break;
        }
    }
}
}  // namespace NameServer
