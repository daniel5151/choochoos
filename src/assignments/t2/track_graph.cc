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

    make_loop();
}
void TrackGraph::make_loop() {
    // TODO: make the loops vary between the two tracks

    // set all the branches to curved
    for (size_t i = 0; auto& b : this->branches) {
        const uint8_t id = Marklin::VALID_SWITCHES[i++];

        b.set_id(id);
        b.set_dir(Marklin::BranchDir::Curved);

        // ...but make outer-ring branches straight
        for (size_t except_id : {6, 7, 8, 9, 14, 15}) {
            if (id == except_id) {
                b.set_dir(Marklin::BranchDir::Straight);
                break;
            }
        }
    }
}

void TrackGraph::set_branch_dir(uint8_t id, Marklin::BranchDir dir) {
    for (auto& b : this->branches) {
        if (b.get_id() == id) {
            b.set_dir(dir);
            return;
        }
    }

    panic("called set_branch_dir with invalid branch id");
}

const Marklin::BranchState* TrackGraph::get_branches() const {
    return this->branches;
}

inline static bool node_is_sensor(const track_node& node,
                                  const Marklin::sensor_t& sensor) {
    return node.type == NODE_SENSOR &&
           node.num == (sensor.group - 'A') * 16 + (sensor.idx - 1);
}

inline const track_node* TrackGraph::node_of_sensor(
    const Marklin::sensor_t& sensor) const {
    for (auto& n : track) {
        if (node_is_sensor(n, sensor)) return &n;
    }
    return nullptr;
}

// precondition: node.type == NODE_SENSOR
inline static Marklin::sensor_t sensor_of_node(const track_node& node) {
    return {.group = (char)('A' + (node.num / 16)),
            .idx = (uint8_t)(node.num % 16 + 1)};
}

// precondition: branch.type == NODE_BRANCH
Marklin::BranchDir TrackGraph::branch_dir(const track_node& branch) const {
    assert(branch.type == NODE_BRANCH);
    for (auto& b : branches) {
        if (b.get_id() == branch.num) {
            return b.get_dir();
        }
    }
    panic("TrackGraph::branch_dir: unkown direction for branch %d", branch.num);
}

