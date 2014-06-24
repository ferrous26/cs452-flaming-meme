
#include <vt100.h>
#include <std.h>

#include <parse.h>
#include <debug.h>

static int consume_integer(const char* const str, int* const index) {
    int result = 0;
    for (; isdigit( str[*index] ); *index += 1) {
        result = (result * 10) + (str[*index] - '0');
    }
    return result;
}

static int consume_whitespace(const char* const str, int* const index) {
    if  (!isspace(str[*index])) return -1;
    for (; isspace(str[*index]); *index += 1);
    if  (str[*index] == '\0') return 1;

    return 0;
}

static int parse_argument(const char* const cmd,
			  const char type,
			  int* const index,
			  int* const arg) {

    if (consume_whitespace(cmd, index)) {
        log("Command parsing failed at position %d (%d)\n",
	    (uint)*index,
	    cmd[*index]);
        return -1;
    }

    switch(type) {
    case 'i':
        *arg = consume_integer(cmd, index);
        break;
    case 'c':
        if (cmd[*index] == '\0') return -1;
        *arg = cmd[*index];
        *index += 1;
        break;
    }
    return 0;
}

static int parse_s(const char* const cmd, int* buffer) {
    int index = 1;

    switch (cmd[index++]) {
    case 't':
        if (!isspace(cmd[index++])) return ERROR;
        return CMD_STRESS;
    case 'w':
        if (parse_argument(cmd, 'i', &index, buffer ))     return ERROR;
        if (parse_argument(cmd, 'c', &index, &buffer[1] )) return ERROR;
        if (!isspace(cmd[index]))                          return ERROR;
        return TRACK_TURNOUT;
    case 's':
        if (parse_argument(cmd, 'c', &index, buffer))     return ERROR;
        if (parse_argument(cmd, 'i', &index, &buffer[1])) return ERROR;
        if (parse_argument(cmd, 'i', &index, &buffer[2])) return ERROR;
        if (!isspace(cmd[index]))                         return ERROR;
        return SWITCH_STOP;
    case 'd':
        if (!isspace(cmd[index])) return ERROR;
        return SWITCH_TIME;
    default:
        return ERROR;
    }
}

static int parse_train(const char* const cmd, int* const buffer) {
    int index = 1;
    if (cmd[index++] != 'r')                          return ERROR;
    if (parse_argument(cmd, 'i', &index, buffer))     return ERROR;
    if (parse_argument(cmd, 'i', &index, &buffer[1])) return ERROR;
    if (!isspace(cmd[index]))                         return ERROR;

    return LOC_SPEED;
}

static int parse_stop(const char* const cmd) {
    int index = 1;

    if (!isspace(cmd[index])) return ERROR;
    return QUIT;
}

static command parse_r(const char* const cmd, int* const buffer) {
    int index = 1;
    switch (cmd[index++]) {
    case 'v':
        if (parse_argument(cmd, 'i', &index, buffer)) return ERROR;
        if (!isspace(cmd[index]))                     return ERROR;
        return LOC_REVERSE;
    case 'e':
        if (!isspace(cmd[index])) return ERROR;
        return TRACK_RESET;
    case 'l':
        if (parse_argument(cmd, 'i', &index, buffer)) return ERROR;
        if (!isspace(cmd[index]))                     return ERROR;
        return REVERSE_LOOKUP;
    }

    return ERROR;
}

static command parse_light(const char* const cmd, int* const buffer) {
    int index = 1;
    switch (cmd[index++]) {
    case 't':
        if (parse_argument(cmd, 'i', &index, buffer)) return ERROR;
        if (!isspace(cmd[index]))                     return ERROR;
        return LOC_LIGHT;
    }

    return ERROR;
}

static command parse_benchmark(const char* const cmd) {
    int index = 1;

    if (cmd[index++] != 'm')   return ERROR;
    if (!isspace(cmd[index++])) return ERROR;

    return CMD_BENCHMARK;
}

static command parse_horn(const char* const cmd, int* const buffer) {
    int index = 1;

    switch (cmd[index++]) {
    case 'r':
	if (parse_argument(cmd, 'i', &index, buffer)) return ERROR;
	return LOC_HORN;
    }

    return ERROR;
}

static command parse_echo(const char* const cmd) {
    int index = 1;

    if (cmd[index++] != 't')   return ERROR;
    if (!isspace(cmd[index++])) return ERROR;

    return CMD_ECHO;
}

command parse_command(const char* const cmd, int* const buffer) {
    switch (cmd[0]) {
    case '\r': return NONE;
    case 'e':  return parse_echo(cmd);
    case 'q':  return parse_stop(cmd);
    case 'b':  return parse_benchmark(cmd);
    case 't':  return parse_train(cmd, buffer);
    case 'r':  return parse_r(cmd, buffer);
    case 's':  return parse_s(cmd, buffer);
    case 'l':  return parse_light(cmd, buffer);
    case 'h':  return parse_horn(cmd, buffer);
    default:   return ERROR;
    }
}
