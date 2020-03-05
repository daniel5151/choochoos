#pragma once

#include <optional>

#include "marklin.h"

/// Tagged-union of the various commands
struct Command {
    enum {
        ADDTR,
        GO,
        LIGHT,
        Q,
        ROUTE,
        RV,
        STOP,
        SW,
        TR,
    } kind;
    union {
        struct {
            size_t no;
        } addtr;
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
            Marklin::BranchDir dir;
        } sw;
        struct {
            size_t no;
            size_t speed;
        } tr;
        struct {
            size_t train;
            char sensor_group;
            size_t sensor_idx;
            int offset;
        } route;
    };

    /// Tries to parse a valid command from the given string
    ///
    /// NOTE: this method will mutate `char* s` (it trims trailing whitespace).
    static std::optional<Command> from_string(char* s);
};
