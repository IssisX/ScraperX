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
// walking, creeping) against how far it has to carry to her; sight at close
// range with a clear line. She runs at kHuntMps: faster than the player
// walks, slower than the sprint, so the sprint that outruns her is also the
// loud thing that calls her.
//
// A sound carries through the tower as it would (the owner, 2026-10-18: she
// is not level-bound). The tower is an open frame, each level a deck band
// round the open shaft, so a sound reaches her the shorter of two ways: the
// straight line, kDeckMuffle times as far for each deck it passes through; or
// out into the shaft, along it and in again, kBendMuffle times as far for each
// end that is not in the shaft. Where it comes from is a lead on that level
// (the deck under it), not hers; one more than a level away she goes after a
// level at a time, by the next way toward it (a line down, a climb up).
//
// A catch (her hands within kCatchM of the player) is a death, restoring the
// last checkpoint like a lethal fall. The checkpoint stops being renewed once
// she is released (otherwise it would lie just ahead of her, a death loop);
// the one at the release is where a death returns the player, to a Matron
// reset at her locker.
//
// Her mind (the owner, 2026-10-18: A two tiers, B meddling that can be
// undone, C tricks learned within a run). She knows the deck's way out: Stage
// A's cage. Losing the player, she goes and stands in its mouth, silent
// (Guarding): the player must draw her off it. A noise she ran to and found
// nobody at makes her wary: the next she checks from where she can see the
// place, kVantageM short of it (Peering), before going on; once fooled twice,
// a noise farther than kWaryNoiseM from her post does not draw her off it at
// all, she only turns to it. Sight, or a noise close by, always does. What
// she learns lasts the run: a restore (a death, a load) forgets it. In the
// Turn she vaults what blocks her way (kVaultUpMps); frail, she shoves it.
//
// Off her deck (decision 9's A: frail, she rides the lifts like anyone). A
// floor that moves under her (a cage she stepped into as it left) she clings
// to: no step, no grab, no Turn, until it has stopped and kLetGoSeconds more.
// Then she hunts the level it stopped at, on that level's walking lines
// (Level). She never walks off an edge; frail, she cannot cross a gap, so the
// 1.1 m from Stage A's cage at its top to the 176 ring keeps her on the cages.
// In the Turn she leaps a gap (kLeapUpMps) as she vaults a crate. Each level
// has the way up she guards: A's cage on her deck, B's on the 176 ring.
//
// Lines: a line's one trolley leaves with its rider, and from her deck the
// only other way down (the riser ladder) is not taken from the top, so frail
// she never rides a line after the player. Seeing the player ride one, she
// guesses where they are going: its far end. In the Turn she goes after
// them: up the stand, a jump for the cable, and down it on her forearms
// (Zipline::clip_other, on the bare cable, kCableFriction her drag on it), to
// let go over the far band and hunt the deck below.
//
// Climbs (A again, the Turn's parkour): a ladder from her level up to the
// next (the 154 m deck's to the 176 ring). Seen or heard on it, the player
// is where it tops out (her guess where they are going). Frail, she cannot
// take hold of it: she stands at its foot looking up, and gives up there. In
// the Turn she takes hold, and then does not let go: up rung by rung at
// kClimbMps, in jerks (kClimbBurstMps) while a Turn has her, her hands
// holding her weight, and over its head onto the floor above.
//
// Meddling that can be undone (decision 9's B): a way up the player has
// rigged (its rope's end on the cage's eye) and left, she goes into the cage
// at her post and takes the rope off the eye (Unrigging): kUnrigSeconds of her
// hands at it, and the shackle hangs at the eye on its rope. Hooked on again,
// the cage rides as before. She never sends a cage away.
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

