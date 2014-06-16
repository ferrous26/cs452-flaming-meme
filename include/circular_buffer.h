
#ifndef __CIRCULAR_BUFFER_H__
#define __CIRCULAR_BUFFER_H__

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
 * @note It is _required_ to choose a buffer length that is a power of
 *       2 as the compiler can better optimize access, and also because
 *       the implementation will break otherwise.
 */
struct circular_buffer {
    size head;     // index of next producer slot
    size tail;     // index of next consumer slot
    size length;   // total length of the buffer
    size count;    // current size of the buffer
};


/**
 * Generic implementation for all circular buffers
 */
#define CB_INIT(type)				\
  cb->cb.head   = 0;				\
  cb->cb.tail   = 0;				\
  cb->cb.length = length;			\
  cb->cb.count  = 0;				\
  cb->buffer    = (type*)buffer;

#define CB_PRODUCE						\
    size ptr = cb->cb.head;					\
    cb->buffer[ptr] = value;					\
    cb->cb.count = mod2(cb->cb.count + 1, cb->cb.length);	\
    cb->cb.head  = mod2(ptr + 1, cb->cb.length);

#define CB_CAN_CONSUME				\
    return (bool)cb->cb.count;

#define CB_CONSUME(type)			\
  size ptr    = cb->cb.tail;			\
  type value  = cb->buffer[ptr];		\
  cb->cb.tail = mod2(ptr + 1, cb->cb.length);	\
  cb->cb.count--;				\
  return value;

#define CB_COUNT				\
    return cb->cb.count

/**
 * A circular buffer specialized for type char
 */
typedef struct circular_buffer_char {
    struct circular_buffer cb;
    char* buffer;
} char_buffer;

static inline void cbuf_init(char_buffer* const cb,
			     const size         length,
			     const char* const  buffer)                  { CB_INIT(char); }
static inline void cbuf_produce(char_buffer* const cb, const char value) { CB_PRODUCE; }
static inline bool cbuf_can_consume(const char_buffer* const cb)         { CB_CAN_CONSUME; }
static inline char cbuf_consume(char_buffer* const cb)                   { CB_CONSUME(char); }
static inline size cbuf_count(const char_buffer* const cb)               { CB_COUNT; }


/**
 * A circular buffer specialized for type uint32
 */
typedef struct circular_buffer_uint32 {
    struct circular_buffer cb;
    uint32* buffer;
} uint_buffer;

static inline void ibuf_init(uint_buffer* const  cb,
			     const size          length,
			     const uint32* const buffer)                   { CB_INIT(uint32); }
static inline void ibuf_produce(uint_buffer* const cb, const uint32 value) { CB_PRODUCE; }
static inline bool ibuf_can_consume(const uint_buffer* const cb)           { CB_CAN_CONSUME; }
static inline uint32 ibuf_consume(uint_buffer* const cb)                   { CB_CONSUME(uint32); }
static inline size   ibuf_count(const uint_buffer* const cb)               { CB_COUNT; }

#endif
