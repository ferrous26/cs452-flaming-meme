
#include <pq.h>
#include <path.h>
#include <vt100.h>
#include <debug.h>
#include <track_data.h>

#include <dijkstra.h>

#define HEAP_SIZE (TRACK_MAX+1)

int dijkstra(const track_node* const track,
             const track_node* const start,
             const track_node* const end,
             path_node* const path) {

    struct {
        int dist;
        int dir;
        const track_node* prev;
    } data[TRACK_MAX];

    pq_node        heap[HEAP_SIZE];
    priority_queue q;

    pq_init(&q, heap, HEAP_SIZE);

    const track_node* ptr = track;
    for (int i = 0; i < TRACK_MAX; i++, ptr++) {
        if (ptr == start || ptr == start->reverse) {
            data[i].prev = start;
            data[i].dist = 0;
            data[i].dir  = 0;
            pq_add(&q, 0, (int)ptr);
        } else if (ptr == start->reverse) {
            data[i].prev = start->reverse;
            data[i].dist = 0;
            data[i].dir  = 0;
            pq_add(&q, 0, (int)ptr);
        } else {
            data[i].prev = NULL;
            data[i].dist = INT_MAX;
            pq_add(&q, INT_MAX, (int)ptr);
        }
    }

    int direction = 0;
    while (1) {
        const int curr_dist = pq_peek_key(&q);
        if (INT_MAX == curr_dist) {
            log("NO PATH EXISTS!\n");
            return -1;
        }
        
        ptr = (track_node*) pq_delete(&q);
        if (end == ptr) break;
        
        else if (end == ptr->reverse) {
            const int nxt_off = ptr->reverse - track;
            assert(nxt_off > 0 && nxt_off < TRACK_MAX,
                    "track calculation came across invalid node");

            data[nxt_off].dist = curr_dist;
            data[nxt_off].prev = ptr;
            direction = 3;

            ptr = ptr->reverse;
            break;
        }

        switch (ptr->type) {
        case NODE_ENTER:
        case NODE_SENSOR: {
            track_node* const nxt_ptr = ptr->edge[DIR_AHEAD].dest;
            const int nxt_dist        = curr_dist + ptr->edge[DIR_AHEAD].dist;
            const int nxt_off         = nxt_ptr - track;

            if (data[nxt_off].dist > nxt_dist) {
                data[nxt_off].dist = nxt_dist;
                data[nxt_off].prev = ptr;
                data[nxt_off].dir  = 0;

                pq_raise(&q, (int)nxt_ptr, nxt_dist);
            }
            break;
        }
        case NODE_MERGE: {
            track_node* const nxt_ptr = ptr->edge[DIR_AHEAD].dest;
            const int nxt_dist        = curr_dist + ptr->edge[DIR_AHEAD].dist;
            const int nxt_off         = nxt_ptr - track;

            if (data[nxt_off].dist > nxt_dist) {
                data[nxt_off].dist = nxt_dist;
                data[nxt_off].prev = ptr;
                data[nxt_off].dir  = 0;

                pq_raise(&q, (int)nxt_ptr, nxt_dist);
            }
            // TODO: add the reverse direction node
            break;
        }
        case NODE_BRANCH:
            for (int i = 0; i < 2; i++) {
                track_node* const nxt_ptr = ptr->edge[i].dest;
                const int nxt_dist        = curr_dist + ptr->edge[i].dist;
                const int nxt_off         = nxt_ptr - track;

                if (data[nxt_off].dist > nxt_dist) {
                    data[nxt_off].dist = nxt_dist;
                    data[nxt_off].prev = ptr;
                    data[nxt_off].dir  = i;

                    pq_raise(&q, (int)nxt_ptr, nxt_dist);
                }
            }
            break;
        case NODE_NONE:
        case NODE_EXIT:
            break;
        }
    }

    int path_size = 0;
    FOREVER {
        path_node* const node = &path[path_size];
        const int offset = ptr - track;

        switch(ptr->type) {
        case NODE_NONE:
        case NODE_EXIT:
        case NODE_ENTER:
            break;
        case NODE_SENSOR:
            if (direction == 3) {
                node->type = PATH_REVERSE;
                node->dist = data[offset].dist;
            } else {
                node->type           = PATH_SENSOR;
                node->dist           = data[offset].dist;
                node->data.int_value = ptr->num; 
            }
            path_size++;
            break;
        case NODE_BRANCH:
            node->type             = PATH_TURNOUT;
            node->dist             = data[offset].dist;
            node->data.turnout.num = ptr->num;
            node->data.turnout.dir = direction ? 'C' : 'S';
            path_size++;
            break;
        case NODE_MERGE:
            // possibly reverse through
            break;
        }
        
        direction = data[offset].dir;
        ptr       = data[offset].prev;
        if (data[ptr-track].prev == ptr) {
            if (ptr != start) {
                path[path_size].type = PATH_REVERSE;
                path[path_size].dist = data[offset].dist;
                path_size++;
            }
            break;
        }
    }

    return path_size;
}
