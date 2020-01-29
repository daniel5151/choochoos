#include <cctype>
#include <cstdlib>
#include <cstring>

#include "common/bwio.h"
#include "common/queue.h"

#include "user/dbg.h"
#include "user/syscalls.h"
#include "user/tasks/nameserver.h"

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
        ACK,
        PLAYER_CONFIG,
        SIGNUP,
        OUT_OF_SPACE,
        PLAY,
        PLAY_RESP,
        QUIT,
        OTHER_PLAYER_QUIT,
        SERVER_CONFIG,
    } tag;
    union {
        struct {
        } empty;

        struct {
            RPS choice;
        } play;

        struct {
            size_t num_games;
            size_t id;
        } player_config;

        struct {
            Result result;
        } play_resp;

        struct {
            bool pause_after_each;
        } server_config;
    };
};

namespace rps {
const char* RPS_SERVER = "RPSServer";
class Game {
    int tid1, tid2;
    RPS choice1, choice2;

   public:
    Game() : tid1(-1), tid2(-1), choice1(RPS::NONE), choice2(RPS::NONE) {}
    Game(int tid1, int tid2)
        : tid1(tid1), tid2(tid2), choice1(RPS::NONE), choice2(RPS::NONE) {
        assert(tid1 >= 0);
        assert(tid2 >= 0);
        assert(tid1 != tid2);
    }

    int empty() const {
        if (tid1 == -1 || tid2 == -1) return true;
        assert(tid1 >= 0);
        assert(tid2 >= 0);
        return false;
    }

    int player1() const {
        assert(!empty());
        return tid1;
    }

    int player2() const {
        assert(!empty());
        return tid2;
    }

    bool has(int tid) const { return tid1 == tid || tid2 == tid; }

    int other_tid(int tid) const {
        assert(has(tid));
        return tid1 == tid ? tid2 : tid1;
    }

    RPS choice_for(int tid) const {
        assert(has(tid));
        return tid == tid1 ? choice1 : choice2;
    }

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

