#include <ui.h>
#include <stdio.h>

char* ui_pad(char* ptr, const size_t input_width, const size_t total_width) {

    const int count = input_width == 0 ?
        total_width - 1 : total_width - input_width;

    for (int i = 0; i < count; i++)
        ptr = sprintf_char(ptr, ' ');
    return ptr;
}

char ui_twirl(const char prev) {
    switch (prev) {
    case  '/': return '-';
    case  '-': return '\\';
    case '\\': return '|';
    case  '|': return '/';
    }
    return '|';
}
