#pragma once

// The bay (shared survival, the owner, 2026-10-21: "Go ahead and build it"):
// a cut through the 154 m deck's north band, from the shaft's edge to the
// tower's face, between where a climber arrives up the high riser and the
// ladder to the 176 ring. A steel leaf spans it, hinged on its east edge and
// held level against its upper stop by a motor; its west end hangs over the
// bay with nothing under it. A fan in the bay's east wall runs on the same
// supply (the drive and the fan are one district, shared_survival.hpp).
//
// Under the leaf's west end, 1.78 m below the deck, a service bracket on the
// bay's west wall, with rungs up to the deck. Standing there, the Matron's
// hands reach the leaf's underside (her trap, matron.hpp): hands in the
// mechanism trip the district, the motor lets go, and the leaf comes down
// onto her palms, held there by her finite strength alone. She can hold the
// leaf. She cannot hold the leaf and a climber on it.
//
// A chain hangs from the pipe bridge over the leaf, its shackle made fast on
// a cleat on the bridge's east post. Hooked onto the eye on the leaf near
// its hinge it carries the leaf: the chain is the way across. The district
// starts again by itself once no hands are in it, its guard is shut and its
// drives have stopped (kRestartSeconds): a restart, never a pull or a button.
//
// Nothing here is a flag: the leaf is held by a motor, by her hands, by the
// chain or by nothing, and falls when it is held by nothing.

#include "sim/mechanism_kit.hpp"
#include "sim/shared_survival.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdint>
#include <memory>

namespace scraperx::sim {

class Bay final {
public:
    static constexpr std::uint64_t kFrameEntity = 1650;
    static constexpr std::uint64_t kLeafEntity = 2650;
    static constexpr std::uint64_t kGuardEntity = 2651;
    static constexpr std::uint64_t kFanEntity = 2652;
    static constexpr std::uint64_t kShackleEntity = 2653;

    static constexpr float kDeckTop = 154.0F;
    // The cut: across the whole band, the shaft's edge to the tower's face.
    static constexpr float kWestX = -3.9F;
    static constexpr float kEastX = -0.4F;
    static constexpr float kSouthZ = -133.0F;
    static constexpr float kNorthZ = -124.0F;

    // The leaf: its deck flush with the deck's, its hinge on the east edge.
    static constexpr float kLeafZ = -128.5F;
    static constexpr float kLeafHalfWidth = 0.8F;
    static constexpr float kLeafThick = 0.08F;
    static constexpr float kLeafWestX = kWestX + 0.03F;
    // Turning down about its mid-thickness, its east end's lower edge swings
    // 4 cm toward the deck it is hinged at: 6 cm clear.
    static constexpr float kLeafEastX = kEastX - 0.06F;
    static constexpr float kLeafHangAngle = -1.45F;   // rad, its lower stop (hanging)
    // Its rails end this far short of its west end (bay.cpp: clear of the
    // deck's edge as it turns down).
    static constexpr float kLeafRailInset = 0.3F;
    // Its motor holds it on the upper stop with a climber and the Matron on
    // it; let go, its brake slows it only a little (kLeafBrakeNm of the
    // ~2000 N m its own weight turns it with).
    static constexpr float kLeafDriveNm = 15000.0F;
    static constexpr float kLeafRaiseRadS = 0.35F;
    static constexpr float kLeafBrakeNm = 300.0F;

    // The bracket she stands on, and where her feet go to hold the leaf:
    // her hands (hand_local below) then lie under its west end.
    static constexpr float kBracketTop = kDeckTop - 1.78F;
    static constexpr float kBraceX = -3.45F;
    static constexpr float kBraceZ = -129.85F;
    static constexpr float kBraceYaw = 0.0F;   // facing north, toward the leaf
    // Where she steps off the deck to drop onto it (west of the cut, south
    // of the leaf), and the rungs up from it.
    static constexpr float kDropX = -4.35F;
    static constexpr float kDropZ = -130.15F;
    // The rungs are where she drops (the gap in the rail): she hangs on them
    // 0.3 m off the wall and tops out west onto the deck.
    static constexpr float kRungsX = kWestX + 0.3F;
    static constexpr float kRungsZ = kDropZ;
    // Her palms: in front of her middle at shoulder height (a lean into it).
    static constexpr float kHandUp = 0.65F;
    static constexpr float kHandForward = 0.65F;

