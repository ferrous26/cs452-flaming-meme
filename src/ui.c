#include <ui.h>
#include <vt100.h>

char* ui_pad(char* ptr, const int input_width, const int total_width) {
    const int count = input_width == 0 ?
        total_width - 1 : total_width - input_width;

    for (int i = 0; i < count; i++)
        ptr = sprintf_char(ptr, ' ');
    return ptr;
}
