
#ifndef __TRACK_NODE_H__
#define __TRACK_NODE_H__

typedef enum {
  NODE_NONE,
  NODE_SENSOR,
  NODE_BRANCH,
  NODE_MERGE,
  NODE_ENTER,
  NODE_EXIT,
} node_type;

#define DIR_AHEAD 0
#define DIR_STRAIGHT 0
#define DIR_CURVED 1

struct track_node;
typedef struct track_node track_node;
typedef struct track_edge track_edge;

typedef enum {
    TRACK_STRAIGHT,
    TRACK_COMBO,
    TRACK_CURVED,
    TRACK_TYPE_COUNT
} track_type;

struct track_edge {
    track_edge* reverse;
    track_node* src;
    track_node* dest;
    int         dist;     /* in millimetres */
    //    edge_type   type;
};

struct track_node {
    const char* name;
    node_type type;
    int num;              /* sensor or switch number */
    track_node* reverse;  /* same location, but opposite direction */
    track_edge  edge[2];
};

#endif
