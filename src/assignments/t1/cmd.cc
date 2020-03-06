#include "cmd.h"

#include <cstring>

/// Return pointer to first element of non-leading whitespace
inline static char* ltrim(char* str, char ws) {
    while (*str == ws) str++;
    return str;
}

/// Trim trailing whitespace by modifying string in-place
inline static char* rtrim(char* str, char ws) {
    char* s = str + strlen(str);
    while (*--s == ws)
        ;
    *(s + 1) = '\0';
    return str;
}

std::optional<Command> Command::from_string(char* input) {
    Command cmd;

    int chars_read = 1;
    char buf_char;

    // trim leading and trailing spaces
    input = ltrim(input, ' ');
    rtrim(input, ' ');

    // sscanf doesn't actually enfore having a space between the command and the
    // first number, so I added some manual checks. kinda jank, but it works.
    if (sscanf(input, "tr%u %u%n", &cmd.tr.no, &cmd.tr.speed, &chars_read) ==
        2) {
        if (input[2] != ' ') return std::nullopt;

        cmd.kind = Command::TR;
    } else if (sscanf(input, "rv%u%n", &cmd.rv.no, &chars_read) == 1) {
        if (input[2] != ' ') return std::nullopt;

        cmd.kind = Command::RV;
    } else if (sscanf(input, "sw%u %c%n", &cmd.sw.no, &buf_char, &chars_read) ==
               2) {
        if (input[2] != ' ') return std::nullopt;

        buf_char = (char)tolower(buf_char);
        if (buf_char != 'c' && buf_char != 's') return std::nullopt;

        cmd.kind = Command::SW;
        cmd.sw.dir = (buf_char == 'c') ? Marklin::BranchDir::Curved
                                       : Marklin::BranchDir::Straight;
    } else if (sscanf(input, "l%u%n", &cmd.light.no, &chars_read) == 1) {
        if (input[1] != ' ') return std::nullopt;

        cmd.kind = Command::LIGHT;
    } else if (sscanf(input, "route%u %c%u %d%n", &cmd.route.train,
                      &cmd.route.sensor_group, &cmd.route.sensor_idx,
                      &cmd.route.offset, &chars_read) == 4) {
        if (input[5] != ' ') return std::nullopt;

        if (strcmp(" --dry-run", input + chars_read) == 0) {
            cmd.route.dry_run = true;
            chars_read += 10;
        } else {
            cmd.route.dry_run = false;
        }

        cmd.route.sensor_group = (char)toupper(cmd.route.sensor_group);
        cmd.kind = Command::ROUTE;
    } else if (sscanf(input, "addtr%u%n", &cmd.addtr.no, &chars_read) == 1) {
        if (input[5] != ' ') return std::nullopt;
        cmd.kind = Command::ADDTR;
    } else if (sscanf(input, "n %c%u %d%n", &cmd.normalize.sensor_group,
                      &cmd.normalize.sensor_idx, &cmd.normalize.offset,
                      &chars_read) == 3) {
        cmd.kind = Command::NORMALIZE;
        cmd.normalize.sensor_group = (char)toupper(cmd.normalize.sensor_group);
    } else if (sscanf(input, "path %c%hhu %c%hhu%n", &cmd.path.source.group,
                      &cmd.path.source.idx, &cmd.path.dest.group,
                      &cmd.path.dest.idx, &chars_read) == 4) {
        cmd.path.source.group = (char)toupper(cmd.path.source.group);
        cmd.path.dest.group = (char)toupper(cmd.path.dest.group);
        cmd.kind = Command::PATH;
    } else if (strcmp("help", input) == 0) {
        chars_read = 4;
        cmd.kind = Command::HELP;
    } else if (strcmp("q", input) == 0) {
        cmd.kind = Command::Q;
    } else if (strcmp("s", input) == 0) {
        cmd.kind = Command::STOP;
    } else if (strcmp("g", input) == 0) {
        cmd.kind = Command::GO;
    } else {
        return std::nullopt;
    }

    if (input[chars_read] != '\0') return std::nullopt;

    return cmd;
}
