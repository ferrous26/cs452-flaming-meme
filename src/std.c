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
    return isdigit( c )
        || ( c >= 'a' && c <= 'f' )
        || ( c >= 'A' && c <= 'F' );
}

int log10(int c) {
    if (c == 0) return 0;
    if (c < 10) return 1;
    if (c < 100) return 2;
    if (c < 1000) return 3;
    if (c < 10000) return 4;
    if (c < 100000) return 5;
    if (c < 1000000) return 6;
    if (c < 10000000) return 7;
    if (c < 100000000) return 8;
    if (c < 1000000000) return 9;
    return 10;
}
