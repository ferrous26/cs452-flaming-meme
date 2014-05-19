#include <std.h>

/**
 * A circular buffer is simply an array buffer which wraps around
 * itself after getting to the end.
 *
 * This implementation provides no safety from overwriting previous
 * values which have not yet been consumed. It is up to the client
 * of this library to ensure that the size of the buffer is sufficient
 * for avoiding this problem (if necessary).
 *
 * @note A specialized struct is exactly one cache line in size and
 *       the buffer should be located directly after the buffer in
 *       memory. These properties make it easier for the compiler and
 *       hardware to optimize access.
 * @note It is beneficial to choose a buffer length that is a power of
 *       2 as the compiler can better optimize access.
 */
struct circular_buffer {
    size head;     // index of next producer slot
    size tail;     // index of next consumer slot
    size length;   // total length of the buffer
};

/** A circular buffer specialized for type char */
typedef struct circular_buffer_char {
    struct circular_buffer cb;
    char* buffer;
} char_buffer;

/** A circular buffer specialized for type uint32 */
typedef struct circular_buffer_uint32 {
    struct circular_buffer cb;
    uint32* buffer;
} uint_buffer;

void cbuf_init(char_buffer* const  cb,
	       const size          length,
	       const char* const   buffer);
void ibuf_init(uint_buffer* const  cb,
	       const size          length,
	       const uint32* const buffer);

void cbuf_produce(char_buffer*  const cb, const char   value);
void ibuf_produce(uint_buffer*  const cb, const uint32 value);

bool cbuf_can_consume(const char_buffer*  const cb);
bool ibuf_can_consume(const uint_buffer*  const cb);

char   cbuf_consume(char_buffer*  const cb);
uint32 ibuf_consume(uint_buffer*  const cb);
