#include <circular_buffer.h>

#define CB_INIT					\
  cb->cb.head   = 0;				\
  cb->cb.tail   = 0;				\
  cb->cb.length = length;			\
  cb->buffer    = buffer;

#define CB_PRODUCE				\
  size_t ptr = cb->cb.head;			\
  cb->buffer[ptr] = value;			\
  cb->cb.head = (ptr + 1) % cb->cb.length;

#define CB_CAN_CONSUME				\
  return (bool)(cb->cb.head != cb->cb.tail);

#define CB_CONSUME(type)			\
  size ptr    = cb->cb.tail;			\
  type value  = cb->buffer[ptr];		\
  cb->cb.tail = (ptr + 1) % cb->cb.length;	\
  return value;


void cbuf_init(char_buffer* const cb,
	       const size         length,
	       const char* const  buffer)                  { CB_INIT; }
void cbuf_produce(char_buffer* const cb, const char value) { CB_PRODUCE; }
bool cbuf_can_consume(const char_buffer* const cb)         { CB_CAN_CONSUME; }
char cbuf_consume(char_buffer* const cb)                   { CB_CONSUME(char); }


void ibuf_init(size_buffer* const  cb,
	       const size          length,
	       const uint32* const buffer)                   { CB_INIT; }
void ibuf_produce(char_buffer* const cb, const uint32 value) { CB_PRODUCE; }
bool ibuf_can_consume(size_buffer* const cb)                 { CB_CAN_CONSUME; }
char ibuf_consume(char_buffer* const cb)                     { CB_CONSUME(uint32); }
