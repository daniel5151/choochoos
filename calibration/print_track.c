// compile with
// gcc -o print_track print_track.c ../src/assignments/t1/track_data_new.c
#include "../src/assignments/t1/track_data_new.h"
#include "stdio.h"

void print_edge(const track_edge* e) {
    printf("EDGE src=%s dest=%s dist=%d\n", e->src->name, e->dest->name,
           e->dist);
}

int main() {
    track_node track[TRACK_MAX];
    // change to init_trackb if you want track b
    init_tracka(track);
    for (size_t i = 0; i < TRACK_MAX; i++) {
        const track_node* n = &track[i];
        if (n->type == NODE_NONE) continue;
        printf("NODE name=%s type=%c num=%d rev=%s\n", n->name, (char)n->type,
               n->num, n->reverse->name);
        if (n->edge[0].src != 0) {
            print_edge(&n->edge[0]);
            print_edge(n->edge[0].reverse);
        }
        if (n->edge[1].src != 0) {
            print_edge(&n->edge[1]);
            print_edge(n->edge[1].reverse);
        }
    }
}
