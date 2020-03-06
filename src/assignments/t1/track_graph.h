#pragma once

#include <optional>

#include "marklin.h"
#include "track_data_new.h"

class TrackGraph {
    track_node track[TRACK_MAX];

   public:
    TrackGraph(Marklin::Track t);

    std::optional<int /* mm */> distance_between(
        const Marklin::sensor_t& old_sensor,
        const Marklin::sensor_t& new_sensor,
        const Marklin::BranchState* branches,
        size_t branches_len) const;

    std::optional<std::pair<Marklin::sensor_t, int /* distance, mm */>>
    next_sensor(const Marklin::sensor_t& sensor,
                const Marklin::BranchState* branches,
                size_t branches_len) const;

    std::optional<std::pair<Marklin::sensor_t, int /* distance, mm */>>
    prev_sensor(const Marklin::sensor_t& sensor,
                const Marklin::BranchState* branches,
                size_t branches_len) const;

    Marklin::track_pos_t normalize(const Marklin::track_pos_t& pos,
                                   const Marklin::BranchState* branches,
                                   size_t branches_len) const;
    Marklin::sensor_t invert_sensor(const Marklin::sensor_t& sensor) const;
};
