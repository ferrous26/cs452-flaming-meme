#include <stdio.h>
#include <syscall.h>

static inline char*
_sprintf_uint(char* buffer, const uint num)
{
    // handle the 0 case specially
    if (!num) return sprintf_char(buffer, '0');

    // build the string in reverse order to be used like a stack
    uint    i = 0;
    uint  nom = num;
    uint8 noms[10];

    for (; i < 10; i++) {
	noms[i] = (uint8)(nom % 10);
	nom    -= noms[i];
	nom    /= 10;
    }

    // pop the stack contents, printing if we need to
    bool started = false;
    for (i = 9; i < 128; i--)
	if (noms[i] || started) {
	    started = true;
	    buffer = sprintf_char(buffer, '0' + noms[i]);
	}

    return buffer;
}

char* sprintf_uint(char* buffer, const uint32 num)
{
    return _sprintf_uint(buffer, num);
}

static inline char* _sprintf_int(char* buffer, const int32 num)
{
    // turn the negative case into a positive case
    if (num < 0) {
	buffer = sprintf_char(buffer, '-');
	return _sprintf_uint(buffer, (const uint32)-num);
    }

    return _sprintf_uint(buffer, (const uint32)num);
}

char* sprintf_int(char* buffer, const int32 num)
{
    return _sprintf_int(buffer, num);
}

static inline char*
_sprintf_hex(char* buffer, const uint num)
{
    buffer = sprintf_char(buffer, '0');
    buffer = sprintf_char(buffer, 'x');

    if (!num)
	return sprintf_char(buffer, '0');

    // build the string in reverse order to be used like a stack
    uint  i = 0;
    uint8 noms[8];
    uint  nom = num;

    for (; i < 8; i++) {
	noms[i] = nom & 0xf; // take the lower four bits
	nom     = nom >> 4;  // shift last three off the end
    }

    // pop the stack contents, printing if we need to
    bool started = false;
    for (i = 7; i < 128; i--) {
	if (noms[i] || started) {
	    started = true;
	    if (noms[i] > 9)
		buffer = sprintf_char(buffer, (noms[i] - 10) + 'A');
	    else
		buffer = sprintf_char(buffer, noms[i] + '0');
	}
    }

    return buffer;
}

char* sprintf_hex(char* buffer, const uint num)
{
    return _sprintf_hex(buffer, num);
}

static inline char*
_sprintf_ptr(char* buffer, const void* const pointer)
{
    buffer = sprintf_char(buffer, '0');
    buffer = sprintf_char(buffer, 'x');

    // build the string in reverse order to be used like a stack
    uint  i = 0;
    uint8 noms[8];
    uint  ptr = (uint)pointer;

    for (; i < 8; i++) {
	noms[i] = ptr & 0xf; // take the lower four bits
	ptr     = ptr >> 4;  // shift last four off the end
    }

    // pop the stack contents, printing everything
    for (i = 7; i < 128; i--) {
	if (noms[i] > 9)
	    buffer = sprintf_char(buffer, (noms[i] - 10) + 'A');
	else
	    buffer = sprintf_char(buffer, noms[i] + '0');
    }

    return buffer;
}

char* sprintf_ptr(char* buffer, const void* const pointer)
{
    return _sprintf_ptr(buffer, pointer);
}

static inline char*
_sprintf_string(char* buffer, const char* str)
{
    while (*str)
	buffer = sprintf_char(buffer, *str++);
    return buffer;
}

char* sprintf_string(char* buffer, const char* str)
{
    return _sprintf_string(buffer, str);
}

static inline char*
_sprintf_va(char* buffer, const char* fmt, va_list args)
{
    char token = *fmt;
    while (token) {
	if (token == '%') {
	    fmt++;
	    token = *fmt;

	    assert(token == '%' || token == 'c' || token == 'd' ||
		   token == 'p' || token == 's' || token == 'u' ||
		   token == 'x',
                   "Unknown format type `%%%c`", token);

	    switch (token) {
	    case '%':
		buffer = sprintf_char(buffer, token);
		break;
	    case 'c': // character
		buffer = sprintf_char(buffer, (const char)va_arg(args, int));
		break;
	    case 'd': // decimal (signed)
		buffer = _sprintf_int(buffer, va_arg(args, int32));
		break;
	    case 'p': // pointer
		buffer = _sprintf_ptr(buffer, va_arg(args, void*));
		break;
	    case 's': // string
		// recursion feels like it should be the correct solution
		// but it would be a security flaw (and also less efficient)
		buffer = _sprintf_string(buffer, va_arg(args, char*));
		break;
	    case 'u': // decimal (unsigned)
		buffer = _sprintf_uint(buffer, va_arg(args, uint32));
		break;
	    case 'x': // hex (unsigned)
		buffer = _sprintf_hex(buffer, va_arg(args, uint32));
		break;
	    }
	}
	else {
	    buffer = sprintf_char(buffer, token);
	}

	fmt++;
	token = *fmt;
    }

    return buffer;
}

char* sprintf_va(char* buffer, const char* fmt, va_list args)
{
    return _sprintf_va(buffer, fmt, args);
}

char* sprintf(char* buffer, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    buffer = _sprintf_va(buffer,fmt, args);
    va_end(args);
    return buffer;
}