    // The chain: from a hanger under the pipe bridge straight down to the
    // leaf's eye, 0.8 m from its hinge, and a centimetre longer than that.
    static constexpr float kEyeX = kLeafEastX - 0.8F;
    static constexpr float kEyeZ = kLeafZ + 0.4F;
    static constexpr float kBridgeY = kDeckTop + 2.75F;

    // The fan: on the district, its noise covers a climber's steps near it.
    static constexpr float kFanX = -0.62F;
    static constexpr float kFanY = kDeckTop - 0.8F;
    static constexpr float kFanZ = -126.2F;
    static constexpr float kFanRadS = 16.0F;
    static constexpr float kFanNm = 25.0F;
    static constexpr float kFanBrakeNm = 3.0F;
    static constexpr float kMaskRadiusM = 7.0F;

    // The district starts again this long after its hands are out, its guard
    // shut and its drives still.
    static constexpr float kRestartSeconds = 3.0F;

    // What a checkpoint keeps beyond the kit's bodies and rope.
    struct State final {
        float restart_s = 0.0F;
        std::uint8_t supplied = 1;
        std::uint8_t leaf_running = 1;
        std::uint8_t fan_running = 1;
        std::uint8_t pad = 0;
    };
    static_assert(sizeof(State) == 8, "Bay::State is saved byte for byte");

    struct View final {
        float leaf_angle = 0.0F;      // rad, 0 level, down negative
        float leaf_speed = 0.0F;      // rad/s
        bool leaf_running = false;    // its motor holding it
        float fan_fraction = 0.0F;    // fan speed over its running speed
        bool fan_running = false;
        bool hooked = false;          // the chain on the leaf's eye
        float chain_n = 0.0F;         // the chain's pull
        bool hands = false;           // hands in the district (hers)
        float support_n = 0.0F;
        float hand_x = 0.0F, hand_y = 0.0F, hand_z = 0.0F;
        std::uint32_t trips = 0;      // times the district stopped with hands in it
        std::uint32_t restarts = 0;
    };

    Bay(JPH::PhysicsSystem &system, kit::Kit &kit);
    ~Bay();
    Bay(const Bay &) = delete;
    Bay &operator=(const Bay &) = delete;

    // The district the Matron braces (she calls its pre_step, once a tick).
    [[nodiscard]] SharedSurvival &district() noexcept { return *district_; }
    // Before the Matron's pre_step: the district's own restart.
    void pre_step(float dt);
    // After the physics update: what is drawn and heard.
    void post_step();

    // The leaf level on its upper stop and driven there: a floor to walk.
    [[nodiscard]] bool leaf_open() const noexcept;
    [[nodiscard]] bool hooked() const noexcept;
    // How much of a climber's noise the fan leaves her to hear at `at`:
    // 1 far from it or with it still, down to 0.25 beside it running.
    [[nodiscard]] float mask_factor(JPH::RVec3 at) const noexcept;
    [[nodiscard]] const View &view() const noexcept { return view_; }
    [[nodiscard]] kit::BodyIndex leaf_body() const noexcept { return leaf_; }
    [[nodiscard]] kit::BodyIndex shackle_body() const noexcept { return shackle_; }
    [[nodiscard]] kit::RopeIndex chain() const noexcept { return chain_; }

    [[nodiscard]] State capture() const noexcept;
    // After the kit's own restore: the motors as they were.
    void restore(const State &state);

private:
    JPH::PhysicsSystem &system_;
    kit::Kit &kit_;
    kit::BodyIndex frame_;
    kit::BodyIndex leaf_;
    kit::BodyIndex guard_;
    kit::BodyIndex fan_;
    kit::BodyIndex shackle_;
    kit::AnchorIndex eye_;
    kit::AnchorIndex cleat_;
    kit::RopeIndex chain_;
    JPH::Ref<JPH::HingeConstraint> leaf_hinge_;
    JPH::Ref<JPH::HingeConstraint> guard_hinge_;
    JPH::Ref<JPH::HingeConstraint> fan_hinge_;
    std::unique_ptr<SharedSurvival> district_;
    std::size_t leaf_channel_ = 0;
    std::size_t fan_channel_ = 0;
    float restart_s_ = 0.0F;
    bool was_hands_ = false;
    View view_;
};

} // namespace scraperx::sim
