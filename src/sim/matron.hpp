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
// holding her weight, and over its head onto the floor above. The riser from
// 143 up to her deck she knows the same way (heard or seen on it, the player
// is going up to her deck), but never takes hold of: her way between those
// two decks is her corner pipe.
//
// Meddling that can be undone (decision 9's B): a way up the player has
// rigged (its rope's end on the cage's eye) and left, she goes into the cage
// at her post and takes the rope off the eye (Unrigging): kUnrigSeconds of her
// hands at it, and the shackle hangs at the eye on its rope. Hooked on again,
// the cage rides as before. She never sends a cage away.
//
// The bay (bay.hpp, shared survival): a cut across her deck's north band,
// spanned by a powered leaf. Her lines cross it on the leaf while it is a
// floor (Link::Leaf). Her trap: on her deck west of the bay, with the player
// across it to the east, she does not cross after them. She goes down onto
// the bracket in the bay and puts her hands under the leaf's west end;
// hands in its district stop its motor, and the leaf comes down onto her
// palms (the district's finite support, shared_survival.hpp). She holds it
// level, as bait: a climber who steps onto it is more than she can hold,
// and it goes down under them, onto her. She lets go when the chain is
// hooked onto it (the chain holds it: her trap is beaten), when the player
// is past it or has fallen, or when she has lost them; then up the bracket's
// rungs and after them. Three times a run at most: a restore forgets.
//
// Only simulation state decides anything here (no clock, no randomness): the
// same start and inputs give the same chase.

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include "sim/matron_traversal.hpp"

#include <array>
#include <memory>
#include <cstdint>
#include <vector>

