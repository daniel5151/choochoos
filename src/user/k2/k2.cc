#include <cstdlib>
#include "user/syscalls.h"

enum class RPS { NONE = 0, ROCK = 1, PAPER = 2, SCISSORS = 3 };
enum class Result { DRAW, I_WON, I_LOST };

const char* str_of_rps(const RPS rps) {
    switch (rps) {
        case RPS::ROCK:
            return "rock";
        case RPS::PAPER:
            return "paper";
        case RPS::SCISSORS:
            return "scissors";
        default:
            panic("unknown RPS %d", (int)rps);
    }
}

struct Message {
    enum uint8_t {
        DONE,
        SIGNUP,
        SIGNUP_ACK,
        OUT_OF_SPACE,
        PLAY,
        PLAY_RESP,
        QUIT,
        QUIT_ACK,
        OTHER_PLAYER_QUIT,
    } tag;
    union {
        struct {
        } empty;

        struct {
            RPS choice;
        } play;

        struct {
            Result result;
        } play_resp;
    };
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
        assert(tid >= 0);
        assert(choice != RPS::NONE);
        if (!has(tid)) {
            panic("set(tid=%d, choice=%d) on game without tid", tid,
                  (int)choice);
        }

        if (tid == tid1) {
            choice1 = choice;
        } else {
            choice2 = choice;
        }
    }

    void add(int tid) {
        assert(tid >= 0);
        if (remaining() == 0) {
            panic("add() to full game");
        }

        if (tid1 == -1) {
            tid1 = tid;
        } else {
            tid2 = tid;
        }
    }

    void remove(int tid) {
        assert(tid >= 0);
        assert(has(tid));
        if (tid1 == tid) {
            tid1 = -1;
        } else {
            tid2 = -1;
        }
    }

    // returns the tid of the winner, or -1 on a draw
    int winner() {
        if (remaining() != 0) {
            panic("winner() called on non-full game");
        }
        if (choice1 == choice2) {
            return -1;
        }
        switch (choice1) {
            case RPS::ROCK:
                return choice2 == RPS::PAPER ? tid2 : tid1;
            case RPS::PAPER:
                return choice2 == RPS::SCISSORS ? tid2 : tid1;
            case RPS::SCISSORS:
                return choice2 == RPS::ROCK ? tid2 : tid1;
            default:
                assert(false);
        }
    }

    bool is_over() { return choice1 != RPS::NONE && choice2 != RPS::NONE; }
};

#define NUM_GAMES 16

void Server() {
    Game games[NUM_GAMES];

    int tid;
    Message req;
    Message res;

    while (true) {
        Receive(&tid, (char*)&req, sizeof(req));
        debug("received message tag=%d from tid=%d ", req.tag, tid);
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
                    res = (Message){.tag = Message::SIGNUP_ACK, .empty = {}};
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
                    res = (Message){.tag = Message::OUT_OF_SPACE, .empty = {}};
                    Reply(tid, (char*)&res, sizeof(res));
                }
            } break;

            case Message::PLAY: {
                RPS choice = req.play.choice;
                assert(choice != RPS::NONE);
                for (int i = 0; i < NUM_GAMES; i++) {
                    Game& game = games[i];
                    if (game.has(tid)) {
                        debug("game.set(tid=%d, choice=%d)", tid, (int)choice);
                        game.set(tid, choice);
                        if (game.is_over()) {
                            debug("game is over");
                            int winner = game.winner();
                            Result r1 = Result::DRAW;
                            Result r2 = Result::DRAW;
                            if (winner == game.tid1) {
                                r1 = Result::I_WON;
                                r2 = Result::I_LOST;
                            } else if (winner == game.tid2) {
                                r1 = Result::I_LOST;
                                r2 = Result::I_WON;
                            }
                            res = (Message){.tag = Message::PLAY_RESP,
                                            .play_resp = {.result = r1}};
                            Reply(game.tid1, (char*)&res, sizeof(res));
                            res.play_resp.result = r2;
                            Reply(game.tid2, (char*)&res, sizeof(res));
                            game.choice1 = RPS::NONE;
                            game.choice2 = RPS::NONE;
                            bwgetc(COM2);
                        }
                        break;
                    }
                }
            } break;

            case Message::QUIT: {
                for (int i = 0; i < NUM_GAMES; i++) {
                    Game& game = games[i];
                    if (game.has(tid)) {
                        game.remove(tid);
                    }
                }
                res = {Message::QUIT_ACK, .empty = {}};
                Reply(tid, (char*)&res, sizeof(res));
            } break;

            default:
                panic("RPSServer: invalid message tag %x, shutting down",
                      req.tag);
        }
    }
}

void Client() {
    int server = 1;  // TODO get this from the name server

    Message req;
    Message res;

    log("sending signup");
    req = (Message){.tag = Message::SIGNUP, .empty = {}};
    int code = Send(server, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    assert(code >= 0);
    switch (res.tag) {
        case Message::SIGNUP_ACK:
            log("received signup ack");
            break;
        default:
            panic("received non-ack response");
    }

    for (int i = 0; i < 4; i++) {
        RPS choice = (RPS)((rand() % 3) + 1);
        req = (Message){.tag = Message::PLAY, .play = {.choice = choice}};
        log("sending %s", str_of_rps(req.play.choice));
        int code =
            Send(server, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
        assert(code >= 0);

        switch (res.tag) {
            case Message::PLAY_RESP: {
                Result result = res.play_resp.result;
                log("%s",
                    result == Result::DRAW
                        ? "it's a draw"
                        : result == Result::I_WON ? "I won!" : "I lost :(");
            } break;
            case Message::OTHER_PLAYER_QUIT: {
                log("other player quit, time to go home");
                break;
            }
            default: {
                panic("invalid reply from server: tag=%d", res.tag);
            }
        }
    }
    req = (Message){.tag = Message::QUIT, .empty = {}};
    log("sending quit");
    code = Send(server, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    assert(code >= 0);
    log("exiting");
}

}  // namespace rps

void FirstUserTask() {
    int priorities[3] = {1, 2, 3};
    int server = Create(0, rps::Server);
    (void)server;  // TODO register this with the name server
    for (int i = 0; i < 3; i++) {
        Create(priorities[i % 3], rps::Client);
    }
}
