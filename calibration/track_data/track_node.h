typedef enum {
    NODE_NONE = 0,
    NODE_SENSOR = 's',
    NODE_BRANCH = 'b',
    NODE_MERGE = 'm',
    NODE_ENTER = 'n',
    NODE_EXIT = 'x',
} node_type;

#define DIR_AHEAD 0
#define DIR_STRAIGHT 0
#define DIR_CURVED 1

struct track_node;
typedef struct track_node track_node;
typedef struct track_edge track_edge;

struct track_edge {
    track_edge* reverse;
    track_node *src, *dest;
    int dist; /* in millimetres */
};

struct track_node {
    const char* name;
    node_type type;
    int num;             /* sensor or switch number */
    track_node* reverse; /* same location, but opposite direction */
    track_edge edge[2];
};
