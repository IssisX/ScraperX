#pragma once

// The Matron (owner's chase, 2026-10-16): a sealed locker on the 154 m deck
// that wakes to noise, strains for a few seconds, bursts, and then hunts the
// player by sound. She is a real Jolt body (like the yard crew: an upright
// capsule moved only by a bounded foot acceleration), never a script: nothing
// teleports her and her route is a fixed rule over the deck's walkways.
//
// The deck is a loop 9 m wide round a 34 m open shaft; across the shaft runs
// a rotten catwalk of bolted plates. A plate hangs on a bolt that shears under
// a sustained load: a climber crosses it, the Matron (more than twice the
// weight, and stamping) brings it down under her and falls the height of the
// tower. That is the one thing that kills her. She only steps onto the
// catwalk when she can see her prey close ahead of her on it; otherwise she
// takes the long way round.
//
// Senses (Hearing): how loud the player is this tick (sprinting, landing,
// walking, creeping) against how far she is; sight at close range with a
// clear line. She runs at kHuntMps: faster than the player walks, slower than
// the sprint, so the sprint that outruns her is also the loud thing that
// calls her.
//
// A catch (her hands within kCatchM of the player) is a death, restoring the
// last checkpoint like a lethal fall. The checkpoint stops being renewed once
// she is released (otherwise it would lie just ahead of her, a death loop);
// the one at the release is where a death returns the player, to a Matron
// reset at her locker.
//
// Only simulation state decides anything here (no clock, no randomness): the
// same start and inputs give the same chase.

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <array>
#include <cstdint>
#include <vector>

namespace scraperx::sim {

class Matron final {
public:
    enum class State : std::uint8_t {
        Dormant = 0,   // sealed, listening
        Straining = 1, // the gauge climbing, the locker rattling
        Emerging = 2,  // out of the locker, not yet hunting
        Hunting = 3,   // after the player, at kHuntMps
        Searching = 4, // lost the player: to the last sound, then about
        Shuffling = 5, // unaware: the stooped patrol
        Dead = 6,      // fell
    };

    static constexpr std::uint64_t kEntity = 3200;
    static constexpr std::uint64_t kPlankEntityBase = 3210;

    // The deck (the stack's top, 154.00): outer edge x +-26, z -124 / -176;
    // the shaft x +-17, z -133 / -167. Walking lines run down the band middles.
    static constexpr float kDeckTop = 154.0F;
    static constexpr float kBandX = 21.5F;
    static constexpr float kBandNorthZ = -128.5F;
    static constexpr float kBandSouthZ = -171.5F;
    static constexpr float kMidZ = -150.0F;

    // The locker: against the north band's outer rail, east of the arrival.
    static constexpr float kLockerX = 11.0F;
    static constexpr float kLockerZ = -126.2F;
    // The locker is a solid steel cabinet (1.2 x 2.2 x 0.9 m) whose door
    // faces the shaft; she steps out of its front.
    static constexpr std::uint64_t kLockerEntity = 3201;
    static constexpr float kLockerHalfX = 0.6F;
    static constexpr float kLockerHalfY = 1.1F;
    static constexpr float kLockerHalfZ = 0.45F;
    static constexpr float kSpawnZ = kLockerZ - kLockerHalfZ - 0.5F;

    // Her body: 2.1 m tall standing, 220 kg.
    static constexpr float kMassKg = 220.0F;
    static constexpr float kRadius = 0.30F;
    static constexpr float kHalfHeight = 0.75F;
    static constexpr float kStandCentre = kHalfHeight + kRadius;

    // The build-up.
    static constexpr float kWakeNoiseM = 30.0F;     // a loud noise this near the locker wakes her
    static constexpr float kStrainSeconds = 16.0F;
    static constexpr float kEmergeSeconds = 2.2F;

    // Her speeds (m/s) and feet (m/s^2). The player walks to 5.5, sprints to 8.
    static constexpr float kShuffleMps = 1.0F;
    static constexpr float kSearchMps = 2.4F;
    static constexpr float kHuntMps = 6.8F;
    static constexpr float kFootAccel = 9.0F;
    static constexpr float kCatchM = 0.85F;
    static constexpr float kSightM = 9.0F;
    static constexpr float kLoseTrackSeconds = 9.0F;
    static constexpr float kSearchSeconds = 8.0F;

    // The catwalk: 11 plates across the shaft, each on a bolt that shears
    // when its load, averaged over kBoltWindowS, passes kBoltShearN. The plate
    // itself is 0.59 kN; with a climber's 80 kg (0.8 kN, a hop's landings a
    // little over) it carries about 1.4 kN, with her (2.2 kN standing, and
    // she stamps) over 2.8 kN.
    static constexpr int kPlanks = 11;
    static constexpr float kPlankLength = 3.08F;  // pitch: 11 plates leave a 6 cm gap at each end of the 34 m shaft
    static constexpr float kPlankWidth = 1.6F;
    static constexpr float kPlankThick = 0.08F;  // top flush with the deck; its underside clears the shaft-edge beam (top 153.90)
    static constexpr float kPlankMassKg = 60.0F;
    static constexpr float kBoltShearN = 2100.0F;
    static constexpr float kBoltWindowS = 0.25F;
    // She takes the catwalk only with her prey in plain sight (a straight line
    // across the shaft) at most this far, standing on it or within kEndM of
    // either end of it.
    static constexpr float kCommitSightM = 50.0F;
    static constexpr float kEndM = 5.0F;
    static constexpr float kCommitHoldSeconds = 2.5F;
    // Below this she is gone (fell): a fall of 12 m from the deck.
    static constexpr float kFallenBelowM = 12.0F;

