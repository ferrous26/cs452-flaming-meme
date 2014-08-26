
#include <vt100.h>
#include <std.h>

#include <tasks/task_launcher.h>

static int consume_integer(const char* const str, int* const index) {
    int result = 0, neg = 0;

    if (str[*index] == '-') {
        neg     = 1;
        *index += 1;
    } else if (str[*index] == '+') {
        *index += 1;
    }

    for (; isdigit( str[*index] ); *index += 1) {
        result = (result * 10) + (str[*index] - '0');
    }
    return neg ? -result : result;
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

static command parse_s(const char* const cmd, int* buffer) {
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
    case 'p':
        return SEPPUKU;
    case 'z':
        return SIZES;
    default:
        return ERROR;
    }
}

static command parse_t(const char* const cmd, int* const buffer) {
    int index = 1;
    switch(cmd[index++]) {
    case 'r':
        if (parse_argument(cmd, 'i', &index, buffer))     return ERROR;
        if (parse_argument(cmd, 'i', &index, &buffer[1])) return ERROR;
        if (!isspace(cmd[index]))                         return ERROR;
        return LOC_SPEED;
    default:
        return ERROR;
    }
}

static command parse_q(const char* const cmd, int* const buffer) {
    UNUSED(buffer);

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
    }

    return ERROR;
}

static command parse_b(const char* const cmd, int* const buffer) {
    int index = 1;
    UNUSED(buffer);

    if (cmd[index++] != 'm')    return ERROR;
    if (!isspace(cmd[index++])) return ERROR;

    return CMD_BENCHMARK;
}

static command parse_h(const char* const cmd, int* const buffer) {
    int index = 1;

    switch (cmd[index++]) {
    case 'r':
	if (parse_argument(cmd, 'i', &index, buffer)) return ERROR;
	return LOC_HORN;
    }

    return ERROR;
}

static command parse_e(const char* const cmd, int* const buffer) {
    UNUSED(buffer);
    int index = 1;

    if (cmd[index++] != 't')   return ERROR;
    if (!isspace(cmd[index++])) return ERROR;

    return CMD_ECHO;
}

static inline command
parse_command(const char* const cmd, int* const buffer)
{
    switch (cmd[0]) {
    case '\r': return NONE;
    case 'b':  return parse_b(cmd, buffer);
    case 'e':  return parse_e(cmd, buffer);
    case 'h':  return parse_h(cmd, buffer);
    case 'q':  return parse_q(cmd, buffer);
    case 's':  return parse_s(cmd, buffer);
    case 'r':  return parse_r(cmd, buffer);
    case 't':  return parse_t(cmd, buffer);
    default:   return ERROR;
    }
}
