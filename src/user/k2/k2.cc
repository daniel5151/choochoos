#include <functional>
#include "bwio.h"
#include "user/syscalls.h"

enum class RPS { NONE, ROCK, PAPER, SCISSORS };

struct Message {
    enum { DONE, SIGNUP, SIGNUP_ACK, OUT_OF_SPACE, PLAY } tag;
    union {
        struct {
        } empty;

        struct {
            RPS choice;
        } play;
    } data;
};

namespace rps {
struct Game {
    int tid1, tid2;
    RPS choice1, choice2;

    Game() : tid1(-1), tid2(-1), choice1(RPS::NONE), choice2(RPS::NONE) {}

    int remaining() {
        int count = 0;
        if (tid1 == -1) count++;
        if (tid2 == -1) count++;
        return count;
    }

    bool has(int tid) { return tid1 == tid || tid2 == tid; }

    void set(int tid, RPS choice) {
        if (!has(tid)) {
            bwprintf(COM2, "set(tid=%d, choice=%d) on game without tid\r\n",
                     tid, (int)choice);
            Exit();
        }

        if (tid == tid1) {
            choice1 = choice;
        } else {
            choice2 = choice;
        }
    }

    void add(int tid) {
        if (remaining() == 0) {
            bwprintf(COM2, "add() to full game\r\n");
            Exit();
        }

        if (tid1 == -1) {
            tid1 = tid;
        } else {
            tid2 = tid;
        }
    }
};

#define NUM_GAMES 16

void Server() {
    Game games[NUM_GAMES];

    int tid;
    Message req;
    Message res;

    while (true) {
        Receive(&tid, (char*)&req, sizeof(req));
        switch (req.tag) {
            case Message::DONE:
                return;
            case Message::SIGNUP: {
                // first, look for nearly full games
                Game* game = nullptr;
                for (int i = 0; i < NUM_GAMES; i++) {
                    if (games[i].remaining() == 1) {
                        game = &games[i];
                        break;
                    }
                }
                if (game != nullptr) {
                    game->add(tid);
                    // we have a full game, send ACKS to both clients
                    res = (Message){.tag = Message::SIGNUP_ACK, .data = {}};
                    Reply(game->tid1, (char*)&res, sizeof(res));
                    Reply(game->tid2, (char*)&res, sizeof(res));
                    break;
                }
                // otherwise, find the first empty game
                for (int i = 0; i < NUM_GAMES; i++) {
                    if (games[i].remaining() == 2) {
                        game = &games[i];
                        break;
                    }
                }
                if (game != nullptr) {
                    game->add(tid);
                } else {
                    res = (Message){.tag = Message::OUT_OF_SPACE, .data = {}};
                    Reply(tid, (char*)&res, sizeof(res));
                }
            } break;

            case Message::PLAY: {
                for (int i = 0; i < NUM_GAMES; i++) {
                    if (games[i].has(tid)) {
                        games[i].set(tid, res.data.play.choice);
                        // TODO if game full, send out results
                        break;
                    }
                }
            } break;

            default:
                bwprintf(
                    COM2,
                    "RPSServer: invalid message tag %x, shutting down.\r\n",
                    req.tag);
                return;
        }
    }
}

void Client() {
    int server = 1;  // TODO get this from the name server

    Message req;
    Message res;

    req = (Message){.tag = Message::SIGNUP, .data = {.empty = {}}};
    int code = Send(server, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    if (code < 0) {
        bwprintf(COM2, "tid=%d could not send to RPSServer: %d\r\n", MyTid(),
                 code);
        Exit();
    }
    switch (res.tag) {
        case Message::SIGNUP_ACK:
            bwprintf(COM2, "tid=%d received signup ack\r\n", MyTid());
            break;
        default:
            bwprintf(COM2, "tid=%d received non-ack response %d\r\n", MyTid(),
                     res.tag);
            Exit();
    }
}

}  // namespace rps

void FirstUserTask() {
    int server = Create(0, rps::Server);
    (void)server;  // TODO register this with the name server
    for (int i = 0; i < 3; i++) {
        Create(1, rps::Client);
    }
}
