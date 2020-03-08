#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "marklin.h"

#define TICKS_PER_SEC 100

struct sensor_time_t {
    Marklin::sensor_t sensor;
    int time;
};

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
    int pos_observed_at;
    int speed_changed_at;
    bool accelerating;
    uint8_t old_speed;

    bool has_next_sensor;
    Marklin::sensor_t next_sensor;
    int next_sensor_time;

    bool has_error;
    int time_error;
    int distance_error;
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

    // Look up the existing track oracle task via the nameserver - callers must
    // ensure that the task-spawning constructor above has already been called
    // once.
    TrackOracle();

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
    std::optional<train_descriptor_t> query_train(uint8_t train_id);
    /// Return a particular branch's state
    Marklin::BranchDir query_branch(uint8_t branch_id);

    // Return a pos representing the same position on the track, making a best
    // effort to return a positive offset that is as small as possible.
    Marklin::track_pos_t normalize(const Marklin::track_pos_t& pos);

    /// Unblock the calling task once the specified train is at the specified
    /// position on the track. Returns false if the track's state changes such
    /// that the train will never arrive at the desired position.
    [[nodiscard]] bool wake_at_pos(uint8_t train_id, Marklin::track_pos_t pos);
};
