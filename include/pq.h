
#ifndef __PRIORITY_QUEUE_H__
#define __PRIORITY_QUEUE_H__

typedef struct {
    int key;
    int val;
} pq_node;

typedef struct {
    int      count;
    int      size;
    pq_node* heap;
} priority_queue;


static inline int __attribute__((always_inline, used))
pq_peek_key(priority_queue* q) { return q->heap[1].key; }

static inline int __attribute__((always_inline, used))
pq_peek_val(priority_queue* q) { return q->heap[1].val; }

void pq_init(priority_queue* const q, pq_node* const heap, const int size);
void pq_add(priority_queue* const q, const int key, const int value);
int  pq_delete(priority_queue* const q);

#endif

