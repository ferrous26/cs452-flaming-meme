
#include <pq.h>
#include <path.h>
#include <vt100.h>
#include <debug.h>
#include <track_data.h>

#include <dijkstra.h>
#include <char_buffer.h>
#include <tasks/track_reservation.h>

typedef void* vptr; //needed to handle the type_buffer properly

#define RESERVE_DIST 
#define HEAP_SIZE    (TRACK_MAX+1)
TYPE_BUFFER(vptr, HEAP_SIZE);



int dijkstra(const track_node* const track,
             const path_requisition* const opts,
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
        if (ptr == opts->start) {
            data[i].prev = opts->start;
            data[i].dist = 0;
            data[i].dir  = 0;
            pq_add(&q, 0, (int)ptr);
        } else if (opts->allow_start_reverse && ptr == opts->start->reverse) {
            data[i].prev = opts->start->reverse;
            data[i].dist = 0;
            data[i].dir  = 0;
            pq_add(&q, 0, (int)ptr);
        } else {
            data[i].prev = NULL;
            data[i].dist = INT_MAX;
            pq_add(&q, INT_MAX, (int)ptr);
        }
    }

    const int total_reserve = opts->reserve_dist;
    int reserve             = total_reserve;
    int direction           = 0;

    while (1) {
        const int curr_dist = pq_peek_key(&q);
        if (INT_MAX == curr_dist) {
            log("NO PATH EXISTS!\n");
            return -1;
        }
        
        ptr = (track_node*) pq_delete(&q);
        if (reserve > 0) {
            const int own_f = reserve_who_owns(ptr, DIR_FORWARD);
            const int own_r = reserve_who_owns(ptr, DIR_REVERSE);

            reserve = total_reserve - curr_dist;
            if ( own_f != -1
               &&own_r != -1
               &&own_f != opts->train_offset
               &&own_r != opts->train_offset) {
                log("Reservation in way of %d finding path around %d-%d",
                    opts->train_offset, own_r, own_f);
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

    FOREVER {
        path_node* const node  = &path[path_size];
        const int offset       = path_ptr - track;
        
        const int node_dist    = data[offset].dist;
        const track_node* next = data[offset].prev;
        const int next_offset  = next - track;

        if (total_reserve - data[next_offset].dist > 0) {
            if (reserve_section(path_ptr, DIR_FORWARD, opts->train_offset)
                || reserve_section(path_ptr, DIR_REVERSE, opts->train_offset)) {

                while (ptr != path_ptr) {
                    reserve_release(ptr, DIR_FORWARD, opts->train_offset);
                    reserve_release(ptr, DIR_REVERSE, opts->train_offset);
                    ptr = data[ptr - track].prev;
                }

                return -100;
            }
        }

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
        path_ptr  = next;
        direction = data[offset].dir;
    }

    if (path_ptr != opts->start) {
        path[path_size].type = PATH_REVERSE;
        // since this is the first command in the path, must be 0
        path[path_size].dist = 0;
        path_size++;
    }

    return path_size;
}
