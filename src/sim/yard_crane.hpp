#pragma once

// The yard crane (the owner, 2026-10-17: a big crane that lifts the forklift,
// the containers and the stock onto the structure, and nothing heavy up there
// that it or a forklift could not really have put there). A hammerhead tower
// crane stands on its foundation west of the stack: a lattice mast 58 m tall,
// and on its slewing ring the jib reaching 38.5 m toward the stack, the
// counter-jib and its concrete counterweights behind, the tower head and its
// pendant bars over both, the cab at the jib's root. A trolley runs out and in
// along the jib's bottom chords, and from it the hoist rope carries the hook
// block. From the hook hangs a chain sling ending in a shackle.
//
// It is worked by radio, from anywhere in the yard: the remote hangs in a
// cabinet at the mast's foot. Taken, it stays in the hand. CRANE (the
// remote's switch) puts the stick and the two held buttons on the crane: the
// stick sends the hook where it points in the view (the slew and the trolley
// share the move), RAISE and LOWER hoist; CRANE again lets go, and every
// drive holds where it stands. A load is rigged by hand: the sling's shackle
// is carried to a lifting eye and HOOKed on (the rigging the tower's machines
// use), and UNHOOKed once the load rests.
//
// Nothing is scripted. The slewing assembly is one body on the mast's
// bearing, turned by its slew drive (a motor of finite torque, to its rate at
// its acceleration); the trolley is a body on the jib, run by its drive; the
// hoist is a winch on the rope, of finite pull, so a load too heavy for it
// stalls it on the ground. A load hangs on its rope and swings. The winch
// stops at its upper limit (the hook block short of the trolley) and pays out
// only while the rope is loaded (a slack-rope switch), as a crane's do. The
// jib never reaches the stack: its tip stops 1.5 m short of the west face at
// the jib's height, and the hook lands loads on three docks cantilevered out
// of the face at 11, 22 and 44 m, set so that none hangs over another and each
// meets its deck where the face's braces leave room to drive in. The
// footbridge's pylon stands inside the jib's circle, taller than the crane:
// a slewing limiter (a crane's zoning) stops the jib short of it either way,
// and the counter-jib is short enough to pass it.
//
// Every body is the kit's, saved and restored with the machines. A restore
// lets go of the controls and holds every drive where it stands.

#include "sim/mechanism_kit.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <array>
#include <cstdint>

namespace scraperx::sim {

class YardCrane final {
public:
    // Static: the mast on its foundation, the remote's cabinet, the docks.
    static constexpr std::uint64_t kMastEntity = 1700;
    static constexpr std::uint64_t kCabinetEntity = 1701;
    static constexpr std::uint64_t kDockFirstEntity = 1702;
    // Moving: the slewing assembly, the trolley, the hook block, the sling's
    // shackle; and the remote, static in its cabinet until taken.
    static constexpr std::uint64_t kJibEntity = 2984;
    static constexpr std::uint64_t kTrolleyEntity = 2985;
    static constexpr std::uint64_t kHookEntity = 2986;
    static constexpr std::uint64_t kShackleEntity = 2987;
    static constexpr std::uint64_t kRemoteEntity = 2988;

    // The mast's axis, and the top of its slewing ring.
    static constexpr float kMastX = -66.0F;
    static constexpr float kMastZ = -137.0F;
    static constexpr float kSlewY = 58.0F;
    // The trolley runs from kTrolleyMinM to kTrolleyMaxM out from the mast.
    static constexpr float kTrolleyMinM = 5.0F;
    static constexpr float kTrolleyMaxM = 37.0F;
    // The hoist rope from the trolley's sheaves to the hook block's: at
    // least kHoistMinM (the upper limit), at most kHoistMaxM (the hook at
    // the ground).
    static constexpr float kHoistMinM = 2.5F;
    static constexpr float kHoistMaxM = 57.0F;

