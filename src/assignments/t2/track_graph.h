#pragma once

#include <optional>

#include "marklin.h"
#include "track_data_new.h"

static constexpr size_t BRANCHES_LEN = sizeof(Marklin::VALID_SWITCHES);

class TrackGraph {
    track_node track[TRACK_MAX];
    Marklin::BranchState branches[BRANCHES_LEN];

    Marklin::BranchDir branch_dir(const track_node& branch) const;
    const track_edge* next_edge(const track_node& node) const;

   public:
    TrackGraph(Marklin::Track t);

    std::optional<int /* mm */> distance_between(
        const Marklin::sensor_t& old_sensor,
        const Marklin::sensor_t& new_sensor) const;

    std::optional<std::pair<Marklin::sensor_t, int /* distance, mm */>>
    next_sensor(const Marklin::sensor_t& sensor) const;

    std::optional<std::pair<Marklin::sensor_t, int /* distance, mm */>>
    prev_sensor(const Marklin::sensor_t& sensor) const;

    Marklin::track_pos_t normalize(const Marklin::track_pos_t& pos) const;
    Marklin::sensor_t invert_sensor(const Marklin::sensor_t& sensor) const;

    [[nodiscard]] int shortest_path(const Marklin::sensor_t& start,
                                    const Marklin::sensor_t& end,
                                    const track_node* path[],
                                    size_t max_path_len,
                                    size_t& distance) const;
};