class Zipline;

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
        Guarding = 7,  // standing in the way out's mouth, silent, listening
        Peering = 8,   // stopped short of a noise, looking for the player there
        Unrigging = 9, // at the way out's eye, taking the player's rope off it
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

    // Sound through the tower (above). CHOSEN: a steel deck takes 20 dB off
    // a sound (as ten times the distance would); turning round the shaft's
    // edge, the low thuds she hears take 3 dB (1.4 times the distance). The
    // shaft is the stack's (x +-17 m, z -133 / -167), the narrowest of the
    // tower's: the rings above leave a wider one round it.
    static constexpr float kDeckMuffle = 10.0F;
    static constexpr float kBendMuffle = 1.4F;
    static constexpr float kShaftHalfM = 17.0F;

    // The build-up.
    static constexpr float kWakeNoiseM = 30.0F;     // a loud noise carrying this far to the locker wakes her
    static constexpr float kStrainSeconds = 16.0F;
    static constexpr float kEmergeSeconds = 2.2F;

    // Her speeds (m/s) and feet (m/s^2). The player walks to 5.5, sprints to 8.
    static constexpr float kShuffleMps = 1.0F;
    static constexpr float kSearchMps = 2.4F;
    static constexpr float kHuntMps = 6.8F;
    static constexpr float kFootAccel = 9.0F;
    static constexpr float kCutAccel = 30.0F;   // braking and cutting sideways
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
    // Below this she is gone (fell): a fall of 12 m from the deck. Riding
    // (a moving floor, a line's cable) or on a climb she is not falling.
    static constexpr float kFallenBelowM = 12.0F;

    // The way out she guards: the mouth of Stage A's cage (its open north
    // side; the deck's spawn is just north of it), while the cage is down.
    static constexpr float kPostX = -10.3F;
    static constexpr float kPostZ = -129.0F;
    // Wary: how far short of a noise she stops to look, how long she looks,
    // and how close a noise must be to draw her off her post once she has
    // been fooled twice (a sprint is heard 44 m off, a walk 16 m).
    static constexpr float kVantageM = 6.0F;
    static constexpr float kPeerSeconds = 2.5F;
    static constexpr float kWaryNoiseM = 12.0F;
    // The Turn's vault: blocked by something lower than her chest, she jumps
    // it (a 1.4 m rise) rather than shoving it.
    static constexpr float kVaultUpMps = 5.2F;
    static constexpr float kVaultForwardMps = 4.0F;
    static constexpr float kVaultCooldownS = 0.8F;
    // The Turn's leap: a floor that ends ahead of her with floor again within
    // reach beyond (kLeapReachM) at about her height and nothing in the way at
    // her head, she jumps the gap.
    static constexpr float kLeapUpMps = 3.0F;
    static constexpr float kLeapForwardMps = 5.0F;
    static constexpr float kLeapReachM = 3.0F;
    // Riding: her floor moving up or down faster than kRideMps, she clings;
    // stopped, she lets go kLetGoSeconds later.
    static constexpr float kRideMps = 0.3F;
    static constexpr float kLetGoSeconds = 1.0F;
    // An edge: no floor within kEdgeDropM below her feet ahead of her. She
    // keeps kEdgeMarginM from one.
    static constexpr float kEdgeDropM = 1.2F;
    static constexpr float kEdgeMarginM = 0.25F;
    // A gap no wider than this (the 0.3 m between two cages) she steps over.
    static constexpr float kStepOverM = 0.45F;
    // The cable: her hands over her head (her centre to them), how near the
    // cable they take it, her jump for it off a line's stand, and her
    // forearms' friction on it going down (cloth and skin on steel, on her
    // weight across the cable): about half a metre a second gained each
    // second down the steepest line, where a trolley gains three.
    static constexpr float kHandsAboveCentre = 1.55F;
    static constexpr float kCableReachM = 0.6F;
    static constexpr float kCableJumpMps = 3.9F;
    static constexpr float kCableFriction = 0.3F;
    // Letting go of a cable at its end, she swings on this fast over the band.
    static constexpr float kLetGoSwingMps = 1.6F;
    // A climb: her hands' steady pull up it (the player climbs at 0.9), the
    // Turn's jerks on it and how hard they snap, and her mantle over its head
    // (on until her middle is this far out over the floor above).
    static constexpr float kClimbMps = 0.6F;
    static constexpr float kClimbBurstMps = 2.2F;
    static constexpr float kClimbAccel = 70.0F;
    static constexpr float kMantleMps = 1.4F;
    static constexpr float kMantleOutM = 1.0F;
    // How long her hands are at a rigged cage's eye before the rope is off it.
    static constexpr float kUnrigSeconds = 2.0F;
    // The 176 ring (world_solids.inc, UpperDeck): its band's middle line, and
    // the cages at its north edge: Stage A's at the top of its travel, B's at
    // the bottom of its, their open north sides 1.09 m off the ring's edge.
    static constexpr float kRingFloor = 176.25F;
    static constexpr float kRingX = 23.09F;
    // The rings above it: one every kRingPitch, each band's middle line
    // kRingStepIn nearer the shaft's than the last, up to the tenth.
    static constexpr float kRingPitch = 22.0F;
    static constexpr float kRingStepIn = 0.91F;
    static constexpr int kUpperRings = 10;
    // The stack's decks under hers: one every kStackPitch from the first,
    // their bands laid out as hers.
    static constexpr float kStackPitch = 11.0F;
    static constexpr int kStackDecks = 13;
    static constexpr float kRingNorthZ = -126.91F;
    static constexpr float kRingSouthZ = -173.09F;
    static constexpr float kCageACenterX = -10.5F;
    static constexpr float kCageBCenterX = -7.2F;
    static constexpr float kCagesCenterZ = -131.4F;
    static constexpr float kLeapX = -11.9F;   // across the open half of A's mouth
    // The ladder from her deck's north band to the 176 ring
    // (band_counterweight_well.cpp: its rungs on x 5.00, z -129.05): where she
    // hangs on it (the player's foot spot, 0.5 m off the rungs), and the way
    // to there from the band, round its west rail.
    static constexpr float kLadderX = 5.0F;
    static constexpr float kLadderFootZ = -129.55F;
    static constexpr float kLadderSideX = 3.9F;

    // The Turn: now and then, on the hunt, something in her breaks. She stops
    // dead (the wind-up: a spine cracking, a head turning the wrong way, the
    // one warning), then lunges in jerks, bursts faster than a sprint and
    // stumbling pauses, with feet that snap rather than step. It starts when
    // she has a clear line to the player within kTurnTriggerM, or the player
    // lands hard with the noise carrying under kTurnNoiseM to her; then not
    // again for a while. It
    // is no faster on the whole than her usual hunt, only harder to read.
    enum class Turn : std::uint8_t {
        None = 0,
        Windup = 1,
        Lunge = 2,
    };
    static constexpr float kTurnTriggerM = 13.0F;
    static constexpr float kTurnSightSeconds = 0.5F;
    static constexpr float kTurnNoiseM = 20.0F;
    static constexpr float kTurnNoiseLoudness = 0.95F;   // a landing of 8 m/s or more
    static constexpr float kTurnWindupSeconds = 0.9F;
    static constexpr float kTurnLungeSeconds = 3.2F;
    static constexpr float kTurnCooldownSeconds = 18.0F;
    static constexpr float kTurnCycleSeconds = 0.5F;
    static constexpr float kTurnBurstSeconds = 0.2F;
    static constexpr float kTurnBurstMps = 9.0F;
    static constexpr float kTurnPauseMps = 3.5F;
    static constexpr float kTurnFootAccel = 70.0F;

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
        Turn turn = Turn::None;
        float turn_s = 0.0F;        // seconds into the Turn
        std::uint32_t fooled = 0;   // noises she went to and found nobody at, this run
        bool wary = false;          // checking the present noise from a vantage first
        std::uint32_t vaults = 0;   // things vaulted in the Turn, this run
        std::uint32_t leaps = 0;    // gaps leapt in the Turn, this run
        std::uint32_t unrigs = 0;   // ropes she took off a way out's eye, this run
        bool on_line = false;       // going down a line's bare cable
        std::uint32_t cable_slides = 0;   // lines she went down after the player, this run
        bool climbing = false;      // on a ladder, going up it (stride: metres climbed)
        std::uint32_t climbs = 0;   // ladders she went up after the player, this run
        bool clinging = false;      // holding on to a floor that moves (or just stopped)
        float floor_y = kDeckTop;   // the level she hunts: its floor's top
        float lead_floor_y = kDeckTop;   // the level of her lead (where she heard or saw the player)
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
    // Whether each level's way up is there to guard (Stage A's cage down at
    // her deck, B's down at the 176 ring): given before pre_step. Gone up,
    // there is nothing to guard.
    void set_ways_out(const bool a_down, const bool b_down) noexcept {
        ways_out_ = {a_down, b_down};
    }
    // Whether each way up is rigged to ride (its rope's end on its cage's
    // eye): given before pre_step.
    void set_rigged(const bool a_rigged, const bool b_rigged) noexcept { rigged_ = {a_rigged, b_rigged}; }
    // The lines (and whether the player rides one): given once they are built.
    void set_zipline(Zipline *zipline) noexcept { zipline_ = zipline; }
    // Reports (once) the way up whose rope she has just taken off its eye
    // (0 Stage A, 1 Stage B), or -1: the world unhooks it.
    [[nodiscard]] int take_unrig() noexcept {
        const int way = unrig_way_;
        unrig_way_ = -1;
        return way;
    }
    // A sound in the world she can hear like the player's noise (a hit: a
    // crate crashing down; a machine running), at its loudness (0..1, a sound
    // of 1 carries kFullHearingM): given before pre_step, heard in it. She
    // takes the loudest at her ear of a tick's that is not her own (within
    // kOwnSoundM of her body: what she strikes herself) and carries to her.
    // Past kMaxSounds in a tick the rest are not heard (the world gives the
    // hardest hits first).
    static constexpr std::size_t kMaxSounds = 16;
    static constexpr float kOwnSoundM = 1.5F;
    void hear(float x, float y, float z, float loudness) noexcept {
        if (sound_count_ < kMaxSounds) {
            sounds_[sound_count_++] = {true, x, y, z, loudness};
        }
    }
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
    // How a walking line is taken: on foot, over the catwalk (only in sight
    // of prey on it), or across a gap (only leapt, in the Turn).
    enum class Link : std::uint8_t {
        Floor,
        Catwalk,
        Gap,
    };
    struct Edge {
        int a, b;
        Link link;
    };
    // Where she stands guard, and the way she faces there.
    struct Post {
        float x, z, yaw;
    };
    // A level she can hunt: its floor, its walking lines, the loops she
    // patrols, the posts by its way up (guarded while ways_out_[way]: the
    // first she can walk to is hers; -1, none), and where she stands to
    // reach that cage's eye.
    struct Level {
        float floor_y;
        std::vector<Node> nodes;
        std::vector<Edge> edges;
        std::vector<std::vector<int>> patrols;
        std::vector<Post> posts;
        int way;
        Post eye;
    };
    // A climb she knows (a ladder): where she hangs on it (her middle's
    // line, on the climber's side, a node of its foot's level), the floor it
    // stands on and the one it tops out onto, and the way off its head. (The
    // riser from 143 to her deck is not one: its foot is on a plate out over
    // the shaft behind 143's rail, reached only from the ladder from 132.)
    struct Climb {
        float x, z;
        float foot_y, head_y;
        float out_x, out_z;
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
    // How far a sound at `from` has to carry to an ear at `to` (above); and
    // the decks (static floors) the straight line between passes through.
    [[nodiscard]] float carry_m(JPH::RVec3 from, JPH::RVec3 to) const;
    [[nodiscard]] int decks_between(JPH::RVec3 from, JPH::RVec3 to) const;
    // The floor of the level a sound at height y is on (the highest under
    // it, within a storey), or her own when it is on none.
    [[nodiscard]] float floor_of(float y) const;
    [[nodiscard]] static bool on_catwalk_line(float x, float z, float margin);
    // The next point to walk to, along the cheapest path on her level to
    // (tx, tz), with the catwalk allowed or not and gaps (leapt) or not;
    // false when no such path reaches it (she then heads straight for it).
    bool route_toward(float fx, float fz, float tx, float tz, bool allow_catwalk, float &wx, float &wz,
                      bool allow_gaps = false) const;
    // Her feet toward (wx, wz) at `speed`; `turn_moves` lets her vault and leap.
    void walk_toward(float dt, float wx, float wz, float speed, float foot_accel = kFootAccel,
                     bool turn_moves = false);
    // Her floor this tick: its height under her, whether it moves (she clings).
    void feel_floor(float dt);
    // The level whose floor is under her feet, once she is on one.
    void settle_level();
    // The post on her level she can walk to, if its way out is there to guard.
    [[nodiscard]] const Post *reachable_post(float ax, float az) const;
    // Of her level's patrol loops, the one she can walk onto from here, and
    // its node nearest her.
    void pick_patrol(float ax, float az);
    // After a lead on a level below, down a line toward it (to it, or to a
    // level between): to its stand, and in the Turn onto its cable. False
    // when no line does.
    bool follow_line(float dt, JPH::RVec3 at);
    // On a line's cable: her drag on it, and letting go at its end.
    void ride_line();
    // After a lead on a level above, up a climb toward it (to it, or to a
    // level between): to its foot, and in the Turn onto it. False when no
    // climb does.
    bool follow_climb(float dt, JPH::RVec3 at);
    // On a climb: up it, and over its head.
    void climb(float dt);
    // A lead: a place she heard (or saw) the player, or a crash, on the level
    // whose floor is `floor`, at that loudness.
    void take_lead(float x, float z, float floor, float loudness, bool seen);
    // Back to her post if there is a way out to guard, else the patrol.
    void stand_down();
    // The Turn's clock: `distance_to_player` across, `carried_m` how far the
    // player's noise carried to her.
    void step_turn(float dt, float distance_to_player, float loudness, float carried_m);
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
    std::vector<Level> levels_;
    int level_ = 0;
    int patrol_ = 0;   // which of her level's loops, and how far round it

    State state_ = State::Dormant;
    float strain_s_ = 0.0F;
    float emerge_s_ = 0.0F;
    float since_heard_s_ = 0.0F;
    float search_s_ = 0.0F;
    float heard_x_ = 0.0F, heard_z_ = 0.0F;
    float stride_ = 0.0F;
    float speed_ = 0.0F;
    float yaw_ = 0.0F;
    int patrol_node_ = 0;
    float noise_heard_ = 0.0F;
    bool sees_player_ = false;
    bool sees_far_ = false;
    float target_x_ = 0.0F, target_z_ = 0.0F;
    bool allow_catwalk_ = false;
    float commit_s_ = 0.0F;
    Turn turn_ = Turn::None;
    float turn_s_ = 0.0F;
    float turn_cooldown_s_ = kTurnCooldownSeconds * 0.5F;
    float sight_far_s_ = 0.0F;
    bool released_edge_ = false;
    bool player_on_deck_ = false;
    struct Sound {
        bool heard = false;
        float x = 0.0F, y = 0.0F, z = 0.0F;
        float loudness = 0.0F;
    };
    std::array<Sound, kMaxSounds> sounds_{};
    std::size_t sound_count_ = 0;
    bool dead_ = false;
    // Her floor (feel_floor): its top under her, and riding it.
    float feet_y_ = kDeckTop;
    bool riding_ = false;
    float let_go_s_ = 0.0F;
    std::uint32_t leaps_ = 0;
    // Lines: the lines, the one whose far end she guessed, the floor of her
    // lead (another level's, guessed down a line), and her going down one.
    Zipline *zipline_ = nullptr;
    int guess_line_ = -1;
    float lead_floor_ = kDeckTop;
    bool on_line_ = false;
    std::uint32_t cable_slides_ = 0;
    // Climbs: the ones she knows, the one whose head she guessed, the one
    // she is on (-1, none) and whether she is over its head.
    std::vector<Climb> climbs_;
    int guess_climb_ = -1;
    int climb_ = -1;
    bool mantling_ = false;
    std::uint32_t climbs_done_ = 0;
    // Her mind (see above): what she has learned this run.
    std::array<bool, 2> ways_out_{true, true};
    std::array<bool, 2> rigged_{false, false};
    float unrig_s_ = 0.0F;
    int unrig_way_ = -1;
    std::uint32_t unrigs_ = 0;
    bool lead_seen_ = false;
    bool wary_ = false;
    float peer_s_ = 0.0F;
    std::uint32_t fooled_ = 0;
    float vault_cooldown_s_ = 0.0F;
    std::uint32_t vaults_ = 0;
    // A far noise she let go while guarding: she turns to it a moment.
    float look_x_ = 0.0F, look_z_ = 0.0F;
    float look_s_ = 0.0F;
    std::uint64_t catches_ = 0;
    float plank_peak_n_ = 0.0F;
    float planks_clock_ = 0.0F;
    View view_{};
};

} // namespace scraperx::sim