    // The drives (CHOSEN, a heavy hammerhead's): the slew to kSlewMaxRadS
    // (0.76 rpm) at kSlewAccelRadS2 with at most kSlewTorqueNm; the trolley
    // to kTrolleyMaxMps at kTrolleyAccelMps2 with at most kTrolleyForceN; the
    // hoist at kHoistMps with a pull of at most kHoistForceN at the hook (its
    // reeving's), enough for the forklift with its driver and not for the
    // 8 t plate stack. The stick sends the hook at up to kHookMps.
    static constexpr float kSlewMaxRadS = 0.08F;
    static constexpr float kSlewAccelRadS2 = 0.06F;
    static constexpr float kSlewTorqueNm = 2.5e6F;
    static constexpr float kTrolleyMaxMps = 1.2F;
    static constexpr float kTrolleyAccelMps2 = 0.6F;
    static constexpr float kTrolleyForceN = 40000.0F;
    static constexpr float kHoistMps = 1.0F;
    // The hoist's drive spools up at kHoistAccelMps2 and stops at
    // kHoistDecelMps2 (CHOSEN: to full speed in half a second, to rest in a
    // third), as a real winch's does: started or stopped in one tick, the
    // 770 kg block was jerked by 1 m/s (a 352 J hit in the impact ledger,
    // heard by the Matron), and held at the upper limit it switched on and
    // off every few ticks. Raising, it eases to rest at the upper limit;
    // lowering, at the drum's last turns.
    static constexpr float kHoistAccelMps2 = 2.0F;
    static constexpr float kHoistDecelMps2 = 3.0F;
    static constexpr float kHoistForceN = 75000.0F;
    static constexpr float kHookMps = 2.5F;
    // The slack-rope switch: lowering pays out no further than this past
    // the rope's length as it lies.
    static constexpr float kSlackPayOutM = 0.3F;
    // The slewing limiter: the jib's heading never enters this sector (the
    // footbridge's pylon, 15.9 to 19.9 m from the mast between headings 1.926
    // and 2.150, widened by the jib's half-width there and a margin). The
    // drive is cut early enough to brake to rest at the sector's edge.
    static constexpr float kZoneLowRad = 1.806F;
    static constexpr float kZoneHighRad = 2.27F;
    // The sling, from the hook's throat to its shackle.
    static constexpr float kSlingM = 3.2F;
    // The remote is taken within kRemoteReachM of its cabinet, and works
    // within kRadioRangeM of the mast.
    static constexpr float kRemoteReachM = 1.6F;
    static constexpr float kRadioRangeM = 400.0F;

    // The devices hung from the sling (to-do 10): each a body with a lifting
    // eye, rigged as any load is, by HOOKing the sling's shackle on its eye,
    // so choosing one is rigging it. While the crane is worked, the remote's
    // device switch works the one on the sling; unhooked, it is dead.
    //
    // The lifting magnet (CHOSEN: a 1.2 m electro-lifting magnet): on, it
    // takes hold of steel lying flat against its face. Of the face's nine
    // points (its middle and eight round it), those over a steel part's face
    // turned to it within kMagnetTiltRad, from kMagnetGapM off it to 2 cm
    // into it, are in contact; with the middle and kMagnetMinContact in all,
    // it holds that body, kMagnetRatedN times the share in contact (a magnet
    // holds by the steel under it), and past that for two ticks the load
    // tears off; torn off, it takes hold again only once it has been clear
    // of the steel. It takes hold as its middle touches (within
    // kMagnetTouchM), flat: within kMagnetTiltRad, the tilt that keeps the far
    // edge of the ring within kMagnetGapM, so touched down on an edge as it
    // swings, it takes hold only as it comes flat. Off, it lets go wherever
    // it is.
    static constexpr std::uint64_t kMagnetEntity = 2995;
    static constexpr float kMagnetRadiusM = 0.6F;
    static constexpr float kMagnetKg = 1100.0F;
    static constexpr float kMagnetRatedN = 49000.0F;
    static constexpr float kMagnetGapM = 0.04F;
    static constexpr float kMagnetTouchM = 0.01F;
    static constexpr float kMagnetTiltRad = 0.035F;
    static constexpr int kMagnetMinContact = 3;
    // Thin sheet (a container's corrugated walls, kit::Kit::set_hollow) holds
    // a magnet's flux poorly: a tenth of the hold.
    static constexpr float kMagnetSheetShare = 0.1F;

