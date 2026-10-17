#pragma once

// The yard's forklift (the owner, 2026-10-17: the heavy stock is moved by a
// machine, and a forklift first; ideas taken from the ChatGPT branch's truck
// and kept lean here). A counterbalance truck of about 2.7 t: two driven
// front wheels, two steered rear wheels, a counterweight behind the rear
// axle, an overhead guard, and a mast whose carriage and forks ride up it on
// a powered slide. It stands parked north of the drums on their pallets,
// aligned with them, its forks low.
//
// The driver stands on its floor under the guard. At the wheel, Action takes
// the controls: the stick drives (forward and back) and steers, two held
// buttons raise and lower the forks, and Action again lets go. Nothing is
// scripted: the wheels drive through the tyres' friction, the forks lift
// through a motor of finite force (kLiftForceN: a 3.75 t container stalls it,
// and tips the truck onto its nose long before), and a load sits on the
// forks only by its weight and their friction. Let go of, it brakes.
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

    // The forks: from 5 cm off the ground up the mast to its top (it does
    // not telescope), at kLiftSpeedMps, by a motor of at most kLiftForceN
    // (CHOSEN: a 2.5 t truck's rating with the carriage's own weight).
    static constexpr float kLiftStrokeM = 1.8F;
    static constexpr float kLiftSpeedMps = 0.4F;
    static constexpr float kLiftForceN = 30000.0F;
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
    };

    Forklift(JPH::PhysicsSystem &physics, kit::Kit &kit, JPH::ObjectLayer moving_layer, JPH::BodyID player);
    ~Forklift();
    Forklift(const Forklift &) = delete;
    Forklift &operator=(const Forklift &) = delete;

    // Before the physics step. take: Action (takes the controls at the
    // wheel, lets go of them while driving). at_controls: the player stands on
    // the truck's floor within kStationReachM of the station. drive, steer
    // and lift are the stick's forward and right and the forks' buttons, each
    // -1 .. 1, and act only while driving.
    void pre_step(float delta_seconds, bool take, bool at_controls, float drive, float steer, float lift);

    [[nodiscard]] bool driving() const noexcept { return state_.driving; }
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
    kit::BodyIndex chassis_{};
    kit::BodyIndex carriage_{};
    JPH::IgnoreMultipleBodiesFilter wheel_filter_;
    JPH::Ref<JPH::VehicleConstraint> vehicle_;
    JPH::Ref<JPH::SliderConstraint> lift_;
    State state_{};
    float lift_target_ = 0.0F;
};

} // namespace scraperx::sim
