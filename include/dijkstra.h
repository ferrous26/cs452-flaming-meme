
#ifndef __DIJKSTRA_H__
#define __DIJKSTRA_H__

#include <track_node.h>

int dijkstra(const track_node* const track,
             const track_node* const start,
             const track_node* const end,
             path_node* const path);

#endif

