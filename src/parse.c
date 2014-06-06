
#include <io.h>
#include <std.h>

#include <parse.h>


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
        kprintf("died at position %d(%d)\n", *index, cmd[*index]);
        return -1;
    }

    switch(type) {
    case 'i':
        *arg = consume_integer(cmd, index);
        break;
    case 'c':
        if( cmd[*index] == '\0' ) {
	    // TODO: the compiler complains about these lines...why?
            // kprintf("died at position %d(%d)\n", *index, cmd[*index]);
            // return -1;
        }
        *arg = cmd[*index];
        *index += 1;
        break;
    }
    return 0;
}

static int parse_gate(const char* const cmd, int* buffer) {
    int index = 1;
    if (cmd[index++] != 'w') return -1;
    if (parse_argument(cmd, 'i', &index, buffer )) return -1;
    if (parse_argument(cmd, 'c', &index, &buffer[1] )) return -1;
    if (!isspace(cmd[index])) return -1;

    return 0;
}

static int parse_train(const char* const cmd, int* const buffer) {
    int index = 1;
    if (cmd[index++] != 'r') return -1;
    if (parse_argument(cmd, 'i', &index, buffer)) return -1;
    if (parse_argument(cmd, 'i', &index, &buffer[1])) return -1;
    if (!isspace(cmd[index])) return -1;

    return 0;
}

static int parse_stop(const char* const cmd) {
    return !isspace(cmd[1]);
}

static int parse_reverse(const char* const cmd, int* const buffer) {
    int index = 1;
    if(cmd[index++] != 'v') return -1;
    if(parse_argument(cmd, 'i', &index, buffer)) return -1;
    return 0;
}

command parse_command(const char* const cmd, int* const buffer) {
    switch (cmd[0]) {
    case '\r':
        return NONE;
    case 'q':
        if ( parse_stop(cmd) ) { return ERROR; }
        return QUIT;
    case 't':
        if (parse_train(cmd, buffer)) { return ERROR; }
        return SPEED;
    case 'r':
	if (parse_reverse(cmd, buffer)) { return ERROR; }
        return REVERSE;
    case 's':
        if (parse_gate(cmd, buffer)) { return ERROR; }
        return GATE;
    default:
        return ERROR;
    }
}
