#include "cmd.h"

#include <cstring>

/// Return pointer to first element of non-leading whitespace
inline static char* ltrim(char* str, char ws) {
    while (*str == ws)
        str++;
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
    char sw_dir;

    // trim leading and trailing spaces
    input = ltrim(input, ' ');
    rtrim(input, ' ');

    // sscanf doesn't actually enfore having a space between the command and the
    // first number, so I added some manual checks. kinda jank, but it works.
    if (sscanf(input, "tr%u %u%n", &cmd.data.tr.no, &cmd.data.tr.speed, &chars_read) == 2) {
        if (input[2] != ' ')
            return std::nullopt;

        cmd.kind = Command::TR;
    } else if (sscanf(input, "rv%u%n", &cmd.data.rv.no, &chars_read) == 1) {
        if (input[2] != ' ')
            return std::nullopt;

        cmd.kind = Command::RV;
    } else if (sscanf(input, "sw%u %c%n", &cmd.data.sw.no, &sw_dir, &chars_read) == 2) {
        if (input[2] != ' ')
            return std::nullopt;

        sw_dir = (char)tolower(sw_dir);
        if (sw_dir != 'c' && sw_dir != 's')
            return std::nullopt;

        cmd.kind = Command::SW;
        cmd.data.sw.dir = (sw_dir == 'c')
            ? SwitchDir::Curved
            : SwitchDir::Straight;
    } else if (sscanf(input, "l%u%n", &cmd.data.light.no, &chars_read) == 1) {
        if (input[1] != ' ')
            return std::nullopt;

        cmd.kind = Command::LIGHT;
    } else if (strcmp("q", input) == 0) {
        cmd.kind = Command::Q;
    } else if (strcmp("s", input) == 0) {
        cmd.kind = Command::STOP;
    } else if (strcmp("g", input) == 0) {
        cmd.kind = Command::GO;
    } else {
        return std::nullopt;
    }

    if (input[chars_read] != '\0')
        return std::nullopt;


    return cmd;
}
