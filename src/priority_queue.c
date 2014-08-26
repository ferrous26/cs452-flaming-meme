#include <priority_queue.h>
#include <syscall.h>

#define LEFT(i)   (i << 1)
#define RIGHT(i)  (LEFT(i) + 1)
#define PARENT(i) (i >> 1)

void pq_init(priority_queue* const q, pq_node* const heap, const int size) {
    q->count = 0;
    q->size  = size;
    q->heap  = heap;

    for (int i = 0; i < size; i++) {
        q->heap[i].key = INT_MAX;
        q->heap[i].val = -1;
    }
}

int pq_delete(priority_queue* const q) {
    assert(pq_size(q) > 0, "[%d] trying to delete from empty queue", myTid());

    int data            = pq_peek_val(q);
    q->count           -= 1;
    int curr            = q->count;
    pq_node* const node = q->heap;


    // take butt and put it on head
    node[0]        = node[curr];
    node[curr].key = INT_MAX; // mark the end
    node[curr].val = -1;

    // now, we need to bubble the head down
    curr = 0;

    // bubble down until we can bubble no more
    FOREVER {
        const int left  = LEFT(curr);
        if (left >= q->size) break;

        const int right = RIGHT(curr);

        int smallest = curr;
        if (node[left].key  < node[curr].key)     smallest = left;
        if (node[right].key < node[smallest].key) smallest = right;

        // if no swapping needed, then break
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
        curr         = parent;
        parent       = PARENT(curr);
    }
}

static inline int __attribute__((pure))
find_val(const priority_queue* const q, const int val) {
    for (int i = 0; i < q->count; i++) {
        if (q->heap[i].val == val) return i;
    }
    return -1;
}

void pq_raise(priority_queue* const q, const int value, const int new_key) {
    pq_node* const node = q->heap;
    int curr            = find_val(q, value);

    assert(curr >= 0, "data couldn't be found within the queue");
    assert(node[curr].key >= new_key,
           "Prioirity queue can't raise priority low a lower priority");

    int parent     = PARENT(curr);
    node[curr].key = new_key;

    while (node[curr].key < node[parent].key && curr > 0) {
        pq_node temp = node[curr];
        node[curr]   = node[parent];
        node[parent] = temp;

        curr   = parent;
        parent = PARENT(curr);
    }
}
