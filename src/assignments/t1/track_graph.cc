#include "track_graph.h"

#include <climits>

#include "common/priority_queue.h"
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

std::optional<std::pair<Marklin::sensor_t, int /* distance, mm */>>
TrackGraph::prev_sensor(const Marklin::sensor_t& sensor,
                        const Marklin::BranchState* branches,
                        size_t branches_len) const {
    auto prev_sensor_inv_opt =
        next_sensor(invert_sensor(sensor), branches, branches_len);
    if (!prev_sensor_inv_opt.has_value()) return std::nullopt;
    auto [prev_sensor_inv, distance] = prev_sensor_inv_opt.value();
    return std::make_pair(invert_sensor(prev_sensor_inv), distance);
}

Marklin::track_pos_t TrackGraph::normalize(const Marklin::track_pos_t& pos,
                                           const Marklin::BranchState* branches,
                                           size_t branches_len) const {
    Marklin::track_pos_t ret = pos;
    while (true) {
        if (ret.offset_mm == 0) return ret;

        if (ret.offset_mm > 0) {
            auto next_sensor_opt =
                next_sensor(ret.sensor, branches, branches_len);
            if (!next_sensor_opt.has_value()) return ret;
            auto [next_sensor, distance] = next_sensor_opt.value();
            if (distance > ret.offset_mm) return ret;
            ret.sensor = next_sensor;
            ret.offset_mm -= distance;
            assert(ret.offset_mm >= 0);
        } else {
            assert(ret.offset_mm < 0);
            auto prev_sensor_opt =
                prev_sensor(ret.sensor, branches, branches_len);
            if (!prev_sensor_opt.has_value()) return ret;
            auto [prev_sensor, distance] = prev_sensor_opt.value();
            assert(distance > 0);
            ret.sensor = prev_sensor;
            ret.offset_mm += distance;
        }
    }
}

struct search_node_t {
    track_node* node;
    int total_distance;
};

#include "ui.h"
#include "user/tasks/clockserver.h"
#include "user/tasks/uartserver.h"

static inline size_t index_of(const track_node* node,
                              const track_node track[]) {
    return (node - track);
}

int TrackGraph::shortest_path(const Marklin::sensor_t& start,
                              const Marklin::sensor_t& end,
                              const track_node* path[],
                              size_t max_path_len,
                              size_t& distance) const {
    int uart = WhoIs(Uart::SERVER_ID);
    int clock = WhoIs(Clock::SERVER_ID);
    assert(uart >= 0);
    assert(clock >= 0);

    int dist[TRACK_MAX];
    bool visited[TRACK_MAX];
    const track_node* prev[TRACK_MAX];
    for (size_t i = 0; i < TRACK_MAX; i++) {
        dist[i] = INT_MAX;
        visited[i] = false;
        prev[i] = nullptr;
    }

    const track_node* start_node = nullptr;
    const track_node* end_node = nullptr;
    for (auto& n : track) {
        if (node_is_sensor(n, start)) start_node = &n;
        if (node_is_sensor(n, end)) end_node = &n;
    }
    assert(start_node != nullptr);
    assert(end_node != nullptr);
    const size_t start_idx = index_of(start_node, track);
    const size_t end_idx = index_of(end_node, track);
    assert(start_idx < TRACK_MAX);
    assert(end_idx < TRACK_MAX);

    log_line(uart, "start=%s (idx %u) end=%s (idx %u)", start_node->name,
             start_idx, end_node->name, end_idx);

    if (start_node == end_node) {
        distance = 0;
        return 0;
    }

    dist[start_idx] = 0;

    bool found = false;
    while (!found) {
        int min_dist = INT_MAX;
        const track_node* curr = nullptr;
        for (size_t i = 0; i < TRACK_MAX; i++) {
            const track_node& node = track[i];
            assert(index_of(&node, track) == i);
            if (!visited[i] && dist[i] < min_dist && node.type != NODE_NONE &&
                node.type != NODE_EXIT) {
                curr = &node;
                min_dist = dist[i];
            }
        }
        if (curr == nullptr) break;
        const size_t curr_idx = index_of(curr, track);
        visited[curr_idx] = true;
        log_line(uart, "visting %s (idx %u) (dist %d)", curr->name, curr_idx,
                 dist[curr_idx]);

        size_t num_edges = curr->type == NODE_BRANCH ? 2 : 1;
        for (size_t i = 0; i < num_edges; i++) {
            const track_edge& edge = curr->edge[i];
            track_node* next = edge.dest;
            size_t next_idx = index_of(next, track);
            assert(next_idx < TRACK_MAX);

            if (visited[next_idx]) continue;

            int alt_dist = std::min(dist[next_idx], dist[curr_idx] + edge.dist);
            if (alt_dist < dist[next_idx]) {
                dist[next_idx] = alt_dist;
                prev[next_idx] = curr;
                if (next == end_node) {
                    found = true;
                }
            }
        }
    }

    if (prev[end_idx] == nullptr) {
        return -1;
    }
    int path_len = 1;
    {
        const track_node* curr = end_node;
        while (curr != start_node) {
            size_t curr_idx = index_of(curr, track);
            assert(curr_idx < TRACK_MAX);
            curr = prev[curr_idx];
            assert(curr != nullptr);
            path_len++;
        }
    }

    if ((size_t)path_len > max_path_len) {
        panic("path_len %d too large (max_path_len=%u)", path_len,
              max_path_len);
    }

    const track_node* curr = end_node;
    size_t i = path_len - 1;
    while (curr != start_node) {
        path[i] = curr;
        curr = prev[index_of(curr, track)];
        i--;
    }
    path[0] = start_node;

    distance = (size_t)dist[end_idx];

    return (int)path_len;
}
