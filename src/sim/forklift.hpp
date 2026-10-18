#pragma once

// The yard's forklift (the owner, 2026-10-17: the heavy stock is moved by a
// machine, and a forklift first; ideas taken from the ChatGPT branch's truck
// and kept lean here). A counterbalance truck of about 5.1 t: two driven
// front wheels, two steered rear wheels, a counterweight behind the rear
// axle, an overhead guard, and a mast whose carriage and forks ride up it on
// a powered slide. It stands parked north of the drums on their pallets,
// aligned with them, its forks low.
//
// The driver stands on its floor under the guard. At the wheel, Action takes
// the controls: the stick drives (forward and back) and steers, two held
// buttons raise and lower the forks, and Action again lets go. Nothing is
// scripted: the wheels drive through the tyres' friction, the forks lift
// through a ram of finite force (kLiftForceN), and a load sits on the forks
// only by its weight and their friction. Its limit is the real one: the ram
// is stronger than the counterweight, so too heavy a load held out on the
// forks lifts the truck's back end off the ground before it leaves its own;
// heavier still, past what the ram can push, the ram stalls. Let go of, it
// brakes.
//
// The wheels are Jolt's vehicle wheels (a ray of suspension and a cast
// cylinder each); the chassis and the carriage are kit bodies, saved and
// restored with the machines. A restore lets go of the controls and stills
// the wheels and the engine, so every world restored from one save drives on
// alike.

#include "sim/mechanism_kit.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>

#include <array>
#include <cstdint>

namespace scraperx::sim {

class Forklift final {
public:
    static constexpr std::uint64_t kChassisEntity = 2982;
    static constexpr std::uint64_t kCarriageEntity = 2983;

    // Parked facing +z (south, into the yard), lined up with the first
    // pallet of drums (loose_stock.cpp), its fork tips 4.6 m short of it.
    static constexpr float kParkX = 38.0F;
    static constexpr float kParkZ = 16.5F;
    // The yard crane's (yard_crane.hpp), parked on its laydown facing east,
    // the way it drives in off the docks onto the stack's decks.
    static constexpr std::uint64_t kCraneChassisEntity = 2989;
    static constexpr std::uint64_t kCraneCarriageEntity = 2990;
    static constexpr float kCraneParkX = -66.5F;
    static constexpr float kCraneParkZ = -113.0F;
    // Two the crane brought up before: on the stack's west band of the 11 m
    // deck by the low dock's inner end, nose south, and of the 44 m deck by
    // the high dock, nose north.
    static constexpr std::uint64_t kDeck11ChassisEntity = 2991;
    static constexpr std::uint64_t kDeck11CarriageEntity = 2992;
    static constexpr std::uint64_t kDeck44ChassisEntity = 2993;
    static constexpr std::uint64_t kDeck44CarriageEntity = 2994;
    // Its lifting eye, on the overhead guard's roof over the centre of mass,
    // where a crane's sling hooks on: in the truck's frame.
    static constexpr float kEyeY = 2.42F;
    static constexpr float kEyeZ = -0.5F;

    // The forks: from 3.5 cm off the ground up the mast to its top (it does
    // not telescope), at kLiftSpeedMps, by a ram of at most kLiftForceN
    // (CHOSEN: a relief valve at about 1.4 times the truck's 3.5 t rating;
    // what limits a lift far out on the forks is the counterweight).
    static constexpr float kLiftStrokeM = 1.8F;
    static constexpr float kLiftSpeedMps = 0.4F;
    static constexpr float kLiftForceN = 50000.0F;
    // What the ram holds with its valve shut, or lets down metered: whatever
    // the mast can carry (CHOSEN: four times what the pump lifts).
    static constexpr float kHoldForceN = 4.0F * kLiftForceN;
    // The stick pushed all the way asks for kTopSpeedMps (about 11.5 km/h),
    // less of it for less.
    static constexpr float kTopSpeedMps = 3.2F;
    // The steered wheels turn at most kSteerMaxRad, at kSteerRateRadS.
    static constexpr float kSteerMaxRad = 0.9F;
    static constexpr float kSteerRateRadS = 1.6F;
    // The driver's place: within kStationReachM of the station, on the floor.
    static constexpr float kStationReachM = 0.7F;
    static constexpr int kWheels = 4;

    struct State {
        bool driving = false;
        float lift_m = 0.0F;        // the forks above their lowest
        float speed_mps = 0.0F;     // along the truck's forward
        float steer = 0.0F;         // -1 .. 1, the wheel as turned (right +)
        float drive = 0.0F;         // the throttle as applied, -1 .. 1
        float lift = 0.0F;          // the lift lever as applied: + raising, - lowering
        float lift_n = 0.0F;        // the lift's push up the mast this tick (to kLiftForceN)
        float pitch_rad = 0.0F;     // the truck's nose down, + (on its front wheels)
        // Each wheel's load on the ground this tick and whether it touches:
        // front left, front right, rear left, rear right.
        std::array<float, kWheels> wheel_load_n{};
        std::array<bool, kWheels> wheel_contact{};
    };

    // Parked at (park_x, park_y, park_z) on a floor at park_y, its nose `yaw`
    // about +y from +z, its bodies the kit's entities chassis_entity and
    // carriage_entity.
    Forklift(JPH::PhysicsSystem &physics, kit::Kit &kit, JPH::ObjectLayer moving_layer, JPH::BodyID player,
             std::uint64_t chassis_entity = kChassisEntity, std::uint64_t carriage_entity = kCarriageEntity,
             float park_x = kParkX, float park_z = kParkZ, float yaw = 0.0F, float park_y = 0.0F);
    ~Forklift();
    Forklift(const Forklift &) = delete;
    Forklift &operator=(const Forklift &) = delete;

    // Before the physics step. take: Action (takes the controls at the
    // wheel, lets go of them while driving). at_controls: the player stands on
    // the truck's floor within kStationReachM of the station. drive, steer
    // and lift are the stick's forward and right and the forks' buttons, each
    // -1 .. 1, and act only while driving.
    void pre_step(float delta_seconds, bool take, bool at_controls, float drive, float steer, float lift);
    // After the physics step: what the lift pushed and the wheels carried.
    void post_step(float delta_seconds);

    [[nodiscard]] bool driving() const noexcept { return state_.driving; }
    [[nodiscard]] std::uint64_t chassis_entity() const noexcept { return chassis_entity_; }
    [[nodiscard]] const State &state() const noexcept { return state_; }
    // Where the driver stands, in the world.
    [[nodiscard]] JPH::RVec3 station() const;
    // Wheel i's centre and its axle (a unit vector), and its size.
    [[nodiscard]] JPH::RMat44 wheel_transform(int wheel) const;
    [[nodiscard]] float wheel_radius(int wheel) const noexcept;
    [[nodiscard]] float wheel_width(int wheel) const noexcept;

    // After the kit's restore: the controls let go, the wheels and the
    // engine still, the steering centred, the forks held where they are.
    void restore();

private:
    JPH::PhysicsSystem &physics_;
    kit::Kit &kit_;
    std::uint64_t chassis_entity_ = kChassisEntity;
    kit::BodyIndex chassis_{};
    kit::BodyIndex carriage_{};
    JPH::IgnoreMultipleBodiesFilter wheel_filter_;
    JPH::Ref<JPH::VehicleConstraint> vehicle_;
    JPH::Ref<JPH::SliderConstraint> lift_;
    State state_{};
    float lift_target_ = 0.0F;
};

} // namespace scraperx::sim