const track_edge* TrackGraph::next_edge(const track_node& node) const {
    switch (node.type) {
        case NODE_NONE:
        case NODE_EXIT:
            return nullptr;
        case NODE_BRANCH:
            switch (branch_dir(node)) {
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
    const Marklin::sensor_t& sensor) const {
    const track_node* curr = node_of_sensor(sensor);
    if (curr == nullptr) return std::nullopt;
    int distance = 0;
    while (true) {
        auto edge = next_edge(*curr);
        if (edge == nullptr) return std::nullopt;
        distance += edge->dist;
        curr = edge->dest;
        if (curr->type == NODE_SENSOR)
            return std::make_pair(sensor_of_node(*curr), distance);
    }
}

std::optional<int> TrackGraph::distance_between(
    const Marklin::sensor_t& old_sensor,
    const Marklin::sensor_t& new_sensor) const {
    debug("distance_between(%c%hhu %c%hhu)", old_sensor.group, old_sensor.idx,
          new_sensor.group, new_sensor.idx);
    const track_node* start = node_of_sensor(old_sensor);
    const track_node* end = node_of_sensor(new_sensor);
    assert(start != nullptr);
    assert(end != nullptr);

    if (start == end) return 0;

    int distance = 0;
    const track_node* curr = start;
    for (size_t i = 0; curr != end; i++) {
        if (i > MAX_ITERS) return std::nullopt;
        const track_edge* edge = next_edge(*curr);
        if (edge == nullptr) {
            // we've run out of track - there's no path to new_sensor given the
            // current track state.
            return std::nullopt;
        }
        debug("  %s->%s: %dmm", curr->name, edge->dest->name, edge->dist);
        curr = edge->dest;
        distance += edge->dist;

        if (curr == start) {
            // we've hit a cycle, and we'll never reach new_sensor
            return std::nullopt;
        }
    }
    debug("distance_between(%c%hhu %c%hhu) = %dmm", old_sensor.group,
          old_sensor.idx, new_sensor.group, new_sensor.idx, distance);
    return distance;
}

Marklin::sensor_t TrackGraph::invert_sensor(
    const Marklin::sensor_t& sensor) const {
    const track_node* node = node_of_sensor(sensor);
    if (node == nullptr) panic("unknown sensor %c%u", sensor.group, sensor.idx);
    return sensor_of_node(*node->reverse);
}

std::optional<std::pair<Marklin::sensor_t, int /* distance, mm */>>
TrackGraph::prev_sensor(const Marklin::sensor_t& sensor) const {
    auto prev_sensor_inv_opt = next_sensor(invert_sensor(sensor));
    if (!prev_sensor_inv_opt.has_value()) return std::nullopt;
    auto [prev_sensor_inv, distance] = prev_sensor_inv_opt.value();
    return std::make_pair(invert_sensor(prev_sensor_inv), distance);
}

Marklin::track_pos_t TrackGraph::normalize(
    const Marklin::track_pos_t& pos) const {
    Marklin::track_pos_t ret = pos;
    while (true) {
        if (ret.offset_mm == 0) return ret;

        if (ret.offset_mm > 0) {
            auto next_sensor_opt = next_sensor(ret.sensor);
            if (!next_sensor_opt.has_value()) {
                ret.offset_mm = std::min(ret.offset_mm, max_offset(ret.sensor));
                return ret;
            }
            auto [next_sensor, distance] = next_sensor_opt.value();
            if (distance > ret.offset_mm) return ret;
            ret.sensor = next_sensor;
            ret.offset_mm -= distance;
            assert(ret.offset_mm >= 0);
        } else {
            assert(ret.offset_mm < 0);
            auto prev_sensor_opt = prev_sensor(ret.sensor);
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

static inline size_t index_of(const track_node* node,
                              const track_node track[]) {
    return (node - track);
}

// Run Dijkstra's Algorithm on the track graph, based on this implementation:
// https://en.wikipedia.org/wiki/Dijkstra%27s_algorithm#Pseudocode
int TrackGraph::shortest_path(const Marklin::sensor_t& start,
                              const Marklin::sensor_t& end,
                              const track_node* path[],
                              size_t max_path_len,
                              size_t& distance) const {
    int dist[TRACK_MAX];
    bool visited[TRACK_MAX];
    const track_node* prev[TRACK_MAX];
    for (size_t i = 0; i < TRACK_MAX; i++) {
        dist[i] = INT_MAX;
        visited[i] = false;
        prev[i] = nullptr;
    }

    const track_node* start_node = node_of_sensor(start);
    const track_node* end_node = node_of_sensor(end);
    assert(start_node != nullptr);
    assert(end_node != nullptr);
    const size_t start_idx = index_of(start_node, track);
    const size_t end_idx = index_of(end_node, track);
    assert(start_idx < TRACK_MAX);
    assert(end_idx < TRACK_MAX);

    if (start_node == end_node) {
        distance = 0;
        return 0;
    }

    dist[start_idx] = 0;

    bool found = false;
    while (true) {
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
        if (curr == end_node) {
            found = true;
            break;
        }
        const size_t curr_idx = index_of(curr, track);
        visited[curr_idx] = true;

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
            }
        }
    }

    if (!found) {
        assert(prev[end_idx] == nullptr);
        return -1;
    }

    assert(prev[end_idx] != nullptr);

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

int TrackGraph::max_offset(const Marklin::sensor_t& sensor) const {
    const track_node* curr = node_of_sensor(sensor);
    assert(curr != nullptr);
    int distance = 0;
    int seen_sensors = 0;
    while (seen_sensors < 2) {
        const track_edge* edge = next_edge(*curr);
        if (edge == nullptr) break;
        distance += edge->dist;
        curr = edge->dest;
        if (curr->type == NODE_SENSOR) seen_sensors++;
    }
    return distance;
}
