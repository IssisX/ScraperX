#pragma once

// The summit (task 92, the owner's choice A + B): what stands at the top of
// the route, west of the luffing derrick's upper receiver (802.64 m).
//
//   A. A dead searchlight on a pylon at the deck's north edge. It is lit by
//      the body: a striker plate, a pad on four guides held up by springs, is
//      struck by a rider landing from the gantry 6.4 m over it. Standing on
//      the plate, or hopping on it, does not take it far. Once it has gone
//      down past the trip travel the beam is lit for good (it persists past a
//      death) and the lamp's turntable is turned by its motor.
//   B. A runway arm out of the deck's west end, 38 m of it, open at its end
//      over the drop, with a windsock on a pole: run off it and open the
//      parachute.
//
// One static frame carries the deck, the pylon, the gantry and the arm; the
// striker plate and the lamp's turntable are the two moving bodies. Jolt owns
// both: no velocity or position is set after the build.

#include "sim/mechanism_kit.hpp"

#include <array>

#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>

namespace scraperx::sim {

class Summit final {
public:
    static constexpr std::uint64_t kFrameEntity = 1900;
    static constexpr std::uint64_t kStrikerEntity = 2980;
    static constexpr std::uint64_t kLampEntity = 2981;

    // The deck's top, level with the derrick's upper receiver (1978).
    static constexpr float kDeckTop = 802.64F;
    static constexpr float kDeckWestX = -12.0F;
    static constexpr float kDeckEastX = 11.5F;
    static constexpr float kDeckNorthZ = -191.5F;
    static constexpr float kDeckSouthZ = -183.5F;
    // The arm: its centre line, its width, and where it stops.
    static constexpr float kArmCentreZ = -187.5F;
    static constexpr float kArmHalfWidth = 1.5F;
    static constexpr float kArmEndX = -50.0F;

    // The gantry the strike is made from, and the blocks up to it.
    static constexpr float kGantryTop = kDeckTop + 7.4F;
    static constexpr float kGantryWestX = -0.8F;
    static constexpr float kGantryEastX = 2.4F;
    static constexpr float kBlockOneTop = kDeckTop + 1.6F;
    static constexpr float kBlockTwoTop = kDeckTop + 4.4F;

    // The striker plate: a pad 3.2 m by 3.0 m on four guides, its top a metre
    // over the deck at rest. Gas struts under it hold it against its upper stop
    // with the struts' preload kStrutPreloadN over the plate's weight.
    static constexpr float kStrikerWestX = 3.0F;
    static constexpr float kStrikerEastX = 7.2F;
    static constexpr float kStrikerNorthZ = -189.0F;
    static constexpr float kStrikerSouthZ = -186.0F;
    static constexpr float kStrikerRestTop = kDeckTop + 1.0F;
    static constexpr float kStrikerMassKg = 300.0F;
    static constexpr float kStrutPreloadN = 5000.0F;
    // The struts are damped on the way up: the plate comes back at no more than this.
    static constexpr float kPlateRiseMps = 0.25F;
    // How far down the plate goes (m) to light the lamp, and its lower stop.
    static constexpr float kTripTravel = 0.2F;
    static constexpr float kStopTravel = 0.5F;

    // The lamp's turntable: its axis, and the motor that turns it once lit.
    static constexpr float kPylonX = -6.0F;
    static constexpr float kPylonZ = -190.2F;
    static constexpr float kTurntableY = kDeckTop + 30.0F;
    static constexpr float kLampSpeedRadPerS = 0.35F;
    static constexpr float kLampTorqueNm = 30000.0F;

    // The overtime crew's last beam: a tandem chain block left on its
    // erection strongback. Its two bearing stools carry it after lowering.
    static constexpr std::uint64_t kEndingFrameEntity = 1901;
    static constexpr std::uint64_t kLastBeamEntity = 2968;
    static constexpr std::uint64_t kLeftBearingEntity = 1902;
    static constexpr std::uint64_t kRightBearingEntity = 1903;
    static constexpr float kBeamX = -7.0F;
    static constexpr float kBeamZ = -184.0F;
    static constexpr float kBearingTop = kDeckTop + 2.4F;
    static constexpr float kBeamSeatY = kBearingTop + 0.3F;
    static constexpr float kChainTop = kDeckTop + 5.0F;
    static constexpr float kBeamLift = 1.2F;
    static constexpr float kChainRate = 0.2F;
    static constexpr float kChainSlack = 0.25F;
    static constexpr float kWorkZ = -185.25F;
    enum class Ending : std::uint8_t { Unfinished, Seated, Unrigged, Canopy, Home };

    struct State final {
        Ending ending = Ending::Unfinished;
        bool beam_seated = false;
        bool sling_free = false;
        bool left_bearing = false;
        bool right_bearing = false;
        float chain_paid_m = 0.0F;
        float chain_tension_n = 0.0F;
        bool lit = false;
        float striker_travel_m = 0.0F;    // down from its rest
        float striker_peak_m = 0.0F;      // the deepest it has been
        float lamp_yaw_rad = 0.0F;
        float lamp_speed_rad_s = 0.0F;
    };

    Summit(JPH::PhysicsSystem &system, kit::Kit &kit);
    ~Summit();
    Summit(const Summit &) = delete;
    Summit &operator=(const Summit &) = delete;

    // Before the step: the lamp's motor is on once it is lit. After it: the
    // plate's angle is read, and the lamp is latched lit by it, never unlit.
    // Appended after existing bodies, preserving their save indices.
    void build_ending();
    [[nodiscard]] bool work_in_reach(JPH::RVec3 player) const noexcept;
    [[nodiscard]] bool can_unrig() const noexcept;
    void work(float dt, bool lower, bool unrig, bool able, JPH::RVec3 player);
    void read_ending(float dt, bool grounded, bool chute, JPH::RVec3 player,
                     std::uint64_t deaths);
    // Reconcile the hoist with restored physical bodies, without moving them.
    void restore_ending();
    void pre_step();
    void post_step();
    [[nodiscard]] const State &state() const noexcept { return state_; }
    // A world that starts after the beacon was lit (kept by the profile).
    void light() noexcept { state_.lit = true; }

private:
    JPH::PhysicsSystem &system_;
    kit::Kit &kit_;
    kit::BodyIndex frame_{};
    kit::BodyIndex striker_{};
    kit::BodyIndex lamp_{};
    JPH::Ref<JPH::SliderConstraint> striker_slide_;
    JPH::Ref<JPH::HingeConstraint> lamp_hinge_;
    kit::BodyIndex ending_frame_{};
    kit::BodyIndex last_beam_{};
    std::array<kit::BodyIndex, 2> bearings_{};
    std::array<JPH::Ref<kit::RopeConstraint>, 2> chains_{};
    float bearing_seconds_ = 0.0F;
    std::uint64_t flight_deaths_ = 0;
    [[nodiscard]] bool bearing(int side) const;
    void rig_chains(float slack);
    State state_{};
};

} // namespace scraperx::sim
