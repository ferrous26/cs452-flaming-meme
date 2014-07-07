
#ifndef __DIJKSTRA_H__
#define __DIJKSTRA_H__

#include <track_node.h>

#define MAX_ROUTING_TIME_ESTIMATE 5

int dijkstra(const track_node* const track,
             const track_node* const start,
             const track_node* const end,
             path_node* const path);

#endif
