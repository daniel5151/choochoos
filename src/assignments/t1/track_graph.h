#pragma once

#include "marklin.h"

#include "track_data_new.h"

class TrackGraph {
    track_node track[TRACK_MAX];

   public:
    TrackGraph(Marklin::Track t);
    int distance_between(const Marklin::sensor_t& old_sensor,
                         const Marklin::sensor_t& new_sensor,
                         const Marklin::BranchState* branches,
                         size_t branches_len) const;
};
