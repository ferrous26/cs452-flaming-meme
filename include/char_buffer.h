
#ifndef __CHAR_BUFFER_H__
#define __CHAR_BUFFER_H__

#include <std.h>

/**
 * A circular buffer is simply an array buffer which wraps around itself
 * after getting to the end.
 *
 * This implementation provides no safety from overwriting previous
 * values which have not yet been consumed. It is up to the client of
 * this library to ensure that the size of the buffer is sufficient for
 * avoiding this problem (if necessary).
 *
 * @assert The implementation of this data structure is so simple that
 *         it is always worth inlining.
 *
 * @note It is _required_ to choose a buffer length that is a power of
 *       2 as the compiler can better optimize access, and also because
 *       the implementation will break otherwise.
 *
 *  char*  head;     // pointer to next producer slot
 *  char*  tail;     // pointer to next consumer slot
 *  char*   end;     // pointer to first address past the end of the buffer
 *  int   count;     // current usage size of the buffer
 *  char buffer[n];  // the actual data (and pointer to the start)
 */

#define CHAR_BUFFER(n)							\
typedef struct {							\
    char* head;								\
    char* tail;								\
    char* end;								\
    uint  count;							\
    char  buffer[n];							\
} char_buffer;								\
									\
static inline void cbuf_init(char_buffer* const cb) {			\
    cb->head  = cb->tail = cb->buffer;					\
    cb->end   = cb->buffer + n;						\
    cb->count = 0;							\
    memset(cb->buffer, 0, n);						\
}									\
									\
static inline uint cbuf_count(const char_buffer* const cb) {		\
    return cb->count;							\
}									\
									\
static inline void cbuf_produce(char_buffer* const cb, const char c) {	\
    *cb->head++ = c;							\
									\
    cb->count = mod2(cb->count + 1, n);					\
    if (cb->head == cb->end)						\
	cb->head = cb->buffer;						\
}									\
									\
static inline char cbuf_consume(char_buffer* const cb) {		\
    assert(cb->count, "Trying to consume from empty char queue");	\
									\
    const char c = *cb->tail++;						\
									\
    cb->count--;							\
    if (cb->tail == cb->end)						\
	cb->tail = cb->buffer;						\
									\
    return c;								\
}

#define BULK_CHAR_BUFFER(n)						\
    CHAR_BUFFER(n)							\
									\
static inline void cbuf_bulk_produce(char_buffer* const cb,		\
				     const char* const c,		\
				     const uint length) {		\
    if ((cb->head + length) > cb->end) {				\
	uint chunk1 = (uint)(cb->end - cb->head);			\
	uint chunk2 = length         - chunk1;				\
	memcpy(cb->head,   c,          chunk1);				\
	memcpy(cb->buffer, c + chunk1, chunk2);				\
	cb->head = cb->buffer + chunk2;					\
    }									\
    else {								\
	memcpy(cb->head, c, length);					\
	cb->head += length;						\
    }									\
									\
    cb->count = mod2(cb->count + 1, n);					\
}									\
									\


#endif