    // The container spreader (CHOSEN: a 40 ft top-lift frame of kSpreaderKg):
    // a frame a 40 ft box's length and width, a twistlock in a pocket at each
    // corner over where a box's corner castings are. Let down on a box, its
    // four twistlocks within kTwistlockReachM of the four castings, square to
    // it within kSpreaderSquareRad and sitting on its roof, LOCK turns them;
    // UNLOCK turns them back. Either only with the box at rest and the rope
    // slack (the sling carrying under kSpreaderSlackShare of the frame's
    // weight): the interlock every spreader has, so a box in the air is
    // never let go. Locked, frame and box are one (kTwistlockRatedN each).
    static constexpr std::uint64_t kSpreaderEntity = 2996;
    static constexpr float kSpreaderKg = 1980.0F;
    static constexpr float kTwistlockX = 6.006F;   // out from its middle along it
    static constexpr float kTwistlockZ = 1.131F;   // and across it
    static constexpr float kTwistlockReachM = 0.12F;
    static constexpr float kSpreaderSquareRad = 0.05F;
    static constexpr float kSpreaderSitM = 0.05F;
    static constexpr float kSpreaderSlackShare = 0.25F;
    static constexpr float kTwistlockRatedN = 245000.0F;

    // The docks: the face out to x_out, between z_min and z_max, their decks'
    // tops at top_y; the low one is a pier on two legs, reaching past the
    // machine hall that overhangs its inner end.
    struct Dock {
        float x_out;
        float z_min, z_max;
        float top_y;
        bool legs;
    };
    static constexpr float kFaceX = -26.0F;
    static constexpr int kDocks = 3;
    static constexpr std::array<Dock, kDocks> kDockSpecs{{
        {-38.5F, -149.2F, -143.8F, 11.0F, true},
        {-33.0F, -129.4F, -125.0F, 22.0F, false},
        {-33.0F, -142.9F, -129.6F, 44.0F, false},
    }};

    struct State {
        bool has_remote = false;
        bool operating = false;
        float slew_rad = 0.0F;        // the jib's heading: 0 along +x, + toward -z (about +y)
        float slew_rate = 0.0F;       // as driven, rad/s
        float trolley_m = 0.0F;       // out from the mast
        float trolley_rate = 0.0F;    // as driven, m/s (+ out)
        float hoist_m = 0.0F;         // the rope as it lies, trolley to hook block
        float hoist = 0.0F;           // the hoist as worked: + raising, - lowering
        float hoist_rate = 0.0F;      // the drum as driven, m/s of rope: + in, - out
        float hoist_n = 0.0F;         // the rope's pull this tick
        bool hoist_stalled = false;   // raising, at the winch's full pull, and not rising
        bool upper_limit = false;     // the hook block at its upper limit
        bool slack = false;           // lowering stopped by the slack-rope switch
        std::uint64_t load_entity = 0;   // what the sling is hooked onto, 0 for none
        // What the parts are doing, measured after the step (for what they
        // sound like): the jib's turn and the slewing drive's torque; the
        // trolley's run along the jib and its drive's pull; the rope coming
        // in (+) or paying out; the hook block's speed; the sling's pull.
        float slew_measured = 0.0F;   // rad/s about +y
        float slew_torque_nm = 0.0F;
        float trolley_mps = 0.0F;     // + out
        float trolley_force_n = 0.0F;
        float rope_mps = 0.0F;        // + coming in
        float hook_mps = 0.0F;
        float sling_n = 0.0F;
        // The magnet's switch, and what the device on the sling holds (0
        // nothing): the hold's pull this tick and what it holds to.
        bool magnet_on = false;
        bool magnet_torn = false;     // a load tore off it, and it has not been clear of steel since
        std::uint64_t magnet_holding = 0;   // what the magnet holds, rigged or not
        std::uint64_t holding_entity = 0;
        float hold_n = 0.0F;
        float hold_rated_n = 0.0F;
        // The spreader: its twistlocks over a box's castings and sitting on
        // it; and landed, what it sits on or holds at rest, the rope slack.
        bool spreader_seated = false;
        bool spreader_landed = false;
    };

    YardCrane(JPH::PhysicsSystem &physics, kit::Kit &kit);
    ~YardCrane();
    YardCrane(const YardCrane &) = delete;
    YardCrane &operator=(const YardCrane &) = delete;

