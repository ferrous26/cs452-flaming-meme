
#include <pq.h>
#include <path.h>
#include <vt100.h>
#include <debug.h>
#include <track_data.h>

#include <dijkstra.h>
#include <char_buffer.h>
#include <tasks/track_reservation.h>

#define RESERVE_DIST 
#define HEAP_SIZE    (TRACK_MAX+1)

typedef struct dijkstra_data {
    int dist;
    int dir;
    const track_node* prev;
    const track_node* next;
} data_t;


inline static void prime_search(const path_requisition* const opts,
                                const track_node* ptr,
                                data_t data[HEAP_SIZE],
                                priority_queue* const q) {

    for (int i = 0; i < TRACK_MAX; i++, ptr++) {
        if (ptr == opts->start) {
            data[i].prev = opts->start;
            data[i].dist = 0;
            data[i].dir  = 0;
            pq_add(q, 0, (int)ptr);
        } else if (opts->allow_start_reverse && ptr == opts->start->reverse) {
            data[i].prev = opts->start->reverse;
            data[i].dist = 0;
            data[i].dir  = 0;
            pq_add(q, 0, (int)ptr);
        } else {
            data[i].prev = NULL;
            data[i].dist = INT_MAX;
            pq_add(q, INT_MAX, (int)ptr);
        }
    }
}

int dijkstra(const track_node* const track,
             const path_requisition* const opts,
             path_node* const path,
             int* const reserved_dist) {

    data_t            data[HEAP_SIZE];
    pq_node           heap[HEAP_SIZE];
    priority_queue    q;
    const track_node* ptr;
    const int         total_reserve = opts->reserve_dist;
    int               reserve       = total_reserve;
    int direction                   = 0;
    
    pq_init(&q, heap, HEAP_SIZE);
    prime_search(opts, track, data, &q);
    
    FOREVER {
        const int curr_dist = pq_peek_key(&q);
        if (INT_MAX == curr_dist) {
            log("NO PATH EXISTS!\n");
            return -1;
        }
        
        ptr = (track_node*) pq_delete(&q);
        if (reserve > 0) {
            reserve = total_reserve - curr_dist;
            if(!reserve_can_double(ptr, opts->train_offset)) {
                log("Reservation in way of %d finding path around %s",
                    opts->train_offset, ptr->name);
                continue;
            }
        }

        if (opts->end == ptr) break;
        else if (opts->allow_approach_back && opts->end == ptr->reverse) {
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
        case NODE_NONE:
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
        }   break;

        case NODE_MERGE: {
            const track_node* const nxt_ptr = ptr->edge[DIR_AHEAD].dest;
            const int nxt_dist              = curr_dist + ptr->edge[DIR_AHEAD].dist;
            const int nxt_off               = nxt_ptr - track;

            if (data[nxt_off].dist > nxt_dist) {
                data[nxt_off].dist = nxt_dist;
                data[nxt_off].prev = ptr;
                data[nxt_off].dir  = 0;

                pq_raise(&q, (int)nxt_ptr, nxt_dist);
            }

            const track_node* const rev_ptr = ptr->reverse;
            const int rev_off               = rev_ptr - track;

            if (opts->allow_short_move && data[rev_off].dist > curr_dist) {
                data[rev_off].dist = curr_dist;
                data[rev_off].prev = ptr;
                data[rev_off].dir  = 3;

                pq_raise(&q, (int)rev_ptr, curr_dist);
            }
        }   break;

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


        case NODE_EXIT:
            break;
        }
    }

    int path_size = 0;
    const track_node* path_ptr = ptr; 
    data[path_ptr-track].next = path_ptr;
    

    FOREVER {
        path_node* const node  = &path[path_size];
        const int offset       = path_ptr - track;
        
        const int node_dist    = data[offset].dist;
        const track_node* next = data[offset].prev;
        const int next_offset  = next - track;

        switch(path_ptr->type) {
        case NODE_NONE:
        case NODE_EXIT:
        case NODE_ENTER: 
            break;
        
        case NODE_SENSOR:
            node->type           = PATH_SENSOR;
            node->dist           = node_dist;
            node->data.int_value = path_ptr->num;
            path_size++;

            if (direction == 3) {
                path[path_size].type = PATH_REVERSE;
                path[path_size].dist = node_dist;
                path_size++;
            }
            break;

        case NODE_BRANCH:
            node->type             = PATH_TURNOUT;
            node->dist             = node_dist;
            node->data.turnout.num = path_ptr->num;
            node->data.turnout.dir = direction ? 'C' : 'S';
            path_size++;
            break;

        case NODE_MERGE:
            if (direction == 3) {
                path[path_size].type = PATH_REVERSE;
                path[path_size].dist = node_dist;
                path_size++;
            }
            break;
        }

        if (next == path_ptr) break;
        data[next_offset].next = path_ptr;
        path_ptr               = next;
        direction              = data[offset].dir;
    }

    ptr = path_ptr;

    *reserved_dist = 0;
    while (*reserved_dist < total_reserve) {
        const int index = path_ptr - track;

        const int res_2 =
            reserve_section(path_ptr, DIR_REVERSE, opts->train_offset);
        const int res_1 = 
            reserve_section(path_ptr, DIR_FORWARD, opts->train_offset);

        while (res_1 < 0 || res_2 < 0) {
            reserve_release(ptr, DIR_FORWARD, opts->train_offset);
            reserve_release(ptr, DIR_REVERSE, opts->train_offset);
            if (ptr == path_ptr) return -100;
            ptr = data[ptr - track].next;
        }
        *reserved_dist += res_1;
        *reserved_dist += res_2;

        if (data[index].next == path_ptr) break;
        path_ptr = data[index].next;
    }

    if (path_ptr != opts->start) {
        path[path_size].type = PATH_REVERSE;
        // since this is the first command in the path, must be 0
        path[path_size].dist = 0;
        path_size++;
    }

    return path_size;
}

