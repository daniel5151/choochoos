#include "track_graph.h"

#include "user/debug.h"

static constexpr size_t MAX_ITERS = 20;

TrackGraph::TrackGraph(Marklin::Track t) {
    switch (t) {
        case Marklin::Track::A:
            init_tracka(track);
            break;
        case Marklin::Track::B:
            init_trackb(track);
            break;
        default:
            assert(false);
    }
}

inline static bool node_is_sensor(const track_node& node,
                                  const Marklin::sensor_t& sensor) {
    return node.type == NODE_SENSOR &&
           node.num == (sensor.group - 'A') * 16 + (sensor.idx - 1);
}

// precondition: branch.type == NODE_BRANCH
inline static Marklin::BranchDir branch_dir(
    const track_node& branch,
    const Marklin::BranchState* branches,
    size_t branches_len) {
    for (size_t i = 0; i < branches_len; i++) {
        if (branches[i].get_branch() == branch.num) {
            return branches[i].get_dir();
        }
    }
    panic("TrackGraph::branch_dir: unkown direction for branch %d", branch.num);
}

int TrackGraph::distance_between(const Marklin::sensor_t& old_sensor,
                                 const Marklin::sensor_t& new_sensor,
                                 const Marklin::BranchState* branches,
                                 size_t branches_len) const {
    debug("distance_between(%c%hhu %c%hhu)", old_sensor.group, old_sensor.idx,
          new_sensor.group, new_sensor.idx);
    const track_node* start = nullptr;
    const track_node* end = nullptr;
    for (auto& node : track) {
        if (node_is_sensor(node, old_sensor)) start = &node;
        if (node_is_sensor(node, new_sensor)) end = &node;
    }
    assert(start != nullptr);
    assert(end != nullptr);

    if (start == end) return 0;

    int distance = 0;
    const track_node* curr = start;
    for (size_t i = 0; curr != end; i++) {
        if (i > MAX_ITERS) panic("no route after %u hops", MAX_ITERS);
        const track_edge* edge = nullptr;
        switch (curr->type) {
            case NODE_BRANCH:
                switch (branch_dir(*curr, branches, branches_len)) {
                    case Marklin::BranchDir::Straight:
                        edge = &curr->edge[0];
                        break;
                    case Marklin::BranchDir::Curved:
                        edge = &curr->edge[1];
                        break;
                    default:
                        assert(false);
                }
                break;
            default:
                edge = &curr->edge[0];
                break;
        }
        assert(edge != nullptr);
        debug("  %s->%s: %dmm", curr->name, edge->dest->name, edge->dist);
        curr = edge->dest;
        distance += edge->dist;

        if (curr == start) {
            panic(
                "TrackGraph::distance_between: no route from %c%hhu to %c%hhu "
                "(route returned to start)",
                old_sensor.group, old_sensor.idx, new_sensor.group,
                new_sensor.idx);
        }
    }
    debug("distance_between(%c%hhu %c%hhu) = %dmm", old_sensor.group,
          old_sensor.idx, new_sensor.group, new_sensor.idx, distance);
    return distance;
}
