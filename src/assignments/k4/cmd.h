#pragma once

#include <optional>

#include "trainctl.h"

enum command_kind {
    CMD_INVALID = 0,
    CMD_G,
    CMD_L,
    CMD_Q,
    CMD_RV,
    CMD_S,
    CMD_SW,
    CMD_TR,
};

/// Tagged-union of the various commands
struct Command {
    enum {
        GO,
        LIGHT,
        Q,
        RV,
        STOP,
        SW,
        TR,
    } kind;
    union {
        struct {
        } go;
        struct {
            size_t no;
        } light;
        struct {
        } q;
        struct {
            size_t no;
        } rv;
        struct {
        } stop;
        struct {
            size_t no;
            SwitchDir dir;
        } sw;
        struct {
            size_t no;
            size_t speed;
        } tr;
    };

    /// Tries to parse a valid command from the given string
    ///
    /// NOTE: this method will mutate `char* s` (it trims trailing whitespace).
    static std::optional<Command> from_string(char* s);
};