namespace scraperx::sim {

class Zipline;
class SharedSurvival;

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
        Staggering = 10, // a blow took her balance: her feet catching her up, no hands
        Down = 11,     // a blow took her off her feet: on the steel, no hands, no eyes
        // Her hands on the player, fully changed (matron_hold.cpp): no kill on
        // touch. Her hand goes for the ankle and closes on it (Grab), she
        // walks them upside down to the shaft's rail (Carry), holds them out
        // over the drop (Dangle), plays with the arm (ArmPlay), lets go aimed
        // at a ledge that will not kill them (Release) or sets them back on
        // the deck (Spare).
        Grab = 12,
        Carry = 13,
        Dangle = 14,
        ArmPlay = 15,
        Release = 16,
        Spare = 17,
        // Both the player's arms gone: she has won (matron_hold.cpp, the
        // physics_world fatality); the run ends, and starts again whole.
        Claimed = 18,
    };
    [[nodiscard]] static constexpr bool holding_state(const State state) noexcept {
        return state == State::Grab || state == State::Carry || state == State::Dangle || state == State::ArmPlay ||
               state == State::Spare || state == State::Claimed;
    }
    // Out of her locker and about the world, alive (whatever she is doing).
    [[nodiscard]] static constexpr bool out(const State state) noexcept {
        return state == State::Emerging || state == State::Hunting || state == State::Searching ||
               state == State::Shuffling || state == State::Guarding || state == State::Peering ||
               state == State::Unrigging || state == State::Staggering || state == State::Down ||
               (state >= State::Grab && state <= State::Claimed);
    }

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
    // shaft is the stack's (x +-17 m, z -133 / -167): the rings up to 264 m
    // leave a wider one round it; above them their openings close in (to
    // +-12 m at 396 m), where the way round the shaft would pass through
    // their decks unmuffled (she has no way of her own up there). A deck is
    // a level's floor: a flat face met within kDeckDepthM under its top.
    static constexpr float kDeckMuffle = 10.0F;
    static constexpr float kBendMuffle = 1.4F;
    static constexpr float kShaftHalfM = 17.0F;
    static constexpr float kDeckDepthM = 0.6F;

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
    static constexpr float kEyesHalfM = 0.15F;   // her two lines of sight, either side of her middle
    static constexpr float kLoseTrackSeconds = 9.0F;
    static constexpr float kSearchSeconds = 8.0F;

    // Her paces in the hunt (matron_moves.cpp choose_pace). She is not one
    // speed: she creeps, stalks, strides and runs, and changes between them
    // when she likes -- from what she has of the player (sight, the noise,
    // how far, her rage) and a deterministic roll -- so where she will be is
    // never sure. Her feet take her from one pace to the next at kFootAccel
    // (speeding up) and kCutAccel (braking): never a snap. Hunt is kHuntMps,
    // her old top speed, kept.
    enum class Pace : std::uint8_t { Creep = 0, Stalk = 1, Stride = 2, Hunt = 3 };
    static constexpr float kCreepMps = 0.7F;
    static constexpr float kStalkMps = 2.0F;
    static constexpr float kStrideMps = 4.2F;
    [[nodiscard]] static constexpr float pace_mps(const Pace pace) noexcept {
        return pace == Pace::Creep ? kCreepMps : pace == Pace::Stalk ? kStalkMps
                                               : pace == Pace::Stride ? kStrideMps : kHuntMps;
    }
    // A pace is held kPaceHoldMinS..+kPaceHoldSpanS, then chosen again (sooner
    // when sight of the player comes or goes). The roll: kPaceSnapUp of the
    // time straight to Hunt, kPaceDropOff of the time (not with the player in
    // plain sight close) back to Creep, kPaceWander a tier either way.
    static constexpr float kPaceHoldMinS = 1.2F;
    static constexpr float kPaceHoldSpanS = 2.8F;
    static constexpr float kPaceSnapUp = 0.12F;
    static constexpr float kPaceDropOff = 0.10F;
    static constexpr float kPaceWander = 0.25F;
    // The first kPaceWarmS of a hunt she holds one tier lower unless the
    // player is in plain sight within kPaceCloseM: slow first, then faster.
    static constexpr float kPaceWarmS = 3.0F;
    static constexpr float kPaceCloseM = 6.0F;

    // Getting unstuck (matron_moves.cpp). Outside the Turn: a step up onto
    // what is under kStepUpM (the player's 0.35 m); a clamber onto what is up
    // to kClamberMaxM (the player mantles 1.85 m), slower forward than the
    // Turn's vault; both only with her feet pressed into it. A stall: under
    // kStallMoveM made in kStallWindowS with her way still ahead; each one in
    // a row goes a rung further: 1 a clamber or a vault, 2 a sidestep along
    // the face in her way (kSidestepM, kSidestepS), 3 a reroute round the
    // node she could not reach (blocked kRerouteBlockS), then the rungs again.
    static constexpr float kStepUpM = 0.38F;
    static constexpr float kClamberMaxM = 1.9F;
    static constexpr float kClamberForwardMps = 1.4F;
    static constexpr float kClamberCooldownS = 1.0F;
    static constexpr float kStallMoveM = 0.3F;
    static constexpr float kStallWindowS = 1.0F;
    static constexpr float kSidestepM = 1.8F;
    static constexpr float kSidestepS = 1.4F;
    static constexpr float kRerouteBlockS = 12.0F;
    // A knee-height sweep in way_clear, over anything a step-up takes.
    static constexpr float kKneeSweepM = 0.5F;

    // The player's way, remembered (matron_moves.cpp): a crumb every
    // kCrumbEveryM of their walk on her level, the last kCrumbs; searching,
    // she walks their trail on from where she meets it.
    static constexpr int kCrumbs = 24;
    static constexpr float kCrumbEveryM = 1.5F;
    static constexpr float kCrumbMeetM = 6.0F;
    // Silence for silence: the player quiet kHushAfterS while she hunts out of
    // sight, she stops dead and listens kHushMinS..+kHushSpanS (or until a
    // sound), then moves again with a creak (a counted edge), at Creep.
    static constexpr float kHushAfterS = 1.2F;
    static constexpr float kHushMinS = 2.0F;
    static constexpr float kHushSpanS = 2.5F;
    static constexpr float kQuietLoudness = 0.02F;

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
    static constexpr float kFootSpanM = 0.24F;   // floor she stands on runs this far either side of her middle
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
    // The riser from 143 to her deck (band_stack.cpp C3: its rungs on x
    // -6.00, z -133.50 from 143's walkway plate out over the shaft, its head a
    // bridge over 154's rail and steps down onto the deck's runway): where a
    // climber hangs on it, and the runway where it lets them off on her deck.
    static constexpr float kRiserX = -6.0F;
    static constexpr float kRiserFootZ = -133.85F;
    static constexpr float kRiserTopZ = -129.5F;

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

    // Her stages (the owner, 2026-10-19): a pest, dangerous, and fully
    // changed. Her rage moves her between them, both ways: only sound that
    // reaches her ear feeds it (by its level there: the sound's loudness,
    // less the way it carried over its reach), quiet lowers it, and no floor
    // rises with time, so a silent climber keeps her a pest. A stage is left
    // at a lower rage than it was entered at, so she never flickers on a
    // threshold. Nothing of it is drawn: the player reads her. The numbers
    // are CHOSEN and will be tuned once she follows up the tower: a sprint
    // beside her for half a minute makes her dangerous; a quiet minute drops
    // her a stage. A death's checkpoint keeps her rage with her state.
    enum class Stage : std::uint8_t { Pest = 0, Dangerous = 1, Changed = 2 };
    static constexpr float kRageGainPerSecond = 0.02F;    // per unit of level at her ear
    static constexpr float kRagePerSpike = 0.04F;         // per unit the level jumps by: a landing, a crash
    static constexpr float kRageDecayPerSecond = 0.003F;
    static constexpr float kRageDangerousUp = 0.45F;
    static constexpr float kRageDangerousDown = 0.30F;
    static constexpr float kRageChangedUp = 0.85F;
    static constexpr float kRageChangedDown = 0.65F;
    // What the stages change today: how soon the Turn can come again. A
    // pest hardly ever snaps; fully changed, she hardly stops.
    static constexpr float kTurnCooldownPestSeconds = 45.0F;
    static constexpr float kTurnCooldownChangedSeconds = 4.0F;
    // Her look follows the exposure, one way: how long she has been out,
    // over this, is how far the source's work shows on her.
    static constexpr float kExposureFullSeconds = 600.0F;
    // What her hands on the player are, by stage (physics_world.cpp): fully
    // changed, a death; before that a shove, kShoveMps along the floor and
    // kShoveUpMps up, once in kShoveCooldownSeconds. A pest's shove is away
    // from her; dangerous, she aims it at the shaft's rim when the player is
    // within kShoveEdgeReachM of it (the owner: she tries to nudge him off a
    // band).
    static constexpr float kShoveMps = 5.5F;
    static constexpr float kShoveUpMps = 2.0F;
    static constexpr float kShoveCooldownSeconds = 1.5F;
    static constexpr float kShoveEdgeReachM = 3.0F;
    // Her mischief as a pest (the owner, 2026-10-19: she carries things off
    // and throws them at you): light stock the player has handled is theirs
    // to her. Hunting as a pest, with the player more than kThrowMinM off,
    // she goes for such a piece within kSeekStockM of her on her floor and
    // takes it in her hands (a point at kHandsOutM ahead of her chest, drawn
    // in at kHandsPullMps). Holding it, with the player in sight and in a
    // throw's reach, she rears back for kThrowWindupSeconds and throws it at
    // them at kThrowMps (CHOSEN: a hit the body feels, short of an arm broken
    // or a death: physics_world_tuning.hpp kHitFeltSpeed, kArmBreakHitSpeed);
    // then not again for kThrowCooldownSeconds. Never getting the throw, she
    // carries it off to her post and drops it there. She lets it go when she
    // is no longer a pest, in the Turn, on a climb, a line or a moving floor,
    // or at the player. The landing of her own throw is not a sound to her
    // (physics_world.cpp): her wrecking the player's work does not feed her
    // own rage.
    static constexpr float kSeekStockM = 5.0F;
    static constexpr float kTakeM = 0.85F;
    static constexpr float kHandsUpM = 0.25F;
    // Light stock only: a toolbox (18 kg) she carries off; a drum (55 kg)
    // she leaves where the player set it.
    static constexpr float kTakeMaxKg = 25.0F;
    static constexpr float kHandsOutM = 0.55F;
    static constexpr float kHandsPullMps = 3.0F;
    static constexpr float kHandsSlipM = 1.0F;
    static constexpr float kThrowMps = 7.5F;
    static constexpr float kThrowMinM = 2.5F;
    static constexpr float kThrowMaxM = 6.0F;
    static constexpr float kThrowWindupSeconds = 0.35F;
    static constexpr float kThrowCooldownSeconds = 6.0F;
    static constexpr float kThrowQuietSeconds = 2.5F;
    // A piece she gets no nearer to (it lies where her feet cannot take her:
    // past a rail, in a cage's back) she gives up on, and leaves a while.
    static constexpr float kFetchStallSeconds = 1.5F;
    static constexpr float kFetchRetrySeconds = 12.0F;
    // Her balance (the owner, 2026-10-19: knock her down, never dead). A blow
    // is the change in her speed across a step that her own feet did not
    // make, counted only when something moving was closing on her before it
    // (a thrown or dropped piece, a rolled drum, a machine): never her own
    // steps into a rail, a floor or a crate at rest, nor the player's body.
    // Against it a declared reduced-order balance, w (b + r) with w =
    // sqrt(g / h): h her stance's height, b her base, r the step her stage
    // can still take. Under w b she takes it; under w (b + r) she staggers;
    // past it she goes down. Stance: upright, or on all fours hunting at a
    // run once dangerous (a pest hurries after the player upright, an old
    // woman's hobble: so a heave can put a pest down) and in the Turn's
    // lunges. CHOSEN so a toolbox heaved from the hands
    // fells an upright pest, a drum only staggers a dangerous Matron, and
    // fully changed only real mass fells her (measured: a toolbox heaved at
    // 5.8 m/s from 2 m gives her about 0.37 m/s, a share of its momentum). No step in the Turn's wind-up
    // (r 0, the one warning is the one opening); twice the step in a lunge's
    // burst, braced against a blow she faces, and just after rising; half
    // with stock in her hands; none against a blow at her shins (low, under
    // kLowBlowM) or a weight coming down on her.
    static constexpr float kUprightHeightM = 1.05F;
    static constexpr float kUprightBaseM = 0.08F;     // frail on her feet: an old woman's narrow, shuffling base
    static constexpr float kCrouchHeightM = 0.55F;
    static constexpr float kCrouchBaseM = 0.45F;
    static constexpr float kPestStepM = 0.02F;
    static constexpr float kDangerousStepM = 0.30F;
    static constexpr float kChangedStepM = 0.75F;
    static constexpr float kLowBlowM = 0.5F;          // a striker's middle under her knees
    static constexpr float kAboveMassKg = 40.0F;      // a weight coming down on her:
    static constexpr float kAboveMps = 4.0F;          //   this heavy, this fast
    static constexpr float kBraceCos = 0.643F;        // facing it within 50 degrees
    static constexpr float kStrikerClosingMps = 0.5F; // the striker's own speed at her
    // A blow is read over the whole contact: Jolt spreads a hit over a few
    // steps, and her feet take back each step's share before the next. The
    // ticks a striker is against her add up (her change, and the impulse the
    // striker gave up, which is hers), till kBlowQuietTicks without one.
    static constexpr int kBlowQuietTicks = 6;
    // Staggering, her feet catch her (stopping her) at her stage's recovery,
    // for a time by how far past w b the blow went. Down, she lies for her
    // stage's time on the steel (a lying capsule, or kneeling where that
    // does not fit), sliding on it, and stands where she has room (else
    // crawls out to room first); then her step is doubled a while.
    static constexpr float kStaggerMinSeconds = 0.4F;
    static constexpr float kStaggerMaxSeconds = 1.2F;
    static constexpr float kPestRecoverAccel = 4.0F;
    static constexpr float kDangerousRecoverAccel = 9.0F;
    static constexpr float kChangedRecoverAccel = 16.0F;
    static constexpr float kPestDownSeconds = 4.0F;
    static constexpr float kDangerousDownSeconds = 2.5F;
    static constexpr float kChangedDownSeconds = 1.2F;
    static constexpr float kDownFriction = 0.6F;
    static constexpr float kSetSeconds = 1.5F;
    static constexpr float kCrawlMps = 0.8F;
    // Pain is borrowed rage: a knockdown adds this, a stagger this.
    static constexpr float kKnockdownRage = 0.08F;
    static constexpr float kStaggerRage = 0.02F;
    // The shaft's corner pipe (the owner, 2026-10-19: "She comes down the
    // shaft after you. One shaft pipe becomes hers to climb."): the north-east
    // riser (world_solids.inc Riser: 0.7 m square at x 15.8, z -134.2, a flange
    // 1.4 m under every deck), too thick for the climber's hands. Her line
    // hugs its north-west corner from the open shaft, west of the flanges
    // (theirs reach x 15.25): a drum thrown down from the north band west of
    // them falls past them onto her, with room for the way a throw carries it
    // on (in the corner by the shaft's east edge the flange under the deck
    // takes any drum: no room past it). She gets on and off over the north
    // edge from the band, kPipeOverRailM over the deck, over the rail. On
    // it she is on the steel (object_layers::kOnSteel): what moves meets her,
    // the steel she wraps round does not. Frail, she cannot take hold of it
    // (as her ladder); dangerous she slides down it held by her forearms and
    // knees and climbs it hand over hand; fully changed she comes down it head
    // first in jerks. Each kPipeHoldM her hands go is a knock on the pipe, each
    // flange passed a bong: she is heard on it from the moment she takes hold
    // (an 11 m storey takes her at least 4 s). A blow on it past kPipeTearMps
    // opens her hands: her legs slide her down to the flange under her
    // (kPipeSlideAccel, her squeeze against her weight) and she slams onto it,
    // stunned a while by how hard, alive; then she goes on.
    static constexpr float kPipeLineX = 15.15F;
    static constexpr float kPipeLineZ = -133.75F;
    static constexpr float kPipeDeckX = 15.95F;
    static constexpr float kPipeDeckZ = -132.25F;
    static constexpr float kPipeOverRailM = 1.35F;
    static constexpr float kPipeLowestFloor = 132.0F;   // the stack's deck 12
    static constexpr float kPipeMountMps = 1.4F;
    static constexpr float kPipeDownMps = 2.6F;
    static constexpr float kPipeUpMps = 0.7F;
    static constexpr float kPipeChangedDownBurstMps = 4.2F;
    static constexpr float kPipeChangedUpBurstMps = 2.6F;
    static constexpr float kPipeChangedPauseMps = 0.3F;
    static constexpr float kPipeHoldM = 0.45F;
    static constexpr float kPipeFlangeUnderDeckM = 1.225F;   // a flange's top, under its deck
    static constexpr float kPipeTearMps = 0.8F;   // CHOSEN: a drum let go onto her from the rail above (0.945 m/s measured with gravity on her and her grip holding against it) tears her off; a thrown toolbox (0.3) does not
    static constexpr float kPipeSlideAccel = 4.4F;
    static constexpr float kPipeStunSeconds = 0.5F;
    static constexpr float kPipeStunPerMps = 0.25F;   // and this per m/s she slams past kPipeStunFromMps
    static constexpr float kPipeStunFromMps = 5.0F;
    // A lead a storey away by the pipe is worth chasing longer than one on her
    // own deck: a dangerous Matron keeps after it this long since she last
    // heard the climber, fully changed this long.
    static constexpr float kPipeChaseDangerousSeconds = 40.0F;
    static constexpr float kPipeChaseChangedSeconds = 150.0F;
    // Off the pipe onto a deck she stands this long, listening, before she
    // goes on (CHOSEN: the climber who heard her coming gets a moment: up
    // and away, or to the drum over her pipe). Listening is for where the
    // climber went: stock still, her head over on her shoulder, a sound
    // carries kListenReach times as far to her as on the move (CHOSEN: a
    // climber on the riser's rungs, 14 m on the move, is heard from her pipe
    // across 143's north edge, 23 m off).
    static constexpr float kPipeListenSeconds = 2.5F;
    static constexpr float kListenReach = 2.0F;
    // The moving bodies near her a blow can come from: within this, at most
    // kMaxNear of the nearest.
    static constexpr float kNearM = 3.0F;
    static constexpr std::size_t kMaxNear = 8;

    // Her hold (matron_hold.cpp; the physics of it in physics_world_grab.cpp).
    // Her hand is a body of its own (kHandMassKg) on her arm (a soft limit of
    // kArmReachM from her shoulder, kStoopReachM while she stoops for an
    // ankle), driven at its pose by her muscle: a spring-damper capped at
    // kHandPullN (kHandReachPullN reaching), its reaction on her body. The
    // grip itself is a SwingTwist hand-to-ankle; its load over
    // kGripMaxForceN, or its strain (load past kGripStrainFloorN, over time,
    // less kGripStrainDecay a second) past kGripTearNs, tears it out of her
    // hand. A calm hang is 0.83 kN; only the player's thrashing gets there.
    static constexpr float kShoulderUpM = 1.80F;      // over her feet
    static constexpr float kArmReachM = 1.4F;
    static constexpr float kStoopReachM = 2.1F;
    static constexpr float kArmRetractMps = 0.9F;
    static constexpr float kHandMassKg = 6.0F;
    static constexpr float kHandSpeedMps = 1.8F;      // her hand's pose moves no faster
    static constexpr float kHandReachSpeedMps = 4.0F;
    static constexpr float kHandPullN = 2600.0F;
    static constexpr float kHandReachPullN = 900.0F;
    static constexpr float kRipRampNps = 2000.0F;     // against a braced player, her pull grows this fast
    static constexpr float kRipMaxN = 3400.0F;
    static constexpr float kGripMaxForceN = 3800.0F;
    static constexpr float kGripStrainFloorN = 2000.0F;
    static constexpr float kGripTearNs = 220.0F;
    static constexpr float kGripStrainDecay = 60.0F;
    static constexpr float kGripCloseM = 0.14F;       // her fingers close on an ankle this near
    static constexpr float kGrabReachSeconds = 1.2F;  // and miss it after this
    static constexpr float kGrabLiftSeconds = 0.9F;
    static constexpr float kGrabCooldownSeconds = 1.5F;   // after a miss
    static constexpr float kAfterReleaseSeconds = 6.0F;   // no new grab for this long after a hold ends
    // Carry: at kCarryPaceScale of her hunt, the body upside down at her
    // side, her hand over her head (kCarryHandUpM over her feet), at most
    // kCarryMaxSeconds to the rail; past it she drops them where she is.
    static constexpr float kCarryPaceScale = 0.5F;
    static constexpr float kCarryHandUpM = 2.8F;
    static constexpr float kCarrySideM = 0.7F;
    static constexpr float kCarryMaxSeconds = 10.0F;
    static constexpr float kRimStandM = 0.42F;        // her middle this far in from the shaft's rail
    static constexpr float kRimArriveM = 0.30F;
    static constexpr float kRimBraceM = 0.9F;        // braced: her middle within this of the rail
    static constexpr float kRimSearchM = 16.0F;       // rail spots she weighs for a ledge below
    // Dangle: the heave out over the rail (the hand up at kCarryHandUpM and
    // kHeaveOutM out from her middle), then down to kDangleHandUpM over her
    // feet, kDangleOutM out: the body hangs below the deck's edge, head down.
    static constexpr float kHeaveOutM = 0.55F;
    static constexpr float kDangleOutM = 1.35F;
    static constexpr float kDangleHandUpM = 1.35F;
    static constexpr float kDangleMinSeconds = 3.0F;
    static constexpr float kDangleSpanSeconds = 3.0F;
    // Then: one in kSpareOdds she sets them back (Spare); else she lets go
    // (Release) aimed at the spot she chose (a ledge whose fall is under
    // kReleaseSafeMps: a limp at worst), the push at most kReleasePushMps;
    // no such ledge, Spare.
    static constexpr float kSpareOdds = 0.25F;
    static constexpr float kReleaseSafeMps = 14.4F;   // kAnkleBreakSpeed * 1.2
    static constexpr float kReleasePushMps = 4.0F;
    static constexpr float kReleasePoseSeconds = 1.0F;
    static constexpr float kSpareSetSeconds = 1.2F;   // the collar put straight
    static constexpr float kSpareCalmSeconds = 20.0F; // then she walks off, no hunt this long

    // ArmPlay (matron_hold.cpp; the physics in physics_world_arm.cpp): fully
    // changed, after a dangle (one in kArmPlayOdds, or the player thrashed
    // and stayed in her hand), her other hand takes a wrist and pulls it away
    // from the body, her pull rising from kArmPullStartN at kRipRampNps to
    // kArmPullMaxN (the ankle hand then holds the pull and the weight: under
    // its cap; she grips it hard, the strain counted only past
    // kGripRipFloorN). The shoulder tears (physics) at kArmTearNs of the
    // wrist's load past kArmHoldN; the arm (kArmMassKg) is hers: lifted to
    // her face, swung, thrown (kArmThrowMps) onto the deck behind her. The
    // whole of it kArmPlaySeconds from the tear; no tear in kArmPullGiveUpS
    // and she gives it up.
    static constexpr float kArmPlayOdds = 1.0F / 3.0F;
    static constexpr float kArmPullStartN = 1500.0F;
    static constexpr float kArmPullMaxN = 2600.0F;
    static constexpr float kGripRipFloorN = 3500.0F;
    static constexpr float kArmHoldN = 1500.0F;
    static constexpr float kArmTearNs = 600.0F;
    static constexpr float kArmMassKg = 4.5F;
    static constexpr float kArmThrowMps = 7.5F;
    static constexpr float kArmPlaySeconds = 4.5F;
    static constexpr float kArmReachSeconds = 3.5F;
    static constexpr float kArmPullGiveUpS = 4.0F;
    static constexpr float kArmPullMoveMps = 0.5F;
    struct Throwable {
        JPH::BodyID body;
        std::uint64_t entity = 0;
        JPH::Vec3 handle = JPH::Vec3::sZero();   // where a hand takes it, in its own frame
        float mass_kg = 0.0F;   // read from its body by set_throwables (infinite: not a moving body)
    };

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
        bool bracing_structure = false;   // her hands holding the bay's leaf
        float support_n = 0.0F;           // and what they hold up, N
        std::uint8_t trap = 0;            // her trap at the bay: 0 none, 1 to its edge, 2 down, 3 to her spot,
                                          // 4 holding, 5 climbing out
        std::uint32_t traps = 0;          // times she set it, this run
        bool clinging = false;      // holding on to a floor that moves (or just stopped)
        float floor_y = kDeckTop;   // the level she hunts: its floor's top
        float lead_floor_y = kDeckTop;   // the level of her lead (where she heard or saw the player)
        float rage = 0.0F;          // 0..1: fed by sound at her ear, lowered by quiet
        Stage stage = Stage::Pest;
        float exposure = 0.0F;      // 0..1: how far the source's work shows, by her time out
        std::uint32_t shoves = 0;   // the player thrown off their feet by her hands, this run
        std::uint64_t carrying = 0; // the entity of the stock in her hands, 0 for none
        std::uint32_t takes = 0;    // pieces of the player's stock she took, this run
        std::uint32_t throws = 0;   // and threw at the player, this run
        std::uint32_t staggers = 0;     // blows that took her balance, this run
        std::uint32_t knockdowns = 0;   // and that took her off her feet
        float blow_mps = 0.0F;          // the last blow: the change in her speed it made
        float fall_x = 0.0F, fall_z = 1.0F;   // down: the way she fell (her head that way)
        bool kneeling = false;          // down where lying did not fit: crumpled on her knees
        float down_s = 0.0F;            // seconds on the steel
        float down_left_s = 0.0F;       // of the time she lies there; 0 and still down: pinned, crawling to room
        bool on_pipe = false;           // on the shaft's corner pipe (getting on, on it, getting off)
        bool pipe_head_down = false;    // coming down it head first (fully changed)
        bool pipe_sliding = false;      // torn off it, sliding on her legs to the flange under her
        bool pipe_stunned = false;      // slammed onto a flange
        bool pipe_listening = false;    // just off it, still, listening
        std::uint32_t pipe_links = 0;   // storeys she set out along it, this run
        std::uint32_t pipe_holds = 0;   // her hands' holds on it (each a knock)
        std::uint32_t pipe_flanges = 0; // flanges she passed (each a bong)
        std::uint32_t pipe_slams = 0;   // flanges slammed onto, torn off
        float pipe_slam_mps = 0.0F;     // the last slam's speed
        float pipe_peak_force_n = 0.0F; // her grip's peak force on the pipe (budget-capped)
        float pipe_peak_power_w = 0.0F;
        // Her pace (Pace) and the speed it asks of her feet; getting unstuck
        // (stalls met, step-ups, clambers, sidesteps, reroutes, the longest
        // time without kStallMoveM of progress); the player's trail (crumbs
        // kept, crumbs walked); silence for silence (listening now, creaks).
        std::uint8_t pace = 0;
        float pace_mps = 0.0F;
        std::uint32_t pace_changes = 0;
        std::uint32_t stalls = 0;
        std::uint32_t step_ups = 0;
        std::uint32_t clambers = 0;
        std::uint32_t sidesteps = 0;
        std::uint32_t reroutes = 0;
        float longest_stall_s = 0.0F;
        std::uint32_t crumbs = 0;
        std::uint32_t crumbs_walked = 0;
        bool hushed = false;
        std::uint32_t creaks = 0;
        // The hold: its edges counted (the voice's hooks), the hand's pose.
        std::uint32_t grabs = 0;        // ankles her fingers closed on
        std::uint32_t grab_misses = 0;  // reaches that came away empty
        std::uint32_t carries = 0;
        std::uint32_t dangles = 0;
        std::uint32_t armplays = 0;
        std::uint32_t releases = 0;
        std::uint32_t spares = 0;
        std::uint32_t grips_torn = 0;   // torn out of her hand by the player
        std::uint32_t brace_losses = 0; // stepped back from the rail, unbraced
        std::uint32_t carry_drops = 0;  // carried too long: dropped on the deck
        std::uint32_t arm_tears = 0;    // arms she tore off
        std::uint32_t arm_throws = 0;   // and threw
        std::uint32_t claims = 0;       // both arms gone: she won
        std::uint8_t fatal_beat = 0;    // Fatal, while Claimed
        std::uint32_t waste_not = 0;    // the 'Waste not.' voice hook, an edge
        std::uint8_t armplay_beat = 0;  // 0 reach, 1 pull, 2 to her face, 3 swing, 4 throw
        float hand2_x = 0.0F, hand2_y = 0.0F, hand2_z = 0.0F;
        bool hold_braced = false;
        bool grip_on = false;
        float hold_s = 0.0F;            // seconds in this hold state
        float hand_x = 0.0F, hand_y = 0.0F, hand_z = 0.0F;   // her hand's pose
        bool aim_ledge = false;         // a ledge below her rail spot to let go onto
        float aim_x = 0.0F, aim_y = 0.0F, aim_z = 0.0F;
    };

    // What the checkpoint keeps of her (her state, not her body: a restore
    // puts her back at her locker at that stage). Once dead she stays dead.
    struct Saved {
        State state = State::Dormant;
        float strain_s = 0.0F;
        float rage = 0.0F;
        Stage stage = Stage::Pest;
        float exposure_s = 0.0F;
        std::uint32_t handled = 0;   // the throwables the player has handled, a bit each
    };

    Matron(JPH::PhysicsSystem &physics, std::uint32_t static_layer, std::uint32_t moving_layer,
           std::uint32_t steel_layer, JPH::BodyID player);
    ~Matron();
    Matron(const Matron &) = delete;
    Matron &operator=(const Matron &) = delete;

    // `loudness`: 0..1 from the player's movement this tick (see the .cpp);
    // `player` is the body's centre.
    void pre_step(float dt, JPH::RVec3 player, float loudness);
    // The bay's district (bay.hpp), its lifetime the world's: detach before
    // destroying it. Her hands' support goes through it (her trap).
    void set_shared_survival(SharedSurvival *encounter) noexcept;
    [[nodiscard]] bool bracing_structure() const noexcept;
    // The bay's leaf: a floor (level on its stop, driven), and whether the
    // chain holds it. Given before pre_step.
    void set_bay(const JPH::BodyID leaf, const bool leaf_open, const bool hooked) noexcept {
        bay_leaf_ = leaf;
        bay_open_ = leaf_open;
        bay_hooked_ = hooked;
    }
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
    // The stock she may take and throw (at most 32 pieces): given once it is laid.
    void set_throwables(std::vector<Throwable> throwables);
    // The entity in the player's hands this tick, 0 for none: given before
    // pre_step. A throwable they hold becomes theirs to her, and she never
    // takes it out of their hands.
    void set_player_holding(std::uint64_t entity) noexcept { player_holding_ = entity; }
    // Whatever is in her hands let go where it is, and no piece gone for: a
    // checkpoint's restore does this before any body is put back.
    void drop_stock();
    // The stock in her hands (an invalid id for none): not the player's to take.
    [[nodiscard]] JPH::BodyID held_body() const noexcept {
        return held_ >= 0 ? throwables_[static_cast<std::size_t>(held_)].body : JPH::BodyID();
    }
    // Her last throw's entity while it is still in the air and landing, else 0.
    [[nodiscard]] std::uint64_t thrown_entity() const noexcept {
        return thrown_ >= 0 && thrown_s_ > 0.0F ? throwables_[static_cast<std::size_t>(thrown_)].entity : 0;
    }
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
    // alive and the player is on her deck, up a climb from it, or on a deck
    // her corner pipe is taking her to or (no longer frail) joins to hers. A
    // player who has left the deck (up a machine) is out of her reach and
    // checkpoints as ever.
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
    // Her hands on the player, by stage: a death only fully changed; else a
    // shove, once in its cooldown, along shove_direction.
    [[nodiscard]] bool kills() const noexcept { return stage_ == Stage::Changed; }
    [[nodiscard]] bool shove_ready() const noexcept { return shove_cooldown_s_ <= 0.0F; }
    void count_shove() noexcept {
        ++shoves_;
        shove_cooldown_s_ = kShoveCooldownSeconds;
    }
    [[nodiscard]] JPH::Vec3 shove_direction(JPH::RVec3 player) const noexcept;
    // Tests only (never bound to the bridge): her stage, and the rage it takes.
    // Tests only: her pace held at `pace` (0..3) with no hush, or (-1) free:
    // the tests that meet the Turn and the vault at one speed, as written.
    void fix_pace_for_test(const int pace) noexcept { fixed_pace_ = pace; }
    // Tests only: out of her locker (stepped out of its front, as at a
    // burst) and on her patrol round her level's loop.
    void patrol_for_test();

    // Her climbing over the tower's real steel (matron_traversal.hpp): the
    // map of what it offers her (scanned from its collision geometry), and a
    // journey to a region of it by her own moves. No route is authored: she
    // plans over the map, makes each move with her capped forces, commits a
    // region only standing on it, and on a failed move falls and plans again
    // from wherever she lands.
    // The tower as she has looked it over (traversal::AffordanceMap): her
    // climbs are the ladders it found, none written in.
    void set_routes(const traversal::AffordanceMap *routes);
    // Out of her locker with the tower not yet looked over.
    [[nodiscard]] bool wants_routes() const noexcept { return routes_ == nullptr && body_in_world_; }
    bool travel_to(int region);   // false: no way there from where she stands
    [[nodiscard]] int moving_layer() const noexcept { return static_cast<int>(moving_layer_); }
    struct Journey {
        bool active = false;
        int goal = -1;
        int region = -1;        // the region she last stood in (committed on support)
        int step = 0, steps = 0;
        std::uint32_t moves_done = 0, moves_failed = 0, replans = 0, arrivals = 0;
        traversal::MoveKind move = traversal::MoveKind::Walk;
        traversal::Failure last_failure = traversal::Failure::None;
    };
    [[nodiscard]] const Journey &journey() const noexcept { return journey_; }
    [[nodiscard]] const traversal::Climber *climber() const noexcept { return climber_.get(); }

    // The hold's interface to the physics (physics_world_grab.cpp).
    [[nodiscard]] bool grab_ready() const noexcept { return grab_cooldown_s_ <= 0.0F && kills(); }
    void begin_grab(JPH::RVec3 player);
    [[nodiscard]] bool holding() const noexcept { return holding_state(state_) && body_in_world_ && !dead_; }
    [[nodiscard]] bool reaching() const noexcept { return state_ == State::Grab && !grip_on_; }
    [[nodiscard]] JPH::RVec3 hand_target() const noexcept { return hand_target_; }
    [[nodiscard]] JPH::Vec3 hand_target_velocity() const noexcept { return hand_target_v_; }
    [[nodiscard]] JPH::Vec3 shoulder_local() const noexcept;
    [[nodiscard]] float arm_reach_m() const noexcept { return arm_reach_m_; }
    [[nodiscard]] float hand_pull_n() const noexcept { return pull_n_; }
    void set_player_braced(const bool braced) noexcept { player_braced_ = braced; }
    void set_thrash(const float thrash) noexcept { thrash_ = thrash; }
    void grip_taken() noexcept;
    void grip_lost(bool torn) noexcept;
    // She lets go this tick: the push to give them (world), once.
    [[nodiscard]] bool take_let_go(JPH::Vec3 &push) noexcept {
        if (!let_go_) {
            return false;
        }
        let_go_ = false;
        push = let_go_push_;
        return true;
    }

    // Her second hand (ArmPlay).
    [[nodiscard]] bool hand2_active() const noexcept {
        return (state_ == State::ArmPlay && hold_phase_ < 5) || (claimed() && fatal_beat_ == 5);
    }
    [[nodiscard]] bool reaching_wrist() const noexcept { return state_ == State::ArmPlay && hold_phase_ == 0; }
    [[nodiscard]] bool pulling_wrist() const noexcept { return state_ == State::ArmPlay && hold_phase_ == 1; }
    [[nodiscard]] JPH::RVec3 hand2_target() const noexcept { return hand2_target_; }
    [[nodiscard]] JPH::Vec3 hand2_target_velocity() const noexcept { return hand2_target_v_; }
    [[nodiscard]] float hand2_pull_n() const noexcept { return pull2_n_; }
    [[nodiscard]] float grip_strain_floor_n() const noexcept {
        return state_ == State::ArmPlay ? kGripRipFloorN : kGripStrainFloorN;
    }
    void set_player_body(const JPH::RVec3 wrist, const int arms_left) noexcept {
        player_wrist_ = wrist;
        player_arms_left_ = arms_left;
    }
    void wrist_taken() noexcept;
    void arm_torn() noexcept;
    [[nodiscard]] bool take_arm_throw(JPH::Vec3 &velocity) noexcept {
        if (!arm_throw_) {
            return false;
        }
        arm_throw_ = false;
        velocity = arm_throw_v_;
        return true;
    }
    void set_armplay_for_test(const int armplay) noexcept { armplay_for_test_ = armplay; }
    [[nodiscard]] bool claimed() const noexcept { return state_ == State::Claimed; }

    // ---- Claimed: the fatality (matron_fatality.cpp) -----------------------
    // Her beats, in order. Each is a physical act on kit and body: nothing in
    // it moves the player but her hands, her knee, the rope and gravity.
    enum class Fatal : std::uint8_t {
        Lay = 0,    // the armless body to the pin spot by its ankle, let down on the steel
        Pin = 1,    // her knee down on the torso
        Leg0 = 2,   // her fist on the left ankle, the leg drawn out from the hip
        Leg1 = 3,   // the right
        Drag = 4,   // the torso by its collar to the skip lift's bollard, onto the rope's shackle
        Kick = 5,   // the catch's trip handle wrenched down under her boot: the skip goes
        Haul = 6,   // 800 kg of skip down the well; up the rope goes the torso
        Fall = 7,   // the knot parts at the head beam or the rope's next snap; the torso comes back down
        Wipe = 8,   // her hands on her apron ("Waste not.")
        Black = 9,  // to black: the run is over
    };
    // Her knee and its weight, her leg pull, and the knot's strength (CHOSEN).
    static constexpr float kKneePinN = 3500.0F;    // her bulk on one knee, braced: the cap on the knee's push
    static constexpr float kLegPullN = 3200.0F;    // both arms on an ankle, her back in it
    static constexpr float kLegHoldN = 2200.0F;    // a hip takes this much for as long as you like
    static constexpr float kLegTearNs = 1200.0F;   // the hip's strain past kLegHoldN that tears the leg
    static constexpr float kLegMassKg = 12.0F;
    static constexpr float kLegTossMps = 3.0F;
    static constexpr float kKnotTearN = 5000.0F;   // her knot of the torso to the shackle
    static constexpr float kWipeSeconds = 2.6F;
    static constexpr float kBlackSeconds = 1.5F;
    static constexpr float kFatalGiveUpSeconds = 20.0F;   // a beat that the world will not let finish
    // Where she does it (the Counterweight Well, stage A, on the 154 deck).
    static constexpr float kPinSpotX = -6.4F, kPinSpotZ = -129.0F;
    static constexpr float kBollardStandX = -13.35F, kBollardStandZ = -130.9F;
    static constexpr float kTripStandX = -11.0F, kTripStandZ = -128.35F;

    [[nodiscard]] Fatal fatal_beat() const noexcept { return static_cast<Fatal>(fatal_beat_); }
    struct FatalPoints final {
        JPH::RVec3 torso = JPH::RVec3::sZero();
        JPH::RVec3 collar = JPH::RVec3::sZero();
        JPH::RVec3 ankle[2] = {JPH::RVec3::sZero(), JPH::RVec3::sZero()};
        JPH::RVec3 shackle = JPH::RVec3::sZero();
        JPH::RVec3 handle = JPH::RVec3::sZero();
        bool torso_grounded = false;
    };
    void set_fatal_points(const FatalPoints &points) noexcept { fatal_points_ = points; }
    // Physics tells her what her hands have done.
    void fatal_pinned() noexcept;
    void fatal_leg_taken() noexcept;
    void fatal_leg_torn() noexcept;
    void fatal_collar_taken() noexcept;
    void fatal_hooked() noexcept;
    void fatal_handle_taken() noexcept;
    void fatal_tripped() noexcept;
    void fatal_dropped() noexcept;
    void fatal_landed() noexcept;
    // And she tells physics what she is doing with them.
    [[nodiscard]] bool knee_down() const noexcept {
        return claimed() && fatal_beat_ >= 1 && fatal_beat_ <= 3;
    }
    [[nodiscard]] JPH::RVec3 knee_target() const noexcept { return knee_target_; }
    [[nodiscard]] int leg_side() const noexcept {
        return claimed() && (fatal_beat_ == 2 || fatal_beat_ == 3) ? fatal_beat_ - 2 : -1;
    }
    [[nodiscard]] bool reaching_leg() const noexcept { return leg_side() >= 0 && fatal_sub_ == 0; }
    [[nodiscard]] bool reaching_collar() const noexcept { return claimed() && fatal_beat_ == 4 && fatal_sub_ == 0; }
    [[nodiscard]] bool hooking() const noexcept { return claimed() && fatal_beat_ == 4 && fatal_sub_ == 1; }
    [[nodiscard]] bool reaching_handle() const noexcept { return claimed() && fatal_beat_ == 5 && fatal_sub_ == 0; }
    [[nodiscard]] bool wrenching_handle() const noexcept { return claimed() && fatal_beat_ == 5 && fatal_sub_ == 1; }
    [[nodiscard]] bool take_leg_toss(JPH::Vec3 &velocity) noexcept {
        if (!leg_toss_) {
            return false;
        }
        leg_toss_ = false;
        velocity = leg_toss_v_;
        return true;
    }
    [[nodiscard]] bool fatality_over() const noexcept {
        return claimed() && fatal_beat_ == 9 && hold_s_ >= kBlackSeconds;
    }
    [[nodiscard]] float wipe_phase() const noexcept { return claimed() && fatal_beat_ == 8 ? hold_s_ : 0.0F; }
    [[nodiscard]] std::uint32_t waste_not_lines() const noexcept { return waste_not_; }
    void set_spare_for_test(const int spare) noexcept { spare_for_test_ = spare; }
    void set_dangle_seconds_for_test(const float seconds) noexcept { dangle_for_test_s_ = seconds; }
    void set_stage_for_test(Stage stage) noexcept;
    // Tests only: her hands held off the player for `seconds` (no shove), so
    // a test can meet her body alone.
    void hold_hands_for_test(const float seconds) noexcept { shove_cooldown_s_ = seconds; }
    [[nodiscard]] Saved save() const noexcept {
        // A hold is never saved: a checkpoint has her after the player.
        return {state_ >= State::Grab ? State::Hunting : state_, strain_s_, rage_, stage_, exposure_s_, handled_};
    }
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
        Leaf,   // the bay's leaf: a floor while it is one (set_bay)
    };
    // Her trap at the bay, phase by phase (see the head of this file).
    enum class Trap : std::uint8_t {
        Off,
        ToEdge,
        Down,
        ToSpot,
        Holding,
        ClimbOut,
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
    // line, on the climber's side), the floor it stands on and the one it
    // tops out onto, the way off its head, where it lets a climber off on the
    // floor above (her guess where they are going), and whether she takes
    // hold of it herself. Her ladder she does (its foot a node of her deck's
    // lines); the riser from 143 to her deck she does not (its foot is on
    // 143's walkway plate out over the shaft, through the gate in the rail).
    struct Climb {
        float x, z;
        float foot_y, head_y;
        float out_x, out_z;
        float top_x, top_z;
        bool hers;
        int edge;   // the scanned edge (AffordanceMap::edges) she climbs it by
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
    // (x, z) put on the nearest of her level's floor lines (not the
    // catwalk's, not a gap's): somewhere on her level she can stand.
    void onto_floor_lines(float &x, float &z) const;
    [[nodiscard]] static bool on_catwalk_line(float x, float z, float margin);
    // The next point to walk to, along the cheapest path on her level to
    // (tx, tz), with the catwalk allowed or not and gaps (leapt) or not;
    // false when no such path reaches it (she then heads straight for it).
    bool route_toward(float fx, float fz, float tx, float tz, bool allow_catwalk, float &wx, float &wz,
                      bool allow_gaps = false) const;
    // Nothing built standing between (fx, fz) and (tx, tz) at her hips, across
    // her width, on her floor (a post, a frame): she could walk it straight.
    // Her width is swept, not sampled: a post thinner than the space between
    // two lines (the trip gantry's 0.1 m legs by her post) still stops her.
    [[nodiscard]] bool way_clear(float fx, float fz, float tx, float tz) const;
    // Her feet toward (wx, wz) at `speed`; `turn_moves` lets her vault and leap.
    void walk_toward(float dt, float wx, float wz, float speed, float foot_accel = kFootAccel,
                     bool turn_moves = false);
    // matron_moves.cpp: her pace chosen for this tick of the hunt; a crumb of
    // the player's way; the next crumb on from where she is; the stall
    // watchdog's tick (true: a sidestep target set); silence for silence.
    Pace choose_pace(float dt, float distance, bool saw, bool heard, float loudness);
    void drop_crumb(float px, float pz, float floor_y);
    bool crumb_ahead(float ax, float az, float &wx, float &wz);
    void watch_stall(float dt, JPH::RVec3 at, float heading_x, float heading_z, bool wants);
    bool hush_step(float dt, bool saw, float distance, float loudness);
    [[nodiscard]] float roll() noexcept;
    [[nodiscard]] bool node_blocked(float x, float z) const noexcept;
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
    // After a lead on a level above, up a climb of hers toward it (to it, or
    // to a level between): to its foot, and in the Turn onto it. False when no
    // climb does.
    bool follow_climb(float dt, JPH::RVec3 at);
    // On a climb: up it, and over its head.
    void climb(float dt);
    void begin_climb(int which);
    void step_journey(float dt);
    [[nodiscard]] int region_under() const;
    // After a lead a storey away along the corner pipe (above): to the rail's
    // corner on her level and onto it. False when it does not go that way, or
    // she is frail.
    bool follow_pipe(float dt, JPH::RVec3 at);
    [[nodiscard]] static bool pipe_reaches(float floor_y) noexcept;
    void get_on_pipe(float from_floor, float to_floor);
    void ride_pipe(float dt);
    // Her grip's force toward a velocity relative to the pipe, budget-capped, gravity on.
    void pipe_drive(JPH::Vec3 want, float dt);
    void get_off_pipe();
    // A blow on the pipe past kPipeTearMps: her hands open, her legs slide.
    void tear_off_pipe();
    // Down on the steel from a blow (take_blow), the way (fall_x_, fall_z_).
    void knock_down();
    // A lead: a place she heard (or saw) the player, or a crash, on the level
    // whose floor is `floor`, at that loudness.
    void take_lead(float x, float z, float floor, float loudness, bool seen);
    // Back to her post if there is a way out to guard, else the patrol.
    void stand_down();
    void step_hold(float dt, JPH::RVec3 player);
    void end_hold(State next);
    void finish_dangle(JPH::RVec3 player);
    void step_armplay(float dt, JPH::RVec3 player, JPH::RVec3 at, float feet);
    void step_claimed(float dt, JPH::RVec3 at, float feet);
    void fatal_next(int beat) noexcept;
    void fatal_work_reset() noexcept;
    bool choose_rim(float ax, float az);
    [[nodiscard]] bool braced_at_rim(float ax, float az) const noexcept;
    void pose_hand(float dt, JPH::RVec3 target, float speed);
    void pose_hand2(float dt, JPH::RVec3 target, float speed);
    // The Turn's clock: `distance_to_player` across, `carried_m` how far the
    // player's noise carried to her.
    void step_turn(float dt, float distance_to_player, float loudness, float carried_m);
    // Her rage and stage, from the level at her ear this tick; her exposure.
    void step_rage(float dt);
    [[nodiscard]] float turn_cooldown_seconds() const noexcept;
    // Her mischief (above): the piece she would go for, or -1; going for it
    // (true while she walks to it or takes it); holding and throwing it.
    [[nodiscard]] int stock_to_fetch(float ax, float az) const;
    bool fetch_stock(float dt, float ax, float az, bool allow_catwalk);
    bool throw_stock(float dt, JPH::RVec3 at, JPH::RVec3 player);
    void take_stock(int index);
    void hold_stock(float dt);
    void release_stock(JPH::Vec3 velocity, JPH::Vec3 spin, bool thrown);
    [[nodiscard]] JPH::Vec3 hands_offset() const noexcept;
    void step_planks(float dt);
    void restore_planks();
    void refresh_view();
    // Her balance (above): the moving bodies near her and the velocity her
    // feet set, taken after she moves (sense_near); the blow they deal her,
    // read after the step (take_blow); and her staggering and lying there
    // (step_knocked), down onto the steel and up off it.
    void sense_near();
    void take_blow(float dt);
    void step_knocked(float dt, JPH::RVec3 player);
    void lie_down(float fall_x, float fall_z);
    void stand_up();
    [[nodiscard]] bool room_to_stand(JPH::RVec3 at) const;
    [[nodiscard]] bool shape_fits(const JPH::Shape *shape, JPH::RVec3 at) const;
    // Off an edge she will not go, staggering or sliding on the steel: her
    // feet and hands take her at it (no floor within a step's drop ahead).
    void brace_at_edges(float dt);
    [[nodiscard]] float step_reach() const noexcept;
    [[nodiscard]] float recover_accel() const noexcept;
    [[nodiscard]] float down_seconds() const noexcept;

    JPH::PhysicsSystem &physics_;
    std::uint32_t moving_layer_;
    std::uint32_t steel_layer_;
    JPH::BodyID player_;
    JPH::Ref<JPH::Shape> shape_;
    JPH::Ref<JPH::Shape> hips_;   // way_clear's sweep: her width at her hips
    SharedSurvival *shared_survival_ = nullptr;
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
    bool patrol_only_ = false; // tests: no post to guard, the loop only

    // The hold (matron_hold.cpp).
    float hold_s_ = 0.0F;
    float grab_cooldown_s_ = 0.0F;
    float calm_s_ = 0.0F;            // after a Spare: she walks off, no hunt
    bool grip_on_ = false;
    bool grip_was_on_ = false;
    bool player_braced_ = false;
    float thrash_ = 0.0F;
    bool thrash_failed_ = false;
    float pull_n_ = kHandReachPullN;
    float arm_reach_m_ = kArmReachM;
    JPH::RVec3 hand_target_ = JPH::RVec3::sZero();
    JPH::Vec3 hand_target_v_ = JPH::Vec3::sZero();
    float rim_x_ = 0.0F, rim_z_ = 0.0F;     // where she stands at the rail
    float rim_nx_ = 0.0F, rim_nz_ = 1.0F;   // out over the shaft from there
    bool aim_ledge_ = false;
    JPH::RVec3 aim_ = JPH::RVec3::sZero();  // the ledge's spot she lets go over
    float dangle_s_ = 0.0F;                 // this dangle's length
    JPH::Ref<JPH::Shape> player_cast_shape_;
    JPH::Ref<JPH::Shape> hang_room_shape_;   // the player's body, swept for a ledge
    int hold_phase_ = 0;                    // within a state: 0 heave/lift, 1 out/lowered, 2..
    bool let_go_ = false;
    JPH::Vec3 let_go_push_ = JPH::Vec3::sZero();
    int spare_for_test_ = -1;

    JPH::RVec3 hand2_target_ = JPH::RVec3::sZero();
    JPH::Vec3 hand2_target_v_ = JPH::Vec3::sZero();
    JPH::RVec3 wrist_taken_at_ = JPH::RVec3::sZero();
    JPH::RVec3 player_wrist_ = JPH::RVec3::sZero();
    int player_arms_left_ = 2;
    float pull2_n_ = kHandReachPullN;
    bool wrist_on_ = false;
    bool arm_throw_ = false;
    JPH::Vec3 arm_throw_v_ = JPH::Vec3::sZero();
    int armplay_for_test_ = -1;
    bool armplay_done_ = false;   // this hold's ArmPlay is over
    int fatal_beat_ = 0;          // Fatal, while Claimed
    int fatal_sub_ = 0;           // within a beat: 0 her hand going to it, 1 on it
    bool fatal_at_work_ = false;  // at the bollard or the trip handle, until pushed out of reach
    float fatal_best_off_ = 1.0e9F;
    float fatal_stall_s_ = 0.0F;
    FatalPoints fatal_points_{};
    JPH::RVec3 knee_target_ = JPH::RVec3::sZero();
    bool leg_toss_ = false;
    JPH::Vec3 leg_toss_v_ = JPH::Vec3::sZero();
    std::uint32_t waste_not_ = 0;
    std::uint32_t arm_tears_ = 0, arm_throws_ = 0, claims_ = 0;
    float dangle_for_test_s_ = -1.0F;
    std::uint32_t grabs_ = 0, grab_misses_ = 0, carries_ = 0, dangles_ = 0, armplays_ = 0, releases_ = 0,
                  spares_ = 0, grips_torn_ = 0, brace_losses_ = 0, carry_drops_ = 0;
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
    float rage_ = 0.0F;
    Stage stage_ = Stage::Pest;
    float exposure_s_ = 0.0F;         // seconds out of her locker, alive
    float heard_level_ = 0.0F;        // this tick's loudest sound at her ear, 0..1
    float heard_level_last_ = 0.0F;
    float shove_cooldown_s_ = 0.0F;
    std::uint32_t shoves_ = 0;
    std::vector<Throwable> throwables_;
    std::uint32_t handled_ = 0;
    std::uint64_t player_holding_ = 0;
    int fetch_ = -1;
    float fetch_best_m_ = 0.0F;        // the nearest she has come to it on this fetch
    float fetch_stall_s_ = 0.0F;       // since she last came nearer
    std::uint32_t fetch_refused_ = 0;  // the pieces she gave up on, a bit each
    float fetch_refused_s_ = 0.0F;     // till she tries them again
    int held_ = -1;
    JPH::Ref<JPH::PointConstraint> hold_;
    JPH::Vec3 hold_offset_ = JPH::Vec3::sZero();   // the hand point, from her centre, drawn in
    float throw_windup_s_ = -1.0F;                 // < 0: not rearing back
    float throw_cooldown_s_ = 0.0F;
    int thrown_ = -1;
    float thrown_s_ = 0.0F;
    std::uint32_t takes_ = 0;
    std::uint32_t throws_ = 0;
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
    // The bay (bay.hpp): its bracket as a level of hers, and its rungs.
    int bay_level_ = -1;
    int bay_climb_ = -1;
    JPH::BodyID bay_leaf_;
    bool bay_open_ = true;
    bool bay_hooked_ = false;
    Trap trap_ = Trap::Off;
    float trap_s_ = 0.0F;        // in this phase
    float trap_lost_s_ = 0.0F;   // holding with the player gone
    std::uint32_t traps_ = 0;
    static constexpr std::uint32_t kTrapsPerRun = 3;
    static constexpr float kTrapFromM = 12.0F;          // she and the player within this of the bay, either side
    static constexpr float kTrapLostM = 25.0F;          // holding, the player farther than this from her
    static constexpr float kTrapLostSeconds = 20.0F;    // for this long: she gives it up
    // Her trap's step (true: it moved her this tick), and whether she lets go.
    bool trap_step(float dt, JPH::RVec3 at, JPH::RVec3 player, bool saw, bool heard, bool on_deck);
    bool trap_lets_go(JPH::RVec3 player, float dt);
    int guess_climb_ = -1;
    int climb_ = -1;
    const traversal::AffordanceMap *routes_ = nullptr;
    std::unique_ptr<traversal::Climber> climber_;
    Journey journey_;
    std::vector<int> journey_edges_;
    std::vector<int> journey_avoid_;   // edges this journey found it could not start
    std::vector<JPH::Vec3> journey_walk_;
    std::size_t journey_walk_i_ = 0;
    float journey_wp_best_ = 1.0e9F;
    float journey_wp_stall_s_ = 0.0F;
    bool journey_rerouted_ = false;
    float journey_wait_s_ = 0.0F;
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
    // matron_moves.cpp (above): pace, the stall watchdog, crumbs, hush.
    std::uint32_t rng_ = 0x9E3779B9U;
    int fixed_pace_ = -1;   // tests only: one pace, no hush (fix_pace_for_test)
    Pace pace_ = Pace::Stalk;
    float pace_hold_s_ = 0.0F;
    bool pace_saw_ = false;
    float hunt_s_ = 0.0F;
    std::uint32_t pace_changes_ = 0;
    float stall_window_s_ = 0.0F;
    float stall_x_ = 0.0F, stall_z_ = 0.0F;
    int stall_rung_ = 0;
    float stall_s_ = 0.0F;
    float longest_stall_s_ = 0.0F;
    std::uint32_t stalls_ = 0;
    std::uint32_t step_ups_ = 0;
    std::uint32_t clambers_ = 0;
    std::uint32_t sidesteps_ = 0;
    std::uint32_t reroutes_ = 0;
    bool clamber_now_ = false;   // the watchdog's first rung: clamber or vault whatever her feet feel
    float sidestep_s_ = 0.0F;
    float sidestep_x_ = 0.0F, sidestep_z_ = 0.0F;
    struct Blocked {
        float x, z, s;
    };
    std::vector<Blocked> blocked_;
    mutable float last_way_x_ = 0.0F, last_way_z_ = 0.0F;
    mutable bool last_way_node_ = false;
    struct Crumb {
        float x, z, floor_y;
    };
    std::array<Crumb, kCrumbs> crumbs_{};
    int crumb_count_ = 0;
    int crumb_head_ = 0;   // the next slot written
    std::uint32_t crumbs_walked_ = 0;
    int crumb_on_ = -1;    // the age (0 newest) of the crumb she is walking to
    float quiet_s_ = 0.0F;
    float hush_s_ = 0.0F;
    bool hush_spent_ = false;
    std::uint32_t creaks_ = 0;
    // A far noise she let go while guarding: she turns to it a moment.
    float look_x_ = 0.0F, look_z_ = 0.0F;
    float look_s_ = 0.0F;
    std::uint64_t catches_ = 0;
    float plank_peak_n_ = 0.0F;
    float planks_clock_ = 0.0F;
    // Her balance (above).
    struct Near {
        JPH::BodyID body;
        JPH::Vec3 velocity = JPH::Vec3::sZero();
        JPH::RVec3 at = JPH::RVec3::sZero();
        float mass_kg = 0.0F;
        float radius = 0.0F;
    };
    std::array<Near, kMaxNear> near_{};
    std::size_t near_count_ = 0;
    JPH::Vec3 set_velocity_ = JPH::Vec3::sZero();   // hers after she moved, before the step
    // The blow being dealt (above): her change, the strikers' impulse on her
    // mass (horizontal), and a weight's come down on her; since the last
    // tick a striker was against her; what kind of blow it is.
    JPH::Vec3 blow_change_ = JPH::Vec3::sZero();
    JPH::Vec3 blow_impulse_ = JPH::Vec3::sZero();
    float blow_above_ = 0.0F;
    int blow_quiet_ = kBlowQuietTicks;
    bool blow_low_ = false;
    bool blow_from_above_ = false;
    bool blow_faced_ = false;
    bool blow_staggered_ = false;   // this blow has already staggered her (and may yet put her down)
    JPH::Vec3 blow_from_ = JPH::Vec3::sZero();
    bool sensed_ = false;                           // near_ and set_velocity_ are this tick's
    State resume_state_ = State::Hunting;           // what she was about when a blow staggered her
    float stagger_s_ = 0.0F;
    float down_s_ = 0.0F;          // left to lie there
    float down_for_s_ = 0.0F;      // seconds on the steel
    float set_s_ = 0.0F;           // just risen: her step doubled
    float fall_x_ = 0.0F, fall_z_ = 1.0F;
    bool lying_ = false;           // her shape is a lying (or kneeling) one
    bool kneeling_ = false;
    JPH::Ref<JPH::Shape> down_shape_;
    std::uint32_t staggers_ = 0;
    std::uint32_t knockdowns_ = 0;
    float blow_mps_ = 0.0F;
    // The corner pipe (above): where she is along her way on it.
    enum class PipePhase : std::uint8_t { Off, Mount, Climb, Dismount, Slide, Stunned };
    PipePhase pipe_phase_ = PipePhase::Off;
    float pipe_from_ = 0.0F;   // the floor she got on from, and the one she is going to
    float pipe_to_ = 0.0F;
    int pipe_waypoint_ = 0;
    float pipe_s_ = 0.0F;      // her clock on it (the changed jerks)
    float pipe_hold_m_ = 0.0F; // her hands' way since the last hold
    float pipe_slide_mps_ = 0.0F;
    float pipe_stop_y_ = 0.0F; // the flange top she slides to
    float pipe_stun_s_ = 0.0F;
    float pipe_listen_s_ = 0.0F; // off the pipe, still and listening
    float pipe_slam_mps_ = 0.0F;
    float pipe_peak_force_n_ = 0.0F;
    JPH::Vec3 pipe_expect_dv_ = JPH::Vec3::sZero();
    float pipe_peak_power_w_ = 0.0F;
    std::uint32_t pipe_links_ = 0;
    std::uint32_t pipe_holds_ = 0;
    std::uint32_t pipe_flanges_ = 0;
    std::uint32_t pipe_slams_ = 0;
    View view_{};
};

} // namespace scraperx::sim
