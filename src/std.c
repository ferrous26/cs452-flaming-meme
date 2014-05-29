#include <std.h>

int abs(const int val) {
    return val < 0 ? -val : val;
}

bool isspace(const char c) {
    switch( c ) {
    case ' ':
    case '\t':
    case '\n':
    case '\v':
    case '\f':
    case '\r':
        return 1;
    default:
        return 0;
    }
}

bool isdigit(const char c) {
    return c <= '9' && c >= '0';
}

bool ishexdigit(const char c) {
    return isdigit( c ) || ( c >= 'a' && c <= 'f' ) || ( c >= 'A' && c <= 'F' );
}