    struct PlankView {
        float x = 0.0F, y = 0.0F, z = 0.0F;
        float qx = 0.0F, qy = 0.0F, qz = 0.0F, qw = 1.0F;
        bool bolted = true;
        bool gone = false;   // fallen far enough to be taken out
    };

    struct View {
        State state = State::Dormant;
        float x = 0.0F, y = 0.0F, z = 0.0F;
        float yaw = 0.0F;
        float speed = 0.0F;
        float stride = 0.0F;        // metres walked: the gait's phase
        float strain = 0.0F;        // 0..1, the gauge
        float bolts_popped = 0.0F;  // how many of the locker's bolts are gone, 0..4
        float noise_heard = 0.0F;   // 0..1, her last tick's loudest sound, for sound and look
        bool alive = true;
        bool released = false;
        bool sees_player = false;
        float target_x = 0.0F, target_z = 0.0F;   // where her feet are headed
        bool catwalk_allowed = false;
    };

    // What the checkpoint keeps of her (her state, not her body: a restore
    // puts her back at her locker at that stage). Once dead she stays dead.
    struct Saved {
        State state = State::Dormant;
        float strain_s = 0.0F;
    };

    Matron(JPH::PhysicsSystem &physics, std::uint32_t static_layer, std::uint32_t moving_layer, JPH::BodyID player);
    ~Matron();
    Matron(const Matron &) = delete;
    Matron &operator=(const Matron &) = delete;

    // `loudness`: 0..1 from the player's movement this tick (see the .cpp);
    // `player` is the body's centre.
    void pre_step(float dt, JPH::RVec3 player, float loudness);
    void post_step(float dt);

    // True once her hands are on the player.
    [[nodiscard]] bool caught(JPH::RVec3 player) const noexcept;
    // While true the checkpoint is not renewed (see above): she is out and
    // alive and the player is on her deck. A player who has left the deck (up
    // a machine) is out of her reach and checkpoints as ever.
    [[nodiscard]] bool freezes_checkpoint() const noexcept {
        return view_.released && view_.alive && player_on_deck_;
    }
    // Reports (once) that she has just burst out, so the world commits the
    // checkpoint where the player stands.
    [[nodiscard]] bool take_released_edge() noexcept {
        const bool edge = released_edge_;
        released_edge_ = false;
        return edge;
    }
    void count_catch() noexcept { ++catches_; }
    [[nodiscard]] Saved save() const noexcept { return {state_, strain_s_}; }
    void restore(const Saved &saved);

    [[nodiscard]] const View &view() const noexcept { return view_; }
    [[nodiscard]] const std::array<PlankView, kPlanks> &planks() const noexcept { return plank_views_; }
    [[nodiscard]] int planks_fallen() const noexcept;
    [[nodiscard]] std::uint64_t catches() const noexcept { return catches_; }
    [[nodiscard]] float strain_force_peak_n() const noexcept { return plank_peak_n_; }
    [[nodiscard]] JPH::BodyID body() const noexcept { return body_; }

private:
    struct Node {
        float x, z;
    };
    struct Edge {
        int a, b;
        bool catwalk;
    };
    struct Plank {
        JPH::BodyID body;
        JPH::Ref<JPH::FixedConstraint> bolt;
        float home_x = 0.0F;
        float load_avg_n = 0.0F;
        bool bolted = true;
        bool gone = false;
    };

    void spawn_body();
    void remove_body();
    [[nodiscard]] bool sees(JPH::RVec3 from, JPH::RVec3 to) const;
    [[nodiscard]] static bool on_catwalk_line(float x, float z, float margin);
    // The next point to walk to, along the cheapest path to (tx, tz), with
    // the catwalk allowed or not.
    void route_toward(float fx, float fz, float tx, float tz, bool allow_catwalk, float &wx, float &wz) const;
    void walk_toward(float dt, float wx, float wz, float speed);
    void step_planks(float dt);
    void restore_planks();
    void refresh_view();

    JPH::PhysicsSystem &physics_;
    std::uint32_t moving_layer_;
    JPH::BodyID player_;
    JPH::Ref<JPH::Shape> shape_;
    JPH::BodyID body_;
    JPH::BodyID locker_body_;
    bool body_in_world_ = false;
    std::array<Plank, kPlanks> planks_{};
    std::array<PlankView, kPlanks> plank_views_{};
    std::vector<Node> nodes_;
    std::vector<Edge> edges_;

    State state_ = State::Dormant;
    float strain_s_ = 0.0F;
    float emerge_s_ = 0.0F;
    float since_heard_s_ = 0.0F;
    float search_s_ = 0.0F;
    float heard_x_ = 0.0F, heard_z_ = 0.0F;
    float stride_ = 0.0F;
    float speed_ = 0.0F;
    float yaw_ = 0.0F;
    float patrol_t_ = 0.0F;
    int patrol_node_ = 0;
    float noise_heard_ = 0.0F;
    bool sees_player_ = false;
    bool sees_far_ = false;
    float target_x_ = 0.0F, target_z_ = 0.0F;
    bool allow_catwalk_ = false;
    float commit_s_ = 0.0F;
    bool released_edge_ = false;
    bool player_on_deck_ = false;
    bool dead_ = false;
    std::uint64_t catches_ = 0;
    float plank_peak_n_ = 0.0F;
    float planks_clock_ = 0.0F;
    View view_{};
};

} // namespace scraperx::sim
