
#ifndef __DIJKSTRA_H__
#define __DIJKSTRA_H__

#include <track_node.h>

#define MAX_ROUTING_TIME_ESTIMATE 5

typedef struct {
    const track_node* const start;
    const track_node* const end;
    const int reserve_dist;

    const int train_offset;
    const int allow_short_move;
    const int allow_start_reverse;
    const int allow_approach_back;
} path_requisition;

int dijkstra(const track_node* const track,
             const path_requisition* const opts,
             path_node* const path);

#endif
