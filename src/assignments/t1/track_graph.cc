#include "track_graph.h"

#include "user/debug.h"

static constexpr size_t MAX_ITERS = 40;

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

// precondition: node.type == NODE_SENSOR
inline static Marklin::sensor_t sensor_of_node(const track_node& node) {
    return {.group = (char)('A' + (node.num / 16)),
            .idx = (uint8_t)(node.num % 16 + 1)};
}

// precondition: branch.type == NODE_BRANCH
inline static Marklin::BranchDir branch_dir(
    const track_node& branch,
    const Marklin::BranchState* branches,
    size_t branches_len) {
    for (size_t i = 0; i < branches_len; i++) {
        if (branches[i].get_id() == branch.num) {
            return branches[i].get_dir();
        }
    }
    panic("TrackGraph::branch_dir: unkown direction for branch %d", branch.num);
}

inline static const track_edge* next_edge(const track_node& node,
                                          const Marklin::BranchState* branches,
                                          size_t branches_len) {
    switch (node.type) {
        case NODE_NONE:
        case NODE_EXIT:
            return nullptr;
        case NODE_BRANCH:
            switch (branch_dir(node, branches, branches_len)) {
                case Marklin::BranchDir::Straight:
                    return &node.edge[0];
                case Marklin::BranchDir::Curved:
                    return &node.edge[1];
                default:
                    assert(false);
            }
        default:
            auto edge = &node.edge[0];
            assert(edge != 0);
            return edge;
    }
}

std::optional<std::pair<Marklin::sensor_t, int>> TrackGraph::next_sensor(
    const Marklin::sensor_t& sensor,
    const Marklin::BranchState* branches,
    size_t branches_len) const {
    const track_node* curr = nullptr;
    for (auto& node : track) {
        if (node_is_sensor(node, sensor)) curr = &node;
    }
    if (curr == nullptr) return std::nullopt;
    int distance = 0;
    while (true) {
        auto edge = next_edge(*curr, branches, branches_len);
        if (edge == nullptr) return std::nullopt;
        distance += edge->dist;
        curr = edge->dest;
        if (curr->type == NODE_SENSOR)
            return std::make_pair(sensor_of_node(*curr), distance);
    }
}

std::optional<int> TrackGraph::distance_between(
    const Marklin::sensor_t& old_sensor,
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
        if (i > MAX_ITERS) return std::nullopt;
        const track_edge* edge = next_edge(*curr, branches, branches_len);
        assert(edge != nullptr);
        debug("  %s->%s: %dmm", curr->name, edge->dest->name, edge->dist);
        curr = edge->dest;
        distance += edge->dist;

        if (curr == start) {
            return std::nullopt;
        }
    }
    debug("distance_between(%c%hhu %c%hhu) = %dmm", old_sensor.group,
          old_sensor.idx, new_sensor.group, new_sensor.idx, distance);
    return distance;
}

Marklin::sensor_t TrackGraph::invert_sensor(
    const Marklin::sensor_t& sensor) const {
    const track_node* node = nullptr;
    for (auto& n : track) {
        if (node_is_sensor(n, sensor)) node = &n;
    }
    if (node == nullptr) panic("unknown sensor %c%u", sensor.group, sensor.idx);
    return sensor_of_node(*node->reverse);
}
