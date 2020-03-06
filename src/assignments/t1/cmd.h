#pragma once

#include <optional>

#include "marklin.h"

/// Tagged-union of the various commands
struct Command {
    struct route_t {
        size_t train;
        char sensor_group;
        size_t sensor_idx;
        int offset;
        bool dry_run;
    };

    enum {
        ADDTR,
        GO,
        HELP,
        LIGHT,
        Q,
        ROUTE,
        RV,
        STOP,
        SW,
        TR,
        NORMALIZE,
        PATH,
    } kind;
    union {
        struct {
            size_t no;
        } addtr;
        struct {
        } go;
        struct {
        } help;
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
        route_t route;
        struct {
            char sensor_group;
            size_t sensor_idx;
            int offset;
        } normalize;
        struct {
            Marklin::sensor_t source;
            Marklin::sensor_t dest;
        } path;
    };

    /// Tries to parse a valid command from the given string
    ///
    /// NOTE: this method will mutate `char* s` (it trims trailing whitespace).
    static std::optional<Command> from_string(char* s);
};