    // returns the tid of the winner, or -1 on a draw
    int winner() const {
        assert(!empty());
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

    bool is_over() const {
        return choice1 != RPS::NONE && choice2 != RPS::NONE;
    }

    void reset() {
        choice1 = RPS::NONE;
        choice2 = RPS::NONE;
    }
};

#define NUM_GAMES 32

void Server() {
    NameServer::RegisterAs(RPS_SERVER);
    assert(NameServer::WhoIs(RPS_SERVER) == MyTid());
    Queue<int, 1> queue;
    Game games[NUM_GAMES];

    int tid;
    Message req;
    Message res;

    Message config_msg;
    Receive(&tid, (char*)&config_msg, sizeof(config_msg));
    assert(config_msg.tag == Message::SERVER_CONFIG);
    bool pause_after_each = config_msg.server_config.pause_after_each;
    Reply(tid, nullptr, 0);

    printf("[RPSServer] accepting signups...\r\n");
    while (true) {
        Receive(&tid, (char*)&req, sizeof(req));
        debug("received message tag=%d from tid=%d ", req.tag, tid);
        switch (req.tag) {
            case Message::SIGNUP: {
                if (queue.is_empty()) {
                    assert(queue.push_back(tid) == QueueErr::OK);
                    break;
                }

                int other_tid;
                assert(queue.pop_front(other_tid) == QueueErr::OK);

                // first, look for empty games
                Game* game = nullptr;
                for (int i = 0; i < NUM_GAMES; i++) {
                    if (games[i].empty()) {
                        game = &games[i];
                        break;
                    }
                }
                // if we're out of free games, return OUT_OF_SPACE to both
                // clients
                if (game == nullptr) {
                    res = (Message){.tag = Message::OUT_OF_SPACE, .empty = {}};
                    Reply(tid, (char*)&res, sizeof(res));
                    Reply(other_tid, (char*)&res, sizeof(res));
                }

                // otherwise, assign the clients to a game and  send ACKS to
                // both clients
                printf("[RPSServer] matching tids %d and %d" ENDL, tid,
                       other_tid);
                *game = Game(tid, other_tid);
                res = {.tag = Message::ACK, .empty = {}};
                Reply(tid, (char*)&res, sizeof(res));
                Reply(other_tid, (char*)&res, sizeof(res));
            } break;

            case Message::PLAY: {
                RPS choice = req.play.choice;
                assert(choice != RPS::NONE);
                bool found = false;
                for (int i = 0; i < NUM_GAMES; i++) {
                    Game& game = games[i];
                    if (game.has(tid)) {
                        found = true;
                        debug("game.set(tid=%d, choice=%d)", tid, (int)choice);
                        game.set(tid, choice);
                        if (game.is_over()) {
                            debug("game is over");
                            int winner = game.winner();
                            Result r1 = Result::DRAW;
                            Result r2 = Result::DRAW;
                            if (winner == game.player1()) {
                                r1 = Result::I_WON;
                                r2 = Result::I_LOST;
                            } else if (winner == game.player2()) {
                                r1 = Result::I_LOST;
                                r2 = Result::I_WON;
                            }
                            res = (Message){.tag = Message::PLAY_RESP,
                                            .play_resp = {.result = r1}};
                            Reply(game.player1(), (char*)&res, sizeof(res));
                            res.play_resp.result = r2;
                            Reply(game.player2(), (char*)&res, sizeof(res));
                            game.reset();
                            printf(
                                "~~~~~~~~~ press any key to continue "
                                "~~~~~~~~~" ENDL);
                            if (pause_after_each) bwgetc(COM2);
                        }
                        break;
                    }
                }

                if (!found) {
                    // the other player must have quit, so send
                    // OTHER_PLAYER_QUIT to tid
                    res = {Message::OTHER_PLAYER_QUIT, .empty = {}};
                    Reply(tid, (char*)&res, sizeof(res));
                }
            } break;

            case Message::QUIT: {
                for (int i = 0; i < NUM_GAMES; i++) {
                    Game* game = &games[i];
                    if (game->has(tid)) {
                        int other_tid = game->other_tid(tid);
                        if (queue.is_empty()) {
                            // clear the game. If the player sends PLAY at this
                            // point, they will receive OTHER_PLAYER_QUIT.
                            *game = Game();
                            printf(
                                "[RPSServer] tid %d quit, but no players are "
                                "waiting." ENDL,
                                tid);
                        } else {
                            // if there is a player waiting, match them up, and
                            // send ACK to the waiting player.
                            int waiting_tid;
                            assert(queue.pop_front(waiting_tid) ==
                                   QueueErr::OK);
                            RPS other_choice = game->choice_for(other_tid);
                            printf(
                                "[RPSServer] tid %d quit, but tid %d is "
                                "waiting. Matching tids %d and %d" ENDL,
                                tid, waiting_tid, other_tid, waiting_tid);
                            *game = Game(other_tid, waiting_tid);
                            if (other_choice != RPS::NONE) {
                                game->set(other_tid, other_choice);
                            }
                            res = {Message::ACK, .empty = {}};
                            Reply(waiting_tid, (char*)&res, sizeof(res));
                        }

                        break;
                    }
                }
                res = {Message::ACK, .empty = {}};
                Reply(tid, (char*)&res, sizeof(res));
            } break;

            default:
                panic("RPSServer: invalid message tag %x, shutting down",
                      req.tag);
        }
    }
}

void Client() {
    char prefix[32];
    int my_tid = MyTid();
    snprintf(prefix, 32, "[Client tid=%d id=?] ", my_tid);

    Message req;
    Message res;

    printf("%swaiting for player config" ENDL, prefix);
    int tid;
    int code = Receive(&tid, (char*)&req, sizeof(req));
    assert(code >= 0);
    assert(req.tag == Message::PLAYER_CONFIG);
    size_t num_games = req.player_config.num_games;
    size_t id = req.player_config.id;
    res = {Message::ACK, .empty = {}};
    Reply(tid, (char*)&res, sizeof(res));
    printf("%sreceived player config (num_games=%u, id=%u)" ENDL, prefix,
           num_games, id);
    memset(prefix, '\0', sizeof(prefix));
    snprintf(prefix, 32, "[Client tid=%d id=%u] ", my_tid, id);

    printf("%squerying nameserver for '%s'" ENDL, prefix, RPS_SERVER);
    int server = NameServer::WhoIs(RPS_SERVER);
    assert(server >= 0);
    printf("%sreceived reply from nameserver: %s=%d" ENDL, prefix, RPS_SERVER,
           server);

    printf("%sI want to play %u games. Sending signup..." ENDL, prefix,
           num_games);
    req = (Message){.tag = Message::SIGNUP, .empty = {}};
    code = Send(server, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    // TODO this happens if we exceed the mailbox size.
    if (code < 0) {
        panic("could not sign up: code=%d", code);
    }
    switch (res.tag) {
        case Message::ACK:
            printf("%sreceived signup ack" ENDL, prefix);
            break;
        default:
            panic("received non-ack response" ENDL);
    }

    for (size_t i = 0; i < num_games; i++) {
        RPS choice = (RPS)((rand() % 3) + 1);
        req = (Message){.tag = Message::PLAY, .play = {.choice = choice}};
        printf("%sI want to play %u more %s. Sending %s..." ENDL, prefix,
               num_games - i, (num_games - i) > 1 ? "games" : "game",
               str_of_rps(req.play.choice));
        int code =
            Send(server, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
        // TODO this seems to fail if we exceed the Mailbox size
        if (code < 0) {
            panic("could not send play message: code=%d", code);
        }

        switch (res.tag) {
            case Message::PLAY_RESP: {
                Result result = res.play_resp.result;
                printf("%s%s" ENDL, prefix,
                       result == Result::DRAW
                           ? "it's a draw"
                           : result == Result::I_WON ? "I won!" : "I lost :(");
            } break;
            case Message::OTHER_PLAYER_QUIT: {
                printf("%sother player quit! I guess I'll go home :(" ENDL,
                       prefix);
                printf("%sexiting" ENDL, prefix);
                Exit();
            }
            default: {
                panic("invalid reply from server: tag=%d" ENDL, res.tag);
            }
        }
    }
    req = (Message){.tag = Message::QUIT, .empty = {}};
    printf("%ssending quit" ENDL, prefix);
    code = Send(server, (char*)&req, sizeof(req), (char*)&res, sizeof(res));
    assert(code >= 0);
    printf("%sexiting" ENDL, prefix);
}

void setup_and_run() {
    int seed = 0;
    char c;
    bool pause_after_each = false;

    char line[24];
    printf("random seed (>= 0): ");
    bwgetline(line, sizeof(line));
    sscanf(line, "%d", &seed);
    assert(seed >= 0);

    printf("pause after each game (y/n)? ");
    bwgetline(line, 100);
    sscanf(line, "%c", &c);
    pause_after_each = (c == 'y');

    size_t num_players = 0;
    printf("num players (0-32): ");
    bwgetline(line, 100);
    sscanf(line, "%u", &num_players);
    assert(num_players <= 32);

    struct {
        int priority;
        size_t num_games;
    } players[num_players];

    for (size_t i = 0; i < num_players; i++) {
        int priority = 1;
        printf("player %u priority  (default 1): ", i + 1);
        bwgetline(line, 100);
        sscanf(line, "%d", &priority);
        size_t num_games = 3;
        printf("player %u num games (default 3): ", i + 1);
        bwgetline(line, 100);
        sscanf(line, "%u", &num_games);
        assert(num_games < 100);

        players[i].priority = priority;
        players[i].num_games = num_games;
    }

    srand((unsigned int)seed);

    // create the RPSServer with priority 0.
    int server = Create(0, rps::Server);
    Message m = {Message::SERVER_CONFIG,
                 .server_config = {.pause_after_each = pause_after_each}};
    int code = Send(server, (char*)&m, sizeof(m), nullptr, 0);
    assert(code == 0);

    size_t id = 1;
    for (auto& config : players) {
        int tid = Create(config.priority, rps::Client);
        if (tid < 0) panic("unable to create player - out of task descriptors");
        m = {Message::PLAYER_CONFIG,
             .player_config = {.num_games = config.num_games, .id = id}};
        Send(tid, (char*)&m, sizeof(m), /* ignore response */ nullptr, 0);

        id++;
    }
}

}  // namespace rps

void FirstUserTask() {
    assert(Create(0, NameServer::Task) == NameServer::TID);
    rps::setup_and_run();
}