    // Takes the remote from its cabinet when `player` is within reach of it.
    bool take_remote(JPH::RVec3 player);
    [[nodiscard]] bool remote_in_reach(JPH::RVec3 player) const;
    [[nodiscard]] bool has_remote() const;

    // After every other kit body, so saves made before them line up: the
    // devices, laid out in the yard.
    void build_devices();

    // Before the physics step. toggle: the remote's switch. able: the
    // player could work it now (hands free, within range).
    // hook_x, hook_z: where the stick sends the hook (world, -1 .. 1 each,
    // length at most 1). hoist: -1 lowering .. 1 raising. device: the
    // remote's device switch, worked while the crane is. carried: the
    // entity in the player's hands (0 for none), which no device takes.
    void pre_step(float delta_seconds, bool toggle, bool able, float hook_x, float hook_z, float hoist, bool device,
                  std::uint64_t carried);
    // After the physics step: the rope's pull and the drives as they went.
    void post_step(float delta_seconds);

    [[nodiscard]] bool operating() const noexcept { return state_.operating; }
    [[nodiscard]] const State &state() const noexcept { return state_; }
    // The hook block and the magnet, as kit bodies.
    [[nodiscard]] kit::BodyIndex hook_body() const noexcept { return hook_; }
    [[nodiscard]] kit::BodyIndex magnet_body() const noexcept { return magnet_; }
    [[nodiscard]] kit::BodyIndex spreader_body() const noexcept { return spreader_; }
    // Its drives as heard (for the Matron's hearing, matron.hpp hear): where
    // each is and how loud while it runs, 0 when it does not. The slewing
    // ring's motors; the hoist's drum on the counter-jib (louder under load,
    // loudest stalled); the trolley's wheels at the rope's top.
    struct Noise {
        float x = 0.0F, y = 0.0F, z = 0.0F;
        float loudness = 0.0F;
    };
    [[nodiscard]] std::array<Noise, 3> noises() const;
    // The hoist rope's ends, on the trolley and on the hook block.
    [[nodiscard]] JPH::RVec3 rope_top() const;
    [[nodiscard]] JPH::RVec3 rope_bottom() const;
    [[nodiscard]] JPH::RVec3 hook_position() const;

    // After the kit's restore: the controls let go, every drive held where
    // it stands, the rope as long as it lies.
    void restore();

private:
    [[nodiscard]] float rope_length_now() const;
    // A point on one of the crane's bodies, in the world.
    [[nodiscard]] JPH::RVec3 on(kit::BodyIndex body, JPH::Vec3 local) const;
    // The body the magnet's face lies flat against, invalid for none, and
    // how many of the face's nine points are in contact with its steel.
    [[nodiscard]] kit::BodyIndex magnet_contact(std::uint64_t carried, int &points) const;
    // The magnet: its switch, and a hold made while it is on and lies flat
    // against steel. Unhooked, it is dead.
    void work_magnet(bool rigged, bool flip, std::uint64_t carried);
    // The box the spreader's twistlocks sit over, invalid for none.
    [[nodiscard]] kit::BodyIndex spreader_seat() const;
    // The spreader: where it sits, and LOCK or UNLOCK when flipped.
    void work_spreader(bool rigged, bool flip);
    // One of the crane's own bodies or devices, which no device takes.
    [[nodiscard]] bool own(kit::BodyIndex body) const noexcept;

    JPH::PhysicsSystem &physics_;
    kit::Kit &kit_;
    kit::BodyIndex mast_{};
    kit::BodyIndex jib_{};
    kit::BodyIndex trolley_{};
    kit::BodyIndex hook_{};
    kit::BodyIndex shackle_{};
    kit::BodyIndex remote_{};
    kit::BodyIndex cabinet_{};
    kit::BodyIndex magnet_{};
    kit::GripIndex magnet_grip_{};
    kit::BodyIndex spreader_{};
    kit::GripIndex spreader_grip_{};
    kit::RopeIndex sling_{};
    JPH::Ref<JPH::HingeConstraint> slew_;
    JPH::Ref<JPH::SliderConstraint> trolley_drive_;
    JPH::Ref<kit::RopeConstraint> hoist_;
    State state_{};
};

} // namespace scraperx::sim
