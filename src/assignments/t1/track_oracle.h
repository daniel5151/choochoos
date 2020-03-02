#pragma once

#include <cstddef>
#include <cstdint>

#include "marklin.h"

/// Aggregate type describing a particular train's current state
struct train_descriptor_t {
    // Fixed params
    uint8_t id;
    uint8_t speed;
    bool reversed;
    bool lights;

    // Computed params
    int velocity;
    Marklin::track_pos_t pos;
};

/// Track Oracle. Responsible for maintaining a model of the current track state
/// (switch positions, train positions and velocities, etc...), and facilitating
/// updates / queries to the track.
class TrackOracle {
   private:
    /// TrackOracle Task TID
    int tid;

   public:
    /// Create a new track oracle task for the given track, resetting the
    /// track's branches to a preset state.
    TrackOracle(Marklin::Track track);

    /// Called whenever a new train is placed on the track. Sets the train speed
    /// to zero, waits a bit, sets the speed to something fairly low, and waits
    /// for it to hit a sensor (thereby determining it's position and direction)
    void calibrate_train(uint8_t train_id);

    /// Update a train's speed
    void set_train_speed(uint8_t train_id, uint8_t speed);
    /// Update a train's lights
    void set_train_light(uint8_t train_id, bool active);
    /// Reverse a train's direction (via speed 15)
    void reverse_train(uint8_t train_id);

    /// Update a branch's direction
    void set_branch_dir(uint8_t branch_id, Marklin::BranchDir dir);

    /// query the marklin for sensor data, updating the internal model
    /// appropriately
    void update_sensors();

    /// Return a particular train's current state
    train_descriptor_t query_train(uint8_t train_id);
    /// Return a particular branch's state
    Marklin::BranchDir query_branch(uint8_t branch_id);

    /// Unblock the calling task once the specified train is at the specified
    /// position on the track
    void wake_at_pos(uint8_t train_id, Marklin::track_pos_t pos);
};
