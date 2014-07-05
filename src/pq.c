
#include <std.h>
#include <debug.h>
#include <syscall.h>

#include <pq.h>

#define LEFT(i)   (i << 1)
#define RIGHT(i)  (LEFT(i) + 1)
#define PARENT(i) (i >> 1)

#define CPY_NODE(SRC, DEST) \
    DEST.key = SRC.key;     \
    DEST.val = SRC.val

void pq_init(priority_queue* const q, pq_node* const heap, const int size) {
    q->count = 1;
    q->size  = size;
    q->heap  = heap;

    q->heap[0].key = 0;
    q->heap[0].val = 0;

    for (int i = 1; i < size; i++) {
        q->heap[i].key = INT_MAX;
        q->heap[i].val = -1;
    }
}

int pq_delete(priority_queue* const q) {
    int data            = q->heap[1].val;
    int curr            = --q->count;
    pq_node* const node = q->heap;

    // take butt and put it on head
    node[1]        = node[curr];
    node[curr].key = INT_MAX; // mark the end

    // now, we need to bubble the head down
    curr = 1;

    // bubble down until we can bubble no more
    FOREVER {
        const int left     = LEFT(curr);
        const int right    = RIGHT(curr);

        int smallest = curr;
        if (node[left].key  < node[curr].key)     smallest = left;
        if (node[right].key < node[smallest].key) smallest = right;

        // if no swapping needed, then brake
        if (smallest == curr) break;

        // else, swap and prepare for next iteration
        const pq_node snode = node[smallest];
        node[smallest]      = node[curr];
        node[curr]          = snode;
        curr                = smallest;
    }

    return data;
}

void pq_add(priority_queue* const q, const int key, const int value) {
    assert(q->count < q->size, "[%d] Priority Queue has been overflown %d %d",
           myTid(), key, value);

    pq_node* const node = q->heap;
    int curr            = q->count++;
    int parent          = PARENT(curr);

    node[curr].key      = key;
    node[curr].val      = value;

    while (node[curr].key < node[parent].key) {
        pq_node temp = node[curr];
        node[curr]   = node[parent];
        node[parent] = temp;

        curr   = parent;
        parent = PARENT(curr);
    }
}

