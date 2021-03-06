#ifndef __PRIORITY_QUEUE_H__
#define __PRIORITY_QUEUE_H__

#include <std.h>

typedef struct {
    int key;
    int val;
} pq_node;

typedef struct {
    int      count;
    int      size;
    pq_node* heap;
} priority_queue;

// TODO: fix the heap[1] hack...fuuuuuuu Nik
static inline int __attribute__((always_inline, const, used))
pq_peek_key(const priority_queue* const q) { return q->heap[0].key; }

static inline int __attribute__((always_inline, const, used))
pq_peek_val(const priority_queue* const q) { return q->heap[0].val; }

static inline int __attribute__((always_inline, const, used))
pq_size(const priority_queue* const q) { return q->count; }

void pq_init(priority_queue* const q, pq_node* const heap, const int size);
void pq_add(priority_queue* const q, const int key, const int value);
int  pq_delete(priority_queue* const q);
void pq_raise(priority_queue* const q, const int value, const int new_key);

#endif
