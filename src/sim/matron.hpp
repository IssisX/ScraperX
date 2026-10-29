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
// Turn she vaults what blocks her way (kVaultUpMps), except a loose crate.
// A loose crate is not vaulted. Once it has stopped her, she drives it off
// her line onto the side that still has floor, on the same force and power
// as the rest of her. A band with a drop on both sides, she does not push
// the crate on down it. The shove of the player, further down, is not this.
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
// the Turn she climbs it by her Climber's rung moves (matron_traversal.hpp):
// real rungs in her reach, a capped pull out of her one budget, gravity on.
// A missing rung, a hold gone and blocked clearance release her; at its head
// she goes over the lip onto verified floor. The riser from
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

#include "sim/matron_mover.hpp"
#include "sim/matron_perception.hpp"
#include "sim/matron_routecraft.hpp"
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
        // Her hands on the player, at every stage (matron_hold.cpp): no kill
        // on touch. Her hand goes for the ankle and closes on it (Grab), she
        // walks them upside down to the shaft's rail (Carry), holds them out
        // over the drop (Dangle), tears an arm off (ArmPlay), and sets them
        // back on the deck (Spare): never let go over open air (the owner,
        // 2026-10-28), but by the fatality (kFatalTargetS: her second catch,
        // held out over the drop and let go; Release is her hand out after a
        // hold, or as she watches him fall). Caught, he is hers (Cory,
        // 2026-10-27, 6:51 PM): only his own blow landed on her (his punch's
        // tag, however small the blow) frees him, and only before the rip
        // begins (her other hand going for his arm: rip_begun()): her hand opens, and he
        // is set down on her deck (his_blow); from then on no blow breaks
        // it. Any other blow, and her own hard landing, she takes on her feet
        // with him in her hand (take_blow). Before her other hand has his
        // wrist, a body that fights its way out of her ankle hand (kicks,
        // thrashing) gets away whole, and one the world tears from it she
        // takes again (grip_lost). Once she has the wrist, nothing breaks the
        // rip (Cory, through PE): her ankle grip lost however (his kick, his
        // thrashing, the world), the arm stays in her hand as the body pulls
        // away (physics_world_grab.cpp tear_grip), and he goes on one-armed,
        // or, his last, the run is hers. Only her own giving up past
        // kMaxArmReposes re-poses (the arm out of her reach) ends it with
        // the arm on: she keeps the ankle and sets him down on her deck
        // (Spare, deck only), never over the drop.
        Grab = 12,
        Carry = 13,
        Dangle = 14,
        ArmPlay = 15,
        Release = 16,
        Spare = 17,
        // The fatality's floor version (no edge in her reach), or both the
        // player's arms gone: she has won (matron_fatality.cpp, the
        // physics_world fatality); it ends in a death and the checkpoint's
        // restore, the body whole.
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
    // Her home deck: where her locker stands and where she wakes. By default
    // her own deck (kHomeDeckTop, the locker at kLockerY); world setup may
    // move it (set_home). kDeckTop stays 154: the bay, the catwalk and the
    // 176 cages are where they are.
    static constexpr float kHomeDeckTop = kDeckTop;
    static constexpr float kLockerY = kHomeDeckTop;

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
    static constexpr std::size_t kMaxAcousticHits = 16;
    static constexpr std::size_t kMaxAcousticQueries = 4 * (16 + 1);
    static constexpr float kObstructionMuffle = 2.0F;
    static constexpr float kAcousticApertureM = 0.30F; // narrow members leave room for sound to bend around
    static constexpr float kAcousticMemorySeconds = 40.0F;
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
    static constexpr float kSightM = 9.0F;       // her paces' "close sight" (choose_pace), not her eyes' reach
    static constexpr float kEyesHalfM = 0.15F;   // her two lines of sight, either side of her middle

    // Her eyes: the sensory frame her sight is cast from (Matron::eyes). No
    // sight of the player is had any other way: not round her, not from her
    // knees. CHOSEN, each a human's:
    // - where: 1.69 m over her soles standing (the rig's authored eye height,
    //   her centre at kStandCentre 1.05 m), a face's depth forward of her
    //   middle, turned with her head;
    // - her head: her body's facing (yaw_), turned up to kHeadTurnRad (a neck's
    //   70 degrees) toward a place she is looking at (look_s_);
    // - sight: within kSightHalfFovRad either side of where her head faces
    //   (60: the binocular field a person picks out a figure in, 120 wide),
    //   out to kEyeRangeM. 16 m over 120 degrees covers about the floor her old 9 m
    //   all-round sight did (268 against 254 square metres), now only where
    //   her eyes point. The tower has no light model: range is her only dark.
    //   Up or down her head tips as far as kHeadPitchRad (70) and the cone
    //   goes with it: a climber straight over her head is seen.
    // - peripheral, weaker and labeled so (Sense::Peripheral): past that, to
    //   kPeripheralHalfFovRad (100: the edge of a person's 200-degree field),
    //   only within kPeripheralM. It is no sighting: it turns her head and is
    //   a lead with an age, as a sound is.
    // Behind her (past 100 degrees) she has her ears only: his noise, and
    // within kBreathM his breath and the steel taking his weight (hearing,
    // labeled so; a person knows someone stands at their back).
    static constexpr float kBreathM = 2.0F;
    static constexpr float kEyeAboveCentreM = 0.64F;
    static constexpr float kEyeForwardM = 0.12F;
    static constexpr float kHeadTurnRad = 1.22F;
    static constexpr float kSightHalfFovRad = 1.047F;
    static constexpr float kHeadPitchRad = 1.22F;
    static constexpr float kEyeRangeM = 16.0F;
    static constexpr float kPeripheralHalfFovRad = 1.745F;
    static constexpr float kPeripheralM = 4.0F;
    // The stalk (matron_stalk.cpp). The owner, 2026-10-28: not a one-track
    // mind. She stalks a player who has seen her; she takes one who has not.
    // RULE, general: she stalks while
    //   - he has turned his face to her, and she saw it (his head's facing
    //     from the sim, his head seen by her eyes; a free read of his camera
    //     is not hers), at least once since this encounter began;
    //   - her rage is still a pest's (his noise has not made her Dangerous);
    //   - she has had him in sight within kStalkSightMemoryS, on her own floor;
    //   - she has stalked him under kStalkPatienceS;
    //   - he is beyond kStalkStrikeM of her;
    //   - he does not run: his run heard (kStalkRunLoudness, as loud as the
    //     run that reels her in at her fastest) makes him prey to chase.
    // Any of those broken, she attacks: the hunt, the Turn, her catch, her
    // hold and the fatality, all as they were. Stalking: under his gaze she
    // freezes; looked away from, she closes, striding (kStrideMps) beyond
    // kStalkFarM, creeping (kCreepMps) to kStalkNearM, and there she holds,
    // facing him. Her sight-triggered Turn waits while she stalks.
    static constexpr float kStalkNearM = 3.0F;
    static constexpr float kStalkFarM = 6.0F;
    static constexpr float kStalkStrikeM = 1.6F;
    static constexpr float kStalkPatienceS = 20.0F;
    static constexpr float kStalkSightMemoryS = 1.0F;
    static constexpr float kStalkRunLoudness = 0.35F;
    // His face turned to her: her eyes within 35 degrees of where his head
    // faces (the middle of a person's view, where a figure is looked at, not
    // glimpsed). His eyes: 0.7 m over his capsule's middle (1.6 m standing).
    static constexpr float kFaceTurnedCos = 0.819F;
    static constexpr float kPlayerEyeAboveCentreM = 0.7F;
    // The ladder he climbs (matron_stalk.cpp): in a stalk, seen on a ladder
    // off her floor and watched kAmbushWatchS, she weighs her way to its head
    // (her scan's plan, not by that ladder) against his (what is left of it
    // at the rate she saw him climb). Hers shorter by kAmbushMarginS, she
    // goes there by her own travel and waits at its head, facing it, up to
    // kAmbushWaitS. Else as before: after him, up the same ladder, or not.
    static constexpr float kAmbushWatchS = 0.5F;
    static constexpr float kAmbushMarginS = 2.0F;
    static constexpr float kAmbushWaitS = 45.0F;
    static constexpr float kAmbushColumnM = 1.0F;   // edges at the same ladder: its rungs, either way
    // What she has of him, and how: the provenance of a lead.
    enum class Sense : std::uint8_t { None = 0, Sight = 1, Peripheral = 2, Hearing = 3, Touch = 4 };
    // One look at a point: unseen, at the edge of her eye, or seen.
    enum class Glimpse : std::uint8_t { None = 0, Peripheral = 1, Sight = 2 };
    struct Eyes {
        JPH::RVec3 at = JPH::RVec3::sZero();
        JPH::Vec3 forward = JPH::Vec3::sAxisZ();   // level: where her head faces
        bool open = false;
    };
    static constexpr float kLoseTrackSeconds = 9.0F;
    static constexpr float kSearchSeconds = 8.0F;
    // Her way to a place she searches may take kSearchTripSeconds and twice
    // its straight-line walk at kSearchMps (her lines and the climber's
    // trail wind); longer, it cannot be reached and she gives it up, as she
    // does when the sound she goes on is older than kAcousticMemorySeconds.
    static constexpr float kSearchTripSeconds = 6.0F;

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
    // A loose crate is not one of those rungs, and it is not a vault. Once it
    // has stopped her, and one side of her line still has floor, she drives
    // it onto that floor. Both sides a drop, she leaves it, and these rungs
    // are how she gets round it.
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
    // Below this she is gone (fell): a fall of 13 m from the deck. Riding
    // (a moving floor, a line's cable) or on a climb she is not falling.
    // Was 12 m. A measured 14 m drop still lands harder than this and kills.
    static constexpr float kFallenBelowM = 13.0F;
    // What kills her is the landing: a stop as hard as the end of a fall of
    // kFallenBelowM (16 m/s), on whatever she lands on.
    static constexpr float kLethalLandingMps = 16.00F;
    static constexpr float kOffTheWorldY = -50.0F;

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
    // stopped, she lets go kLetGoSeconds later. A machine floor she is
    // headed onto, still within a clamber, she steps onto with the same
    // force budget as the rest of her. A loose crate is not that floor.
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
    // Her hands on the corner pipe: steady pull (the player climbs at 0.9),
    // finite hand reach from the body centre and combined grip rating, and
    // her push over a rail. (Her ladders are her Climber's moves, with its
    // own reach and pace, traversal::Limits, inside the same reach.)
    static constexpr float kClimbMps = 0.6F;
    static constexpr float kClimbReachM = 1.9F;
    static constexpr float kClimbGripMaxN = 5500.0F;
    static constexpr float kMantleMps = 1.4F;
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
    static constexpr float kRiserBridgeTop = 155.18F;   // what she goes over at its head
    // C3 below that (band_stack.cpp build_c3): the stepped walkway up from the
    // 132 deck's south band across the shaft to a landing (135.5), the duct's
    // 1.7 m face up from it (137.2), and the wall ladder from the duct's north
    // end to the 143 walkway plate: where she hangs on the ladder and the duct.
    static constexpr float kC3WallX = -10.5F;
    static constexpr float kC3WallHangZ = -135.85F;
    static constexpr float kC3DuctHangZ = -144.40F;
    static constexpr float kC3LandingTop = 135.5F;
    static constexpr float kC3DuctTop = 137.2F;

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
    // Lunge push only. Extra force past 9.8 kN did not raise the measured
    // peak (8.08 m/s at 9.8 kN, 8.10 at 11 kN): the burst's power limit is
    // the wall. 65 kW on that 0.2 s burst is the stronger push. Walk, climb,
    // and shove stay on 8 kN and 44 kW.
    static constexpr float kTurnLungeForceN = 9800.0F;
    static constexpr float kTurnLungePowerW = 65000.0F;
    // Held to a ramp she is on only until the stall watchdog has had her stalled this long on it
    // (patch Item 3; moved here from matron.cpp so matron_found_climb.cpp can use the same value).
    static constexpr float kRampStallS = 3.0F;

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
    // What her hands on the player are (physics_world.cpp): at every stage
    // they take hold (her hold, below). Only while they cannot (a grab's
    // cooldown, a body on a ladder), and short of fully changed, a shove,
    // kShoveMps along the floor and kShoveUpMps up, once in
    // kShoveCooldownSeconds. A pest's shove is away from her; dangerous, she
    // aims it at the shaft's rim when the player is within kShoveEdgeReachM of
    // it (the owner: she tries to nudge him off a band). A crate is not this
    // shove. That one is above, with getting unstuck.
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
    // riser (world_solids.inc Riser: 0.7 m square at x 15.8, z -134.2, solid
    // from grade to 154, a 0.35 m flange reaching x 15.25 under every deck).
    // Too thick for the climber's hands; hers wrap it. Her line runs down its
    // west side clear of every flange (her 0.30 m against their x 15.25), so
    // the steel she climbs is steel she meets: she is on her own layer, with
    // gravity, and only her hands hold her (matron_pipe.cpp). From a deck she
    // takes the north rail's top (kPipeRailTopM over the deck at kPipeRailZ),
    // lifts herself over it and out to her line, and takes the riser; at 154,
    // where the riser ends flush with the deck, she lowers herself on the rail
    // until it is in reach. On it her grip is friction: her squeeze, at most
    // kPipeSqueezeN, times kPipeFriction holds her weight; she climbs it hand
    // over hand and lets herself down it at her stage's pace. At the other
    // deck she takes its rail and goes over it onto the deck, done only with
    // her feet on its steel. Frail, she cannot take hold of it (as her
    // ladder); dangerous she slides down it held by her forearms and knees
    // and climbs it hand over hand; fully changed she comes down it head first
    // in jerks. Each kPipeHoldM her hands go is a knock on the pipe, each
    // flange passed a bong: she is heard on it from the moment she takes hold
    // (an 11 m story takes her at least 4 s). A blow on it past kPipeTearMps
    // opens her hands: her legs alone slow her (kPipeSlideAccel, her squeeze
    // against her weight) until they meet the flange under her and she slams
    // onto it, stunned a while by how hard, alive; then she goes on.
    static constexpr float kPipeLineX = 14.85F;
    static constexpr float kPipeLineZ = -133.75F;
    static constexpr float kPipeDeckX = 14.85F;    // where she stands at the rail to go over it
    static constexpr float kPipeDeckZ = -132.55F;
    static constexpr float kPipeCentreX = 15.8F;
    static constexpr float kPipeCentreZ = -134.2F;
    static constexpr float kPipeRailZ = -133.0F;
    static constexpr float kPipeRailTopM = 1.09F;  // the north rail's top over its deck
    static constexpr float kPipeOverRailM = 0.15F; // her feet over the rail's top, going over it
    static constexpr float kPipeSqueezeN = 4500.0F;
    static constexpr float kPipeFriction = 0.7F;
    static constexpr float kPipeLowestFloor = 132.0F;   // the stack's deck 12
    // Her feet's brace on the riser, hand over hand (matron_contact.cpp): from
    // her shins, this far over her soles, level toward its axis, within her
    // legs' reach (her line is 0.6 m off the riser's west face).
    static constexpr float kBraceShinM = 0.45F;
    static constexpr float kBraceReachM = 1.0F;
    static constexpr float kPipeDownMps = 2.6F;
    static constexpr float kPipeUpMps = 0.7F;
    static constexpr float kPipeChangedDownBurstMps = 4.2F;
    static constexpr float kPipeChangedUpBurstMps = 2.6F;
    static constexpr float kPipeChangedPauseMps = 0.3F;
    static constexpr float kPipeHoldM = 0.45F;
    static constexpr float kPipeFlangeUnderDeckM = 1.225F;   // a flange's top under its deck (her bong as she passes it)
    // CHOSEN: a drum let go onto her from the rail above tears her off (0.99
    // m/s measured on her physical grip; 1.15 when she was moved by velocity),
    // a toolbox let go the same way does not (tests: matron pipe drum, toolbox).
    static constexpr float kPipeTearMps = 0.8F;
    static constexpr float kPipeSlideAccel = 4.4F;
    static constexpr float kPipeStunSeconds = 0.5F;
    static constexpr float kPipeStunPerMps = 0.25F;   // and this per m/s she slams past kPipeStunFromMps
    static constexpr float kPipeStunFromMps = 5.0F;
    static constexpr float kPipeSwatNs = 120.0F;      // the most her swat gives a loose piece in her way
    // A landing this hard puts her down a while; from kLethalLandingMps it
    // kills her. Her own jump comes back near 7.2 m/s. The line sits above
    // that return with room for a short miss, so the jump does not put her
    // down. A longer fall still does (the home ladder opens her hands at
    // about 9.5 m/s).
    static constexpr float kHardLandingMps = 8.20F;
    // A lead a story away by the pipe is worth chasing longer than one on her
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
    // kArmReachM from her shoulder, wherever her lean and crouch have it),
    // driven at its pose by her muscle: a spring-damper capped at
    // kHandPullN (kHandReachPullN reaching), its reaction on her body. The
    // grip itself is a SwingTwist hand-to-ankle; its load over
    // kGripMaxForceN, or its strain (load past kGripStrainFloorN, over time,
    // less kGripStrainDecay a second) past kGripTearNs, tears it out of her
    // hand. A calm hang is 0.83 kN; the player's thrashing gets there, and so
    // can the world (her heave hooking a still body on a rail), a tear she
    // takes them again from (grip_lost).
    // Her body's reach, as it is drawn (godot/presentation/life/matron/
    // matron_rig.json; the owner, 2026-10-29: "the matron's arm is like 12
    // feet long, bro"): her arm is kUpperArmM + kForearmM to the wrist, the
    // ankle held in her palm kGripInHandM past it (her hand is kHandLongM).
    // Her shoulder stands kTorsoUpM over her hips (kHipUpM over her feet),
    // kTorsoFwdM ahead of them with her hunch, kShoulderSideM to the side; to
    // reach further she leans from the hips (up to kLeanMaxRad, kLeanRadps)
    // and crouches (up to kCrouchMaxM, kCrouchMps), her shoulder carried
    // with her trunk: her hand is never further than kArmReachM from it.
    static constexpr float kUpperArmM = 0.262F;
    static constexpr float kForearmM = 0.248F;
    static constexpr float kHandLongM = 0.228F;
    static constexpr float kGripInHandM = 0.09F;
    static constexpr float kArmReachM = kUpperArmM + kForearmM + kGripInHandM;   // 0.60
    static constexpr float kHipUpM = 0.98F;
    static constexpr float kTorsoUpM = 0.46F;
    static constexpr float kTorsoFwdM = 0.08F;
    static constexpr float kShoulderSideM = 0.157F;
    static constexpr float kShoulderUpM = kHipUpM + kTorsoUpM;   // over her feet, standing: 1.44
    static constexpr float kLeanMaxRad = 1.3F;
    static constexpr float kLeanRadps = 2.5F;
    static constexpr float kCrouchMaxM = 0.5F;
    static constexpr float kCrouchMps = 1.0F;
    static constexpr float kLoadedPostureScale = 0.6F;
    // Her hand's poses stay kPoseReachM from her shoulder: her arm's limit is
    // met only by a jolt, not held against.
    static constexpr float kPoseReachM = kArmReachM - 0.06F;
    // Holding him out over the drop in her fatality, her arm all the way out
    // (his weight takes up the rest of it, a centimetre or two).
    static constexpr float kFatalPoseReachM = kArmReachM - 0.02F;
    static constexpr float kArmLimitHz = 15.0F;   // her arm's limit, stiff: 7 cm at her grip's 3.8 kN
    // Where she holds him (the owner, 2026-10-29: "we can modify the way she
    // carries or where she holds or how she holds"): by the thigh,
    // kHeldBelowMiddleM below his middle, so a real arm hangs him head down
    // and her other hand can reach his arm.
    static constexpr float kHeldBelowMiddleM = 0.25F;
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
    static constexpr float kGrabInvertMaxS = 1.5F;   // waiting for him to hang head down, at most
    static constexpr float kGrabCooldownSeconds = 1.5F;   // after a miss
    static constexpr float kAfterReleaseSeconds = 20.0F;  // no new grab for this long after a hold ends
    // A grip the world tears from her hand before the arm she takes again
    // where they land, and the arm comes off where she stands (grip_lost,
    // retake): at most kMaxRetakes times in one hold. Torn once more, it is
    // a Release: no hold loops on the same steel.
    static constexpr int kMaxRetakes = 2;
    // Carry: at kCarryPaceScale of her hunt, the body upside down at her
    // side, her hand up by her head (kCarryHandUpM over her feet, as high
    // as her arm goes there: his head just off her deck), at most
    // kCarryMaxSeconds to the rail; past it she drops them where she is.
    static constexpr float kCarryPaceScale = 0.5F;
    static constexpr float kCarryTurnRate = 2.5F;   // rad/s, him in her fist
    static constexpr float kCarryAccel = 2.5F;   // m/s2, him in her fist: he swings back at most 14 degrees
    static constexpr float kFatalCarryPaceScale = 0.75F;   // of her hunt: she runs him to her fatality's edge
    static constexpr float kFatalCarryAccel = 7.0F;   // m/s2, to her fatality's edge
    static constexpr float kCarryComingOverMps = 1.4F;
    static constexpr float kCarryHeadDownCos = 0.6F;   // near enough head down to carry him at pace   // her pace while he comes over head down
    static constexpr float kCarryHandUpM = 1.66F;
    static constexpr float kCarrySideM = 0.66F;
    // Her heave: her fist up kHeaveUpM over her feet (over the rail's line,
    // tipping him onto it; kHeaveOutM out in the rip). Then the tip: out
    // kTipPastM past the side and down to kTipUpM over her feet, at
    // kTipPaceScale of her hand's pace, till his middle is kTipDoneM past
    // it. Back in: her fist up over the rail's line again, then in to
    // kTipBackFwdM in from the side and kTipBackUpM up as she steps back,
    // till his middle is kTipDoneM in from it.
    static constexpr float kHeaveUpM = 1.85F;
    static constexpr float kArmPlayUpM = 1.48F;   // the rip: him hauled up to her, his arm in her other hand's reach
    static constexpr float kTipPastM = 0.40F;
    static constexpr float kTipUpM = 1.0F;
    static constexpr float kTipPaceScale = 0.6F;
    static constexpr float kTipDoneM = 0.30F;
    static constexpr float kTipBackFwdM = 0.45F;
    static constexpr float kTipBackUpM = 1.60F;
    static constexpr float kTipBackMps = 0.35F;
    static constexpr float kCarryMaxSeconds = 10.0F;
    static constexpr float kRimStandM = 0.42F;        // her middle this far in from the shaft's rail
    static constexpr float kRimArriveM = 0.30F;
    static constexpr float kRimBraceM = 0.9F;        // braced: her middle within this of the rail
    static constexpr float kRimSearchM = 16.0F;       // rail spots she weighs for a ledge below
    // Dangle: the heave out over the rail (the hand up at kHeaveUpM and
    // kHeaveOutM out from her middle), then down to kDangleHandUpM over her
    // feet, kDangleOutM out, leant out over the rail: the body hangs past the
    // deck's edge, head down below it.
    static constexpr float kHeaveOutM = 0.84F;
    static constexpr float kDangleOutM = 1.00F;
    static constexpr float kDangleHandUpM = 1.20F;
    static constexpr float kDangleMinSeconds = 3.0F;
    static constexpr float kDangleSpanSeconds = 3.0F;
    // Then she sets them back on her deck (Spare), every time: an ordinary
    // hold never lets go of them over open air (the owner, 2026-10-28: being
    // dropped over open air is the fatality's alone). The rail she takes them
    // to is still the one with a ledge below it whose fall is under
    // kReleaseSafeMps where there is one (choose_rim), and the push her arm
    // gives a body she lets go of is at most kReleasePushMps.
    static constexpr float kReleaseSafeMps = 14.4F;   // kAnkleBreakSpeed * 1.2
    static constexpr float kReleasePushMps = 4.0F;
    static constexpr float kReleasePoseSeconds = 1.0F;
    static constexpr float kSpareSetSeconds = 1.2F;   // the collar put straight
    static constexpr float kSpareCalmSeconds = 20.0F; // then she walks off, no hunt this long
    // After any arm torn off, set down alive (spared, or pulled out of her
    // hand), her hands do not take him again for this long (the owner,
    // 2026-10-28: after an arm tear she can't grab him again right away).
    // Spared, she walks off first (kSpareCalmSeconds), then hunts him by
    // sound; a touch meanwhile is her shove, short of fully changed.
    static constexpr float kRegrabAfterTearS = 30.0F;
    // His own blow landed in her hold before the rip (the owner, 2026-10-27:
    // it frees him; his punch's tag, player_blow_landed, however small the
    // blow): her hand opens on him (his_blow). Over her deck, at once; out
    // over the drop (Dangle, or on the way back in), she reels back from the
    // edge this far, him still in her fist, and sets him down on her deck
    // first (reel). Then her feet catch her up a moment (Staggering), and her
    // hands stay off him kRegrabAfterTearS.
    static constexpr float kReelBackM = 0.8F;

    // The fatality (the owner, 2026-10-28; matron_hold.cpp): her second
    // catch, his one arm left. She carries him to the nearest edge of her
    // floor her scan's map shows with open air under it (choose_fatal_edge:
    // a walkable top's side, the body's room to hang past it clear, and the
    // first steel under it kFatalDropM and more down: a fall that kills),
    // one she can walk to straight within kFatalCarryS of her carry. There
    // she holds him out over it upside down by the ankle, her arm out (the
    // Dangle's heave and hang, the same muscle), kFatalHangWindowS from when
    // his middle is out past the lip: his own blow landed on her in it frees
    // him (reel: set down on her deck, alive, one-armed). Else she lets go
    // (the release beat, a counter for the viewer), and gravity does the
    // rest: the landing kills him, a death, and the checkpoint's restore. No
    // such edge in reach (or not reached in kFatalCarryS): the floor version,
    // her Claimed beats cut short (Lay where she stands, Pin, Wipe, Black:
    // about kFatalFloorS), ending in the same death and restore. From her
    // fingers on his ankle to her letting go, 5 to 7 s (kFatalLetGoFromS to
    // kFatalLetGoByS, kFatalTargetS at the most; matron grab fatality length
    // and band measure it).
    enum class FatalPath : std::uint8_t { None = 0, Hang = 1, Floor = 2 };
    static constexpr float kFatalTargetS = 7.0F;
    static constexpr float kFatalHangWindowS = 2.5F;
    static constexpr float kFatalCarryS = 3.0F;
    // Of it, her start, her slowing in and her turn to the edge: an edge in
    // reach is within her carry's pace times what is left (4.8 m).
    static constexpr float kFatalCarryEaseS = 1.6F;
    static constexpr float kFatalFloorS = 3.0F;
    // A lethal fall: kLethalImpactSpeedMps (20 m/s) is a 20.4 m drop.
    static constexpr float kFatalDropM = 24.0F;
    // His fall, foreseen (fall_impact_mps): his body from where it is, as it
    // moves and spins, under gravity alone, swept (its breadth and a margin)
    // to the first steel it meets; that meeting's speed down must be
    // kFatalLandMps, past kLethalImpactSpeedMps (20 m/s, simulation.hpp) with
    // room: the first landing kills, and nothing on the way breaks the fall.
    // Choosing the edge she foresees it from where he will really hang at
    // her let-go (kFatalDropOutM past her middle, still) with
    // kFatalChooseMarginM; at the let-go, from his real state, with
    // kFatalLetGoMarginM.
    // Steel may stand out under the edge (the beams under the decks below, a
    // deck's edge beam), so her arm gives him a toss out as her fingers open:
    // the least of kFatalTossMps out over the drop that the foreseen fall
    // needs (at most 0.9 m/s: 72 N*s, under her 8,000 N in the tick she lets
    // go), for what is left of his swing. The edge itself must kill with no toss,
    // from his hang and kFatalSwayM either side of it along the edge.
    // A first meeting side-on (a wall, a beam's edge he would graze) is
    // foreseen not to kill: how hard a graze closes is not foreseeable.
    static constexpr float kFatalLandMps = 23.0F;
    static constexpr float kFatalChooseMarginM = 0.15F;
    static constexpr float kFatalLetGoMarginM = 0.12F;
    static constexpr float kFatalSwayM = 0.3F;
    static constexpr std::array<float, 4> kFatalTossMps{0.0F, 0.3F, 0.6F, 0.9F};
    // From her fingers closing on his ankle to her letting go, no sooner than
    // kFatalLetGoFromS and no later than kFatalLetGoByS (the viewer fades a
    // second after it: 6-8 s from the catch to the fade). Not lethal by then
    // (his swing, steel under him she cannot get him clear of by shifting
    // along the edge, kFatalShiftM a step, kFatalShiftMaxM in all), she
    // hauls him back in and her floor version runs: she never lets go into a
    // fall he can survive.
    static constexpr float kFatalLetGoFromS = 5.2F;
    static constexpr float kFatalLetGoByS = 6.8F;
    static constexpr float kFatalShiftM = 0.4F;
    static constexpr float kFatalShiftMaxM = 1.5F;
    // Let go, she watches him fall at least kFatalWatchS, and on until he is
    // still kFatalStillS (his fall over: the death's restore usually comes
    // first), kFatalWatchMaxS at most; then she is after him.
    static constexpr float kFatalWatchS = 8.0F;
    static constexpr float kFatalStillS = 1.0F;
    static constexpr float kFatalWatchMaxS = 20.0F;
    // Her hand's pace (of kHandSpeedMps) taking him out and down over the
    // edge (an ordinary hold's: 0.6 of it). His lift is her carry's, on the
    // way. Past the rail's height she takes him down only while his swing
    // cannot bring his middle back within kFatalClearM of the lip (his
    // breadth, 0.35 m, and a hand's width: his swing back in over the rail
    // would lay him across it), and up again if it does.
    static constexpr float kFatalLowerScale = 1.0F;
    static constexpr float kFatalClearM = 0.40F;
    // His window (the owner, 2026-10-28: a punch of his landed in it frees
    // him). She stands with her front to the rail (her middle kFatalStandM in
    // from the lip) and holds him at his window's pose: her hand
    // kFatalWindowOutM out from her middle and kFatalWindowUpM over her
    // feet, him head down under it, the whole of him above her rail's top,
    // his shoulder level with hers and within his punch's reach of her body
    // (0.75 m, shoulder to fist). Her fingers are firm on his ankle: his swing
    // about them meets kFatalGripFrictionNm of friction (his weight about the
    // ankle is 640 N m a radian), so he comes to hang still. Easing into the
    // pose (kFatalEaseRate of what is left, a second), she opens the window
    // once his middle is kFatalWindowPastM out past the lip and his shoulder
    // stays within kFatalReachM of her (the punch's reach less a hand's
    // width) as he swings. His shoulder, the punch's start, is
    // kShoulderOfMiddleM along his body's up from his middle.
    static constexpr float kFatalStandM = 0.30F;
    static constexpr float kFatalWindowOutM = 0.9F;
    static constexpr float kFatalWindowUpM = 1.30F;
    static constexpr float kFatalGripFrictionNm = 80.0F;
    static constexpr float kFatalWristHz = 1.5F;   // her wrist's steadying of him, head down
    static constexpr float kFatalEaseRate = 3.0F;
    static constexpr float kFatalWindowPastM = 0.20F;
    static constexpr float kFatalReachM = 0.70F;
    static constexpr float kFatalHeadDownCos = 0.85F;   // his window opens with him this near head down
    static constexpr float kShoulderOfMiddleM = 0.25F;
    // His window over, she takes him out over the drop toward her let-go's
    // pose (kFatalDropOutM out from her shoulder and kFatalDropUpM over her
    // feet: past her arm's length, which her lean over the rail and her
    // reach take as far as they go), and lets go on the way once his
    // middle is kFatalOpenAirM out past the lip (over open air) and his fall
    // from there, as he moves, kills.
    static constexpr float kFatalDropOutM = 1.0F;
    static constexpr float kFatalDropUpM = 1.25F;
    static constexpr float kFatalOpenAirM = 0.5F;
    // Her arm's thrust out to that pose (at kFatalLowerScale of her hand's
    // pace), as she lets go on the way: he leaves her hand moving out at
    // least this fast (the edge's choice counts on it; the let-go itself
    // foresees his fall from how he really moves).
    static constexpr float kFatalThrustMps = 0.3F;
    static constexpr float kFatalHeaveBackM = 0.25F;   // her heave back before a thrust out

    // ArmPlay (matron_hold.cpp; the physics in physics_world_arm.cpp): every
    // hold, at every stage (Cory, 2026-10-27: every time she catches you she
    // rips an arm off, for sure), while the player has an arm: after the
    // dangle (or where she stands, when she cannot get them to the rail), her
    // other hand takes a wrist and pulls it away from the body, her pull
    // rising from kArmPullStartN at kRipRampNps to kArmPullMaxN (the ankle
    // hand then holds the pull and the weight: under its cap; she grips it
    // hard, the strain counted only past kGripRipFloorN). She does not give
    // it up: her hand not at the wrist in kArmReachSeconds, or the shoulder
    // not torn kArmPullSeconds into the pull, she re-poses (arm_repose: a
    // step of kArmReposeStepM back from the body, hauling it after her, her
    // weight in it; out of reach, her hand for the nearer wrist again), up
    // to kMaxArmReposes times; past that, the arm still not hers, she keeps
    // the ankle and sets him down on her deck (Spare, deck only: her deck
    // close under his body, not only under her hand; none, she walks him
    // on along her lines), no arm off, so a body caught on steel never
    // loops her. Once her other hand goes for his arm, no blow
    // breaks it; once it has the wrist, nothing does: her ankle grip lost,
    // the arm stays in that hand (the State comment). The shoulder tears
    // (physics) at kArmTearNs of the wrist's load past kArmHoldN; the arm
    // (kArmMassKg) is hers: lifted to her face, swung, thrown (kArmThrowMps)
    // onto the deck behind her. The whole of it kArmPlaySeconds from the
    // tear. The last arm gone, the run is hers (Claimed).
    static constexpr float kArmPullStartN = 1500.0F;
    static constexpr float kArmPullMaxN = 2600.0F;
    static constexpr float kGripRipFloorN = 3500.0F;
    static constexpr float kArmHoldN = 1500.0F;
    static constexpr float kArmTearNs = 600.0F;
    static constexpr float kArmMassKg = 4.5F;
    static constexpr float kArmThrowMps = 7.5F;
    static constexpr float kArmPlaySeconds = 4.5F;
    static constexpr float kArmPullMoveMps = 0.5F;
    static constexpr float kArmReachSeconds = 3.5F;
    static constexpr float kArmPullSeconds = 4.0F;
    static constexpr float kArmReposeStepM = 1.0F;
    static constexpr int kMaxArmReposes = 4;
    struct Throwable {
        JPH::BodyID body;
        std::uint64_t entity = 0;
        JPH::Vec3 handle = JPH::Vec3::sZero();   // where a hand takes it, in its own frame
        float mass_kg = 0.0F;   // read from its body by set_throwables (infinite: not a moving body)
        JPH::Vec3 laid = JPH::Vec3::sZero();   // where it lay when given (set_throwables): her deck as she knows it
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
        JPH::RVec3 climb_hand = JPH::RVec3::sZero();
        // Her left hand's own hold, on two-handed moves: the same height
        // as climb_hand, one shoulder-width (0.45 m) to her left along
        // the rung or lip. View plumbing only (see refresh_view): it is
        // reported, never gripped by movement code, and known is false
        // unless a legacy climb's support validated it, in which case
        // the rig poses the left wrist to it instead of to climb_hand.
        JPH::RVec3 climb_hand2 = JPH::RVec3::sZero();
        bool climb_hand2_known = false;
        // Her feet's real support point, not a floor level: the surface
        // probed straight under her body centre, or the climb's top deck
        // when no surface is under her (mid-mantle, feet in the air).
        // known is false when it is only her level's floor point.
        JPH::RVec3 climb_feet_support = JPH::RVec3::sZero();
        bool climb_feet_support_known = false;
        // The move that owns the pose, even while a jump or drop has no hold.
        traversal::MoveKind climb_move = traversal::MoveKind::Walk;
        int climb_phase = 0;
        bool climb_move_active = false;
        bool climb_airborne = false;
        // Her contacts as the simulation itself has them this tick
        // (refresh_contact_view): what her feet stand on by walk_toward's own
        // test (center and foot-span rays, a floor's normal, her fall against
        // it), and what the mover that has her really holds. Facts for the
        // presentation only: nothing reads them back, nothing is pushed.
        // Supported: her feet on a floor. Hand held: her hand's grip.
        // Hand 2: a second grip, which no mover takes today. Line: her
        // forearms on a line's cable. Brace: on her pipe, the riser's face
        // her feet press against (the other side of her hands' squeeze).
        bool contact_supported = false;
        JPH::BodyID contact_feet_body;
        JPH::SubShapeID contact_feet_sub;
        JPH::RVec3 contact_feet = JPH::RVec3::sZero();
        bool contact_hand_held = false;
        JPH::BodyID contact_hand_body;
        JPH::SubShapeID contact_hand_sub;
        JPH::RVec3 contact_hand = JPH::RVec3::sZero();
        bool contact_hand2_held = false;
        JPH::BodyID contact_hand2_body;
        JPH::SubShapeID contact_hand2_sub;
        JPH::RVec3 contact_hand2 = JPH::RVec3::sZero();
        bool contact_line = false;
        bool contact_braced = false;
        JPH::BodyID contact_brace_body;
        JPH::RVec3 contact_brace = JPH::RVec3::sZero();
        bool climb_sliding = false;
        bool climb_leg_driving = false;
        bool climb_bar_traverse = false;
        float climb_force_n = 0.0F;
        bool climb_lunge_drive = false;
        float foot_power_w = 0.0F;   // her feet's positive power this tick (lunge or walk), from their Budget
        std::uint32_t climb_grips = 0;
        std::uint32_t climb_releases = 0;
        bool mantling = false;
        bool climbing = false;      // on a ladder, going up it (stride: metres climbed)
        std::uint32_t climbs = 0;   // ladders she went up after the player, this run
        bool bracing_structure = false;   // her hands holding the bay's leaf
        float support_n = 0.0F;           // and what they hold up, N
        std::uint8_t trap = 0;            // her trap at the bay: 0 none, 1 to its edge, 2 down, 3 to her spot,
                                          // 4 holding, 5 climbing out
        std::uint32_t traps = 0;          // times she set it, this run
        bool clinging = false;      // holding on to a floor that moves (or just stopped)
        float floor_y = kDeckTop;   // the level she hunts: its floor's top
        float lead_x = 0.0F, lead_z = 0.0F;
        float lead_age_s = 0.0F;
        bool lead_seen = false;
        Sense lead_sense = Sense::None;   // how she had it: sight, the edge of her eye, hearing, her hands
        bool glimpsed = false;            // the player at the edge of her eye this tick (not a sighting)
        float eye_x = 0.0F, eye_y = 0.0F, eye_z = 0.0F;   // her eyes (Matron::eyes)
        float eye_yaw = 0.0F;                            // where her head faces
        bool watched = false;        // his face turned to her, seen by her this tick
        bool noticed = false;        // ...at least once this encounter
        bool stalking = false;       // stalking him (matron_stalk.cpp), not attacking
        float stalk_s = 0.0F;        // seconds stalked this encounter
        std::uint32_t stalk_attacks = 0;   // stalks that ended in her attack, this run
        bool ambush = false;         // after the ladder he climbs, to its head
        bool ambush_waiting = false; // at its head, waiting
        std::uint32_t ambushes = 0;  // ladders she went round to the head of, this run
        bool lead_valid = false;
        std::uint32_t acoustic_queries = 0;
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
        std::uint32_t pipe_links = 0;   // stories she set out along it, this run
        std::uint32_t pipe_holds = 0;   // her hands' holds on it (each a knock)
        std::uint32_t pipe_flanges = 0; // flanges she passed (each a bong)
        std::uint32_t pipe_slams = 0;   // flanges slammed onto, torn off
        float pipe_slam_mps = 0.0F;     // the last slam's speed
        float pipe_peak_force_n = 0.0F; // her grip's peak force on the pipe
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
        std::uint32_t grips_torn = 0;   // torn out of her hand, by the player or the world
        std::uint32_t retakes = 0;      // of those, the world's before the arm: taken again
        std::uint32_t arm_reposes = 0;  // ArmPlay re-posed: the wrist out of reach, or the pull not telling
        std::uint32_t brace_losses = 0; // stepped back from the rail, unbraced
        std::uint32_t carry_drops = 0;  // carried too long: dropped on the deck (no longer: set down, Spare)
        std::uint32_t arm_tears = 0;    // arms she tore off
        std::uint32_t arm_throws = 0;   // and threw
        std::uint32_t claims = 0;       // the floor version of the fatality begun (Claimed)
        // The fatality (kFatalTargetS): which path this second catch takes
        // (FatalPath: 0 none, 1 the hang over the drop, 2 the floor), the
        // hang's window (his blow frees him) and its seconds, the edge she
        // holds him out over and the way out over the drop from it.
        std::uint8_t fatal_path = 0;
        bool fatal_hang = false;
        float fatal_window_s = 0.0F;
        float fatal_edge_x = 0.0F, fatal_edge_y = 0.0F, fatal_edge_z = 0.0F;
        float fatal_out_x = 0.0F, fatal_out_z = 0.0F;
        // The release beat: rises by one as she lets go of him over the drop
        // (the viewer's slow motion, its pull-out down the drop and its fade).
        std::uint32_t fatal_release_beat = 0;
        std::uint32_t fatal_escapes = 0;   // his blow in the fatality's hold: reeled back, set down alive
        float fatal_foreseen_mps = 0.0F;   // in the hang: his fall from now foreseen to meet steel at this speed down
        float fatal_toss_mps = 0.0F;       // and her toss out with the let-go it needs
        std::uint32_t reels = 0;           // his blow out over the drop (any hold): reeled back onto her deck
        std::uint8_t fatal_beat = 0;    // Fatal, while Claimed
        std::uint32_t waste_not = 0;    // the 'Waste not.' voice hook, an edge
        std::uint8_t armplay_beat = 0;  // 0 reach, 1 pull, 2 to her face, 3 swing, 4 throw
        float hand2_x = 0.0F, hand2_y = 0.0F, hand2_z = 0.0F;
        bool hold_braced = false;
        bool grip_on = false;
        float hold_s = 0.0F;            // seconds in this hold state
        float hand_x = 0.0F, hand_y = 0.0F, hand_z = 0.0F;   // her hand's pose
        float hold_lean = 0.0F, hold_crouch = 0.0F;          // her reach's lean and crouch
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

    // `him`: the player's true state this tick (his body's centre, the noise
    // his movement makes), which she cannot open: Perception turns it into her
    // evidence (matron_perception.hpp) before anything of hers decides.
    void pre_step(float dt, const Stimulus &him);
    // Opt-in native goal. Facing selects the approach side, never a route.
    bool request_climb_goal(JPH::RVec3 goal, JPH::Vec3 facing);
    void cancel_climb_goal() noexcept { routecraft_.cancel(); }

    const MatronRoutecraft::Report &climb_report() const noexcept { return routecraft_.report(); }
    // The force her feet drive put on her body this tick (walk_toward), when no climb has it.
    float foot_force_n() const noexcept { return foot_force_n_; }
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
    // pre_step. Hers only as she sees it (Evidence::seen_holding: a throwable
    // she sees in his hands becomes his to her); and her contact sense counts
    // it as part of his body (sense_near: carried into her, it is his push).
    void set_player_holding(std::uint64_t entity) noexcept {
        extras_.holding(entity);
        push_load_ = entity;
    }
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
    // How far a sound of full loudness carries.
    static constexpr float kFullHearingM = 55.0F;
    static constexpr std::size_t kMaxSounds = 16;
    static constexpr float kOwnSoundM = 1.5F;
    void hear(float x, float y, float z, float loudness) noexcept {
        if (loudness > 0.0F && sound_count_ < kMaxSounds) {
            sounds_[sound_count_++] = {true, x, y, z, loudness > 1.0F ? 1.0F : loudness};
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
        return view_.released && view_.alive && checkpoint_freeze_;
    }
    // Reports (once) that she has just burst out, so the world commits the
    // checkpoint where the player stands.
    [[nodiscard]] bool take_released_edge() noexcept {
        const bool edge = released_edge_;
        released_edge_ = false;
        return edge;
    }
    void count_catch() noexcept { ++catches_; }
    // Her hands on the player (Cory, 2026-10-27: at any rage, every time she
    // catches you she grabs you and rips an arm off, for sure; your second
    // arm gone is the fatality). At every stage they take hold (grab_ready:
    // the grab, the carry, the dangle, an arm torn off; the last arm gone,
    // Claimed). Her stage changes how she hunts, not what a catch is. Only
    // while her hands cannot take hold (a grab's cooldown, a body on a
    // ladder) do they shove, once in its cooldown, along shove_direction: a
    // pest's or a dangerous Matron's; fully changed, never.
    [[nodiscard]] bool shoves() const noexcept { return stage_ != Stage::Changed; }
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

    // Her travel over the whole tower's real steel (matron_journey.cpp): the
    // map of what it offers her (scanned from its collision geometry, every
    // deck), and a journey to a region of it by her own moves. No route is
    // authored: she plans over the map, makes each move with her capped
    // forces, commits a region only standing on it, and on a failed move or a
    // knock off her floor plans again from wherever she stands.
    void set_routes(const traversal::AffordanceMap *routes);
    [[nodiscard]] const traversal::AffordanceMap *scanned_routes() const noexcept { return scanned_; }
    // Out of her locker with the tower not yet looked over.
    [[nodiscard]] bool wants_routes() const noexcept { return scanned_ == nullptr && body_in_world_; }
    bool travel_to(JPH::Vec3 feet);   // reads the steel if needed, then resolves this destination
    // True accepts the destination. A free, airborne body waits for real
    // support before executing a route; acceptance does not promise arrival.
    bool travel_to(int region);
    // The same, with edges her plan leaves out (a ladder with him on it).
    bool travel_to(int region, const std::vector<int> &avoid);
    [[nodiscard]] int moving_layer() const noexcept { return static_cast<int>(moving_layer_); }
    struct Journey {
        bool active = false;
        int goal = -1;
        int region = -1;        // the region she last stood in (committed on support)
        int step = 0, steps = 0;
        std::uint32_t moves_done = 0, moves_failed = 0, replans = 0, arrivals = 0, yields = 0;
        traversal::MoveKind move = traversal::MoveKind::Walk;
        traversal::Failure last_failure = traversal::Failure::None;
    };
    [[nodiscard]] const Journey &journey() const noexcept { return travel_; }
    [[nodiscard]] const traversal::Climber *climber() const noexcept { return climber_.get(); }
    // Tests only: her body put standing at `feet`, awake and hunting (a test's
    // start, never a move in play); and a climb of hers begun where she stands.
    void place_for_test(JPH::RVec3 feet);
    // Tests only: her head and body turned to face `yaw` (a test's start: her
    // heading, never a force or a pose of her body).
    void face_for_test(float yaw) noexcept { yaw_ = yaw; look_s_ = 0.0F; }
    // Her home: the deck (one of her levels' floors, by its top) her locker
    // stands on, where she wakes, and where a restore puts her back. Moves
    // the locker; while she is in it, puts her level, floor and lead there.
    // Returns false (nothing changed)
    // if no level of hers has that floor.
    bool set_home(float deck_top);
    [[nodiscard]] float home_deck_top() const noexcept { return home_y_; }
    [[nodiscard]] int home_level() const noexcept { return home_level_; }
    // Tests only: she hears the climber at `at` (feet), as her ears would
    // report a sound there: a lead with an age, nothing more.
    void hear_for_test(JPH::RVec3 at);
    // Tests only: her hands open on the climb or the pipe she is on, as a hold
    // that gave way would open them.
    void let_go_for_test();
    // The corner pipe ruled out for her (Simulation::debug_matron_rule_out_pipe).
    void rule_out_pipe_for_test();
    // Tests only (Simulation::debug_matron_rule_out_hang): no edge of her
    // floor counts for the fatality's hang, so the floor version is met.
    void rule_out_hang_for_test() noexcept { hang_ruled_out_ = true; }
    // Tests only (Simulation::debug_matron_foresee_no_kill): every let-go of
    // the hang foreseen as a fall he would live, so she must hold on and haul
    // him back in for her floor version.
    void foresee_no_kill_for_test() noexcept { no_kill_foreseen_ = true; }
    // The fatality's hold (kFatalTargetS): its path, and the release beat.
    [[nodiscard]] FatalPath fatal_path() const noexcept { return fatal_path_; }
    // Her hold is the fatality's (her grip firm on his ankle: kFatalGripFrictionNm).
    [[nodiscard]] bool fatal_holding() const noexcept { return fatal_hold_ && holding_state(state_); }
    [[nodiscard]] std::uint32_t fatal_release_beat() const noexcept { return fatal_release_beat_; }
    // Tests only: one of her climbs begun from where she stands, up or down:
    // the corner pipe, or the scanned ladder nearest her off her floor.
    bool climb_for_test(bool pipe, bool down);

    // The hold's interface to the physics (physics_world_grab.cpp).
    [[nodiscard]] bool grab_ready() const noexcept { return grab_cooldown_s_ <= 0.0F && !grab_held_for_test_; }
    void begin_grab(JPH::RVec3 player);
    [[nodiscard]] bool holding() const noexcept { return holding_state(state_) && body_in_world_ && !dead_; }
    [[nodiscard]] bool reaching() const noexcept { return state_ == State::Grab && !grip_on_; }
    // Knocked off her feet: her body's capsule stands over where she lies.
    [[nodiscard]] bool down() const noexcept { return state_ == State::Down; }
    [[nodiscard]] JPH::RVec3 hand_target() const noexcept { return hand_target_; }
    [[nodiscard]] JPH::Vec3 hand_target_velocity() const noexcept { return hand_target_v_; }
    // Her right shoulder (the ankle hand's) and left (her second hand's),
    // from her middle, where her lean and crouch have them now.
    [[nodiscard]] JPH::Vec3 shoulder_local() const noexcept { return shoulder_at(lean_, crouch_, 1.0F); }
    [[nodiscard]] JPH::Vec3 shoulder2_local() const noexcept { return shoulder_at(lean_, crouch_, -1.0F); }
    [[nodiscard]] float hold_lean() const noexcept { return lean_; }
    [[nodiscard]] bool grip_free() const noexcept { return grip_free_ && holding(); }
    [[nodiscard]] float hold_crouch() const noexcept { return crouch_; }
    // Her wrist steadies him head down (not while her other hand has his arm).
    [[nodiscard]] bool wrist_steadies() const noexcept { return holding() && state_ != State::ArmPlay; }
    [[nodiscard]] float hand_pull_n() const noexcept { return pull_n_; }
    // ArmPlay out over the rail, his arm not yet in her other hand after a
    // re-pose for it (no step back there): she hauls him in to her, her
    // weight in it, her pull rising as against a braced body.
    [[nodiscard]] bool hauled_in() const noexcept {
        return state_ == State::ArmPlay && hold_phase_ == 0 && over_drop_ && armplay_reposes_ > 0;
    }
    void set_player_braced(const bool braced) noexcept { player_braced_ = braced; }
    void set_thrash(const float thrash) noexcept { thrash_ = thrash; }
    void grip_taken() noexcept;
    // Her grip gone: `torn` out of her hand (else let go); `fought` when the
    // player fought it this hold (kicked, thrashed, braced on a rail). Torn
    // with no fight (the world's doing: her heave against the rail, a snag),
    // she takes them again where they land and the arm comes off there.
    void grip_lost(bool torn, bool fought = false) noexcept;
    // Where the player's ankle is (the feet end of the body, however it
    // lies), given before pre_step: her hand goes there.
    void set_player_ankle(const JPH::RVec3 ankle) noexcept { player_ankle_ = ankle; }
    // Where his head faces (the sim's facing and look pitch; zero when he
    // faces nowhere): read only through her eyes (Perception: watched).
    void set_player_look(const JPH::Vec3 look) noexcept { extras_.look(look); }
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
    // She grips it hard in the rip, and in the fatality's hold (only his blow
    // frees him from it: the world's snags on her carry and her heave do not).
    [[nodiscard]] float grip_strain_floor_n() const noexcept {
        return state_ == State::ArmPlay || fatal_hold_ ? kGripRipFloorN : kGripStrainFloorN;
    }
    void set_player_body(const JPH::RVec3 wrist, const int arms_left) noexcept {
        player_wrist_ = wrist;
        player_arms_left_ = arms_left;
    }
    void wrist_taken() noexcept;
    void arm_torn() noexcept;
    // She re-posed (arm_repose): the world lets go of the wrist in her hand,
    // if any, and her hand goes for the nearer whole arm again. Once.
    [[nodiscard]] bool take_wrist_reseat() noexcept {
        if (!wrist_reseat_) {
            return false;
        }
        wrist_reseat_ = false;
        return true;
    }
    // His own blow landed on her (the player's punch): the world tags it as
    // it lands, and take_blow reads it with the blow it is part of. Only such
    // a blow, before the rip begins, takes her out of her hold (State).
    void player_blow_landed() noexcept { player_blow_tag_ = true; }
    // His blow can still free him: the body in her hand, the rip not begun
    // (State). His punch's offer in her hand (physics_world_strike.cpp).
    [[nodiscard]] bool his_blow_can_free() const noexcept { return body_in_hold() && !rip_begun(); }
    [[nodiscard]] bool take_arm_throw(JPH::Vec3 &velocity) noexcept {
        if (!arm_throw_) {
            return false;
        }
        arm_throw_ = false;
        velocity = arm_throw_v_;
        return true;
    }
    // 0: no ArmPlay; 2: ArmPlay whose pull never tears the arm (the re-pose
    // cap's test); 3: as 2, her re-pose cap reached at her first re-pose
    // (the deck-only set-down begun where the arm play began, out over the
    // drop); otherwise as in play.
    void set_armplay_for_test(const int armplay) noexcept { armplay_for_test_ = armplay; }
    [[nodiscard]] bool claimed() const noexcept { return state_ == State::Claimed; }
    // The body has no arm left and is not hers yet (a grip torn after the
    // tear, a blow that took her mid-hold): she claims it now, or once she is
    // back on her feet. False when she cannot (dead, or out of the world):
    // the world ends the run all the same. No body goes on armless.
    bool claim_armless() noexcept;

    // ---- Claimed: the fatality's floor version (matron_fatality.cpp) -------
    // Her beats. Each is a physical act on kit and body: nothing in it moves
    // the player but her hands, her knee, the rope and gravity. The floor
    // version (no edge in reach, kFatalFloorS) plays Lay, Pin, Wipe, Black;
    // the beats between (the legs, the well's rope) are no longer begun
    // (kept for their numbering and their physics).
    enum class Fatal : std::uint8_t {
        Lay = 0,    // the body let down on the steel where she stands, by its ankle
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
    // The floor version's beats, about kFatalFloorS in all: her fingers open
    // kFloorLaySeconds into the lowering, her knee on the chest
    // kFloorPinSeconds, the wipe, black. A beat the world will not let her
    // finish (a knee that cannot reach, a body that will not settle) she
    // leaves after kFloorBeatGiveUpS.
    static constexpr float kFloorLaySeconds = 0.8F;
    static constexpr float kFloorPinSeconds = 0.5F;
    static constexpr float kFloorBeatGiveUpS = 1.5F;
    static constexpr float kWipeSeconds = 0.7F;
    static constexpr float kBlackSeconds = 0.5F;
    static constexpr float kFatalGiveUpSeconds = 20.0F;   // a beat that the world will not let finish
    // Where the well's beats were done (the Counterweight Well, stage A, on
    // the 154 deck): no longer begun.
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
    // Tests only: her hands held off the player for `seconds` (no grab, no
    // shove), so a test can meet her body alone.
    void hold_hands_for_test(const float seconds) noexcept {
        shove_cooldown_s_ = seconds;
        grab_cooldown_s_ = seconds;
    }
    // Tests only: her hands never take hold (a grab's cooldown that does not
    // end, a death's restore too): a touch is her shove, not a catch.
    void hold_grab_for_test(const bool held) noexcept { grab_held_for_test_ = held; }
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
    // A climb (matron_climb.cpp): she hangs on the line (x, z), standoff
    // short of its holds, between the support at its foot and the deck at its
    // head; the steel she goes over at the top is lip_y (a bridge or a fascia
    // can stand above the deck it leads to). out is the way over the top;
    // top is where she stands up there, and where she starts a climb down.
    struct Climb {
        float x, z;
        float foot_y, head_y;
        float out_x, out_z;
        float top_x, top_z;
        bool hers;
        float standoff;
        float lip_y;
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
    // Her sensory frame (kEyeAboveCentreM above): where her eyes are and
    // where her head faces. Closed out of the world, dead, or knocked about.
    [[nodiscard]] Eyes eyes() const;
    // One look from her eyes at `target`, out to `range`: the cone, then a
    // real line through the world's solids.
    [[nodiscard]] Glimpse look_at(JPH::RVec3 target, float range) const;
    // The stalk (matron_stalk.cpp).
    [[nodiscard]] bool stalk_encounter() const noexcept {
        return noticed_ && !stalk_spent_ && stage_ == Stage::Pest;
    }
    void end_encounter() noexcept;
    // One hunt tick of the stalk, her way (wx, wz) already chosen: true when
    // it had her feet this tick.
    bool stalk_step(float dt, float ax, float az, float feet_y, float wx, float wz);
    // Him seen on a ladder off her floor (an edge of her scan): his climb
    // rate as she saw it, and her way round to its head.
    void watch_climber(int edge, float player_y, float dt);
    bool start_ambush(int edge, JPH::RVec3 at, float player_y);
    // Her ambush's tick, from arrival on the head's floor: to its head, and
    // waiting there. True when it had her feet this tick.
    bool ambush_step(float dt, float ax, float az, float feet_y, bool saw);
    // How far a sound at `from` has to carry to an ear at `to` (above); and
    // the decks (static floors) the straight line between passes through.
    [[nodiscard]] float carry_m(JPH::RVec3 from, JPH::RVec3 to) const;
    [[nodiscard]] float muffled_distance(JPH::RVec3 from, JPH::RVec3 to) const;
    // The floor of the level a sound at height y is on (the highest under
    // it, within a story), or her own when it is on none.
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
    // way_clear, and steel under her feet all along, within a step of hers.
    [[nodiscard]] bool straight_to(float fx, float fz, float tx, float tz) const;
    // Her feet toward (wx, wz) at `speed`; `turn_moves` lets her vault and leap.
    void walk_toward(float dt, float wx, float wz, float speed, float foot_accel = kFootAccel,
                     bool turn_moves = false);
    // matron_moves.cpp: her pace chosen for this tick of the hunt; a crumb of
    // the player's way; the next crumb on from where she is; the stall
    // watchdog's tick (true: a sidestep target set); silence for silence.
    Pace choose_pace(float dt, float distance, bool saw, bool heard, float loudness);
    void drop_crumb(float px, float pz, float floor_y);
    bool crumb_ahead(float ax, float az, float &wx, float &wz);
    void watch_stall(float dt, JPH::RVec3 at, float heading_x, float heading_z, bool wants, float to_x = 0.0F,
                     float to_z = 0.0F, bool hold = false);
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
    bool follow_line(float dt, JPH::RVec3 at, bool wait_ok);
    // The player is on a line, and a journey or a climb owns her body this
    // tick (those return before the hunt looks). She remembers which line,
    // and where it lets off. She does not move for it, and the Turn's clock
    // stays where it is until her feet are hers again.
    void notice_ridden_line();
    // The line follow_line goes to from here (-1 for none), and whether she is on its ramp.
    [[nodiscard]] int line_to_take(float ax, float az) const;
    [[nodiscard]] bool on_line_ramp(JPH::RVec3 at) const;
    // On a line's cable: her drag on it, and letting go at its end.
    void ride_line();
    // After a lead on a level above, up a climb of hers toward it (to it, or
    // to a level between): to its foot, and in the Turn onto it. False when no
    // climb does.
    bool follow_climb(float dt, JPH::RVec3 at);
    // The corner pipe's way from one deck to the other (0..3; -1 for none),
    // and whether it may be tried now (pipe_ways_).
    [[nodiscard]] static int pipe_way(float from_floor, float to_floor) noexcept;
    [[nodiscard]] bool pipe_way_open(int way) const noexcept;
    // Her own way between levels, found in the world, no table
    // (matron_found_climb.cpp): a column of level bars from her floor toward
    // her lead's, real floor at its foot and head, climbed by routecraft.
    struct FoundClimb {
        bool active = false;
        std::int64_t key = 0;
        float start_x = 0.0F;
        float start_z = 0.0F;
        float from_y = 0.0F;   // the floor she starts from
        JPH::RVec3 goal = JPH::RVec3::sZero();
        JPH::Vec3 facing = JPH::Vec3::sAxisZ();
        int walk_ticks = 0;    // walking to its start
    };
    static constexpr int kFoundWalkTicks = 30 * 90;   // a start not reached in 30 s is not a way
    struct ClimbBlock {
        std::int64_t key = 0;
        int ticks = 0;
        bool failed = false;   // it failed under her (not a search's refusal)
    };
    static constexpr int kFoundClimbBlockTicks = 30 * 90;   // a refused or failed column left alone 30 s
    bool follow_found_climb(float dt, JPH::RVec3 at);
    [[nodiscard]] bool find_climb(JPH::RVec3 at, FoundClimb &out) const;
    [[nodiscard]] bool found_floor(float x, float z, float from_y, float to_y, float &floor_y) const;
    [[nodiscard]] bool climb_blocked(std::int64_t key) const;
    void block_climb(std::int64_t key, bool failed = true);
    void block_climb_at(float x, float z);   // the column standing at (x, z)
    // A column at or beside (x, z) that failed under her: failed for every
    // way of hers that takes it, her map's included (a refusal by her own
    // search is not: the bar climber refuses a column the player is on).
    [[nodiscard]] bool column_blocked_near(float x, float z) const;
    // A way she plans herself where no bar column serves (a pole, a lip, a
    // run of moves): the traversal scan of the steel round her and her lead
    // (src/sim/matron_traversal.*), its moves done by its Climber out of one
    // capped budget; while it runs it has her body.
    bool start_journey(JPH::RVec3 at);
    void step_journey(float dt);
    void end_journey(bool failed);
    [[nodiscard]] bool journeying() const noexcept { return journey_active_; }
    // On a climb her scan found (matron_climb.cpp), by her Climber's moves:
    // onto it (false: none to take, or her hands are on another move), its
    // tick, and off it (failed: her hands opened on it).
    bool take_climb(int index, bool down = false);
    void step_climb(float dt);
    void release_climb(bool failed);
    // After a lead a story away along the corner pipe (above): to the rail's
    // corner on her level and onto it. False when it does not go that way, or
    // she is frail.
    bool follow_pipe(float dt, JPH::RVec3 at);
    [[nodiscard]] static bool pipe_reaches(float floor_y) noexcept;
    // Her hands on the rail of the deck she is leaving: false (and nothing
    // started) when there is no rail top in her reach.
    bool get_on_pipe(float from_floor, float to_floor);
    void ride_pipe(float dt);
    // Off it: her feet on the deck's steel, or (failed) her hands gone.
    void get_off_pipe(bool failed = false);
    // A blow on the pipe past kPipeTearMps: her hands open, her legs slide.
    void tear_off_pipe();
    // Struck off the pipe by such a blow (take_blow): at the riser she
    // slides; still at a rail, she goes down onto the steel her feet find.
    void pipe_struck(JPH::RVec3 at);
public:
    // The player's fist on her (physics_world_strike.cpp): the impulse it
    // gave her body, read as a blow in take_blow against her balance like
    // any striker's (the force itself is already on her body).
    void struck_by_player(const JPH::Vec3 impulse_ns, const JPH::RVec3 fist, const bool low) noexcept {
        player_blow_ns_ += impulse_ns;
        player_blow_at_ = fist;
        player_blow_low_ = player_blow_low_ || low;
        // Tagged as his own: before the rip begins it frees him (take_blow).
        player_blow_landed();
    }
    void clear_player_blow() noexcept {
        player_blow_ns_ = JPH::Vec3::sZero();
        player_blow_low_ = false;
    }
private:
    JPH::Vec3 player_blow_ns_ = JPH::Vec3::sZero();
    JPH::RVec3 player_blow_at_ = JPH::RVec3::sZero();
    bool player_blow_low_ = false;
    // Her report of the pipe and of her cables (refresh_view), and both set
    // back for a restore (matron_pipe.cpp, matron_line.cpp).
    void report_pipe();
    void report_line();
    void restore_pipe() noexcept;
    void restore_line() noexcept;

    // Her ways between floors under one contract (matron_mover.hpp), each
    // wrapping one executor of hers and defined in that executor's file.
    // Each way's state is its own: the journey's journey_*, the routecraft's
    // routecraft_* with the column it found (found_, find_wait_ticks_,
    // climb_back_*, climbing_key_), the travel's travel_*, the pipe's pipe_*,
    // the rung way's climb_ (its move is her Climber's), the cable's on_line_. What they
    // share by design is hers, no one way's: climb_retry_s_ (a start refused
    // or a way failed: none started a while), climb_blocks_ and
    // journey_wait_ticks_ (her own ways' search clocks), climb_releases_ and
    // climbs_done_ (her report's counts), climber_ and scanned_ (her Climber
    // and her scan), riding_ and let_go_s_ (her cling), chase_way_held_.
    class Way : public Mover {
    public:
        explicit Way(Matron &m) noexcept : m_(m) {}
        bool begin(const MoverGoal &goal) override {
            goal_ = goal;
            return true;
        }

    protected:
        Matron &m_;
        MoverGoal goal_{};
    };
    // Her journey over her scan (matron_found_climb.cpp): its Climber's moves.
    class JourneyWay final : public Way {
    public:
        using Way::Way;
        bool begin(const MoverGoal &goal) override;   // a plan over the steel round her and her lead
        MoverStatus step(float dt, traversal::Budget &budget) override;
        void cancel() override;
        [[nodiscard]] MoverStatus status() const override;
        [[nodiscard]] MoverView view() const override;
        // Her hands open on a move, as a hold giving way opens them (false:
        // no move of it under way).
        bool let_go();
    };
    // Routecraft's bars, with the column she found for them and the walk to
    // its foot (matron_found_climb.cpp).
    class RoutecraftWay final : public Way {
    public:
        using Way::Way;
        MoverStatus step(float dt, traversal::Budget &budget) override;
        void cancel() override;
        [[nodiscard]] MoverStatus status() const override;
        [[nodiscard]] MoverView view() const override;
        // Not asked this tick: her search clocks still run, and the column
        // she was walking to is forgotten.
        void stand_aside();
        bool let_go();
    };
    // Her travel across the tower (matron_journey.cpp).
    class TravelWay final : public Way {
    public:
        using Way::Way;
        bool begin(const MoverGoal &goal) override;   // travel_to the lead's place
        MoverStatus step(float dt, traversal::Budget &budget) override;
        void cancel() override;
        [[nodiscard]] MoverStatus status() const override;
        [[nodiscard]] MoverView view() const override;
    };
    // The corner pipe (matron_pipe.cpp).
    class PipeWay final : public Way {
    public:
        using Way::Way;
        MoverStatus step(float dt, traversal::Budget &budget) override;
        void cancel() override;
        [[nodiscard]] MoverStatus status() const override;
        [[nodiscard]] MoverView view() const override;
        bool let_go();
    };
    // Her rung way in the hunt (matron_climb.cpp): a ladder of her level's
    // (the bay's rungs, a climb a test names), by her Climber's moves.
    class ClimbWay final : public Way {
    public:
        using Way::Way;
        MoverStatus step(float dt, traversal::Budget &budget) override;
        void cancel() override;
        [[nodiscard]] MoverStatus status() const override;
        [[nodiscard]] MoverView view() const override;
        bool let_go();
    };
    // A cable down toward a lead below (matron_line.cpp).
    class LineWay final : public Way {
    public:
        using Way::Way;
        MoverStatus step(float dt, traversal::Budget &budget) override;
        void cancel() override;
        [[nodiscard]] MoverStatus status() const override;
        [[nodiscard]] MoverView view() const override;
    };
    // The way that has her body (its status Running), first in the order her
    // tick steps them; nullptr when her feet are the hunt's.
    [[nodiscard]] Mover *holder() noexcept;
    [[nodiscard]] const Mover *holder() const noexcept;
    // Held on the steel or cable of a way the hunt steps (the corner pipe, a
    // climb, a cable).
    [[nodiscard]] bool held_in_hunt() const noexcept;
    // The planner, her follow chain made one call: her next way toward a lead
    // on another floor, each asked in her order. True: one took the tick.
    bool take_way(float dt, JPH::RVec3 at, traversal::Budget &budget);
    // A hold for her hands (matron_pipe.cpp): the rail's top over `deck`
    // beside her, or the riser's face at height y on her side of it.
    bool pipe_rail_hold(float deck, JPH::RVec3 at);
    bool pipe_face_hold(JPH::RVec3 at, float y);
    // Down on the steel from a blow (take_blow), the way (fall_x_, fall_z_).
    void knock_down();
    // A lead: a place she heard (or saw) the player, or a crash, on the level
    // whose floor is `floor`, at that loudness.
    void take_lead(float x, float z, float floor, float loudness, Sense sense);
    // Back to her post if there is a way out to guard, else the patrol.
    void stand_down();
    void step_hold(float dt, JPH::RVec3 player);
    void end_hold(State next);
    void finish_dangle();
    bool begin_armplay(State then);
    void stand_in_for_rim(JPH::RVec3 at, JPH::RVec3 player, JPH::Vec3 fwd) noexcept;
    // Set down on her deck beside her where she stands (Spare from its
    // lowering): an ordinary hold's end with no rail, or carried too long.
    void spare_here() noexcept;
    void claim() noexcept;
    void retake() noexcept;
    // The fatality (kFatalTargetS): its way chosen once his body is up in her
    // fist (to the edge, else the floor); her fingers opening over the drop
    // (the release beat), or the body lost over it; his blow out over the
    // drop (reel), and the set-down that ends it; the time a hold's end keeps
    // her hands off him.
    void begin_fatal(float ax, float az, float feet);
    void fatal_floor() noexcept;
    void fatal_release() noexcept;
    // His own blow landed on her in her hold before the rip (his punch's tag,
    // player_blow_landed): her hand opens on it, however small the blow (a
    // flinch, no stagger needed). Over her deck, at once; out over the drop
    // (or anywhere her deck is not under him), she reels back and sets him
    // down on it first (reel). True when it freed him (or has begun to).
    bool his_blow();
    [[nodiscard]] bool over_her_deck(JPH::RVec3 player, float feet) const;
    void begin_reel(JPH::RVec3 at, JPH::RVec3 player);
    void end_reel(JPH::RVec3 player);
    [[nodiscard]] float hands_off_after_hold() const noexcept {
        return arm_tears_ != hold_tears_at_ ? kRegrabAfterTearS : kAfterReleaseSeconds;
    }
    bool choose_fatal_edge(float ax, float az, float feet);
    // The speed down at which a body at `com` (rotation `rot`, moving at `v`,
    // spinning at `w`) meets steel, falling free from there: its capsule
    // grown by `margin` swept along its path. -1 when it meets none (an
    // endless fall is no death).
    [[nodiscard]] float fall_impact_mps(JPH::RVec3 com, JPH::Quat rot, JPH::Vec3 v, JPH::Vec3 w, float margin) const;
    // An edge spot: her stand (sx, sz), out over the drop (nx, nz). Room for
    // her and to hang the body there (and its way over the edge); and a
    // fall from where he would really hang that kills.
    [[nodiscard]] bool hang_room(float sx, float sz, float nx, float nz, float feet) const;
    [[nodiscard]] bool lethal_from(float sx, float sz, float nx, float nz, float feet) const;
    // The let-go not lethal: her stand a step along the edge toward one that
    // is (false: none within kFatalShiftMaxM).
    bool fatal_shift(float feet);
    // Her deck close under his body (his breadth swept down from his middle):
    // where any set-down lets go of him.
    [[nodiscard]] bool deck_close_below(JPH::RVec3 player, float feet) const;
    // A straight walk from (ax, az) to (x, z) on her floor (its top within a
    // step of `feet` all the way, nothing built across it at her hips).
    [[nodiscard]] bool straight_on_floor(float ax, float az, float x, float z, float feet) const;
    void arm_repose(JPH::RVec3 at, JPH::RVec3 player, bool reseat);
    [[nodiscard]] JPH::Vec3 spare_side(JPH::RVec3 at, float feet, JPH::Vec3 right, JPH::Vec3 out, bool &deck) const;
    // The body in her hand, or hers already: from her fingers closing on the
    // ankle till she lets go, and the whole of the fatality.
    [[nodiscard]] bool body_in_hold() const noexcept {
        return state_ == State::Claimed || (holding_state(state_) && grip_on_);
    }
    // The rip begun: her other hand on its way to his arm (ArmPlay), an arm
    // off already this hold, or the fatality.
    [[nodiscard]] bool rip_begun() const noexcept {
        return state_ == State::ArmPlay || state_ == State::Claimed || armplay_done_ || arm_tears_ != hold_tears_at_;
    }
    void step_armplay(float dt, JPH::RVec3 player, JPH::RVec3 at, float feet);
    void step_claimed(float dt, JPH::RVec3 at, float feet);
    void fatal_next(int beat) noexcept;
    void fatal_work_reset() noexcept;
    [[nodiscard]] bool off_its_steel(int level, float x, float feet, float z) const;
    bool choose_rim(float ax, float az);
    [[nodiscard]] bool braced_at_rim(float ax, float az) const noexcept;
    void pose_hand(float dt, JPH::RVec3 target, float speed, JPH::Vec3 carried = JPH::Vec3::sZero());
    // Her reach (kArmReachM from a shoulder, her trunk leant `lean` from the
    // hips and her hips down `crouch`): the shoulder from her middle; the
    // least lean and crouch that bring `want` (in the world) into the reach
    // of the `side` shoulder (else the nearest they come); how far `want`
    // is from that shoulder; `want` brought into the reach of the shoulder
    // as her body has it now.
    [[nodiscard]] JPH::Vec3 shoulder_at(float lean, float crouch, float side) const noexcept;
    void posture_for(JPH::RVec3 want, float side, float &lean, float &crouch) const noexcept;
    [[nodiscard]] float reach_to(JPH::RVec3 want, float side = 1.0F) const noexcept;
    [[nodiscard]] JPH::RVec3 in_reach(JPH::RVec3 want, float side = 1.0F) const noexcept;
    // Her lean and crouch toward what her hand's pose (and, `both`, her
    // other hand's reach) wants, at their pace.
    void step_posture(float dt, JPH::RVec3 want, bool both) noexcept;
    // He hangs head down in her fist (within acos(cos_down) of it).
    [[nodiscard]] bool bodies_hung(float cos_down = kFatalHeadDownCos) const noexcept;
    // His shoulder's distance from her body (her at `at`, his middle at
    // `player`), and so as far as his swing (`swing_v`) carries it: what his
    // punch must reach (kFatalReachM).
    [[nodiscard]] float fist_reach_m(JPH::RVec3 at, JPH::RVec3 player, float swing_v) const;
    // His fall foreseen from his middle at `player` as he moves, with the
    // least of her tosses out along `out` that kills: fatal_lethal_,
    // fatal_toss_ and fatal_lethal_mps_.
    void foresee_fall(JPH::RVec3 player, JPH::Vec3 out);
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
    // Light stock of her deck she sees out of the place it was laid: moved,
    // and the player's to her (handled_).
    void notice_moved_stock();
    static constexpr float kMovedStockM = 1.5F;
    bool fetch_stock(float dt, float ax, float az, bool allow_catwalk);
    bool throw_stock(float dt, JPH::RVec3 at, JPH::RVec3 player);
    // True when her hands took it: never a load another constraint already
    // holds (his carry), which her hands feel held fast (load_held_elsewhere).
    bool take_stock(int index);
    [[nodiscard]] bool load_held_elsewhere(JPH::BodyID body) const;
    void hold_stock(float dt);
    void release_stock(JPH::Vec3 velocity, JPH::Vec3 spin, bool thrown);
    [[nodiscard]] JPH::Vec3 hands_offset() const noexcept;
    void step_planks(float dt);
    void restore_planks();
    void refresh_view();
    // The contact facts of view_ (contact_*), from the movers' grips and her
    // floor as walk_toward finds it (matron_contact.cpp). Read-only.
    void refresh_contact_view();
    // Her balance (above): the moving bodies near her and the velocity her
    // feet set, taken after she moves (sense_near); the blow they deal her,
    // read after the step (take_blow); and her staggering and lying there
    // (step_knocked), down onto the steel and up off it.
    void sense_near();
    void take_blow(float dt);
    void step_knocked(float dt);
    void lie_down(float fall_x, float fall_z);
    void stand_up();
    [[nodiscard]] bool room_to_stand(JPH::RVec3 at) const;
    [[nodiscard]] bool shape_fits(const JPH::Shape *shape, JPH::RVec3 at) const;
    // Off an edge she will not go, staggering or sliding on the steel: her
    // feet and hands take her at it (no floor within a step's drop ahead).
    void brace_at_edges(float dt);
    // Her sole contact under her (center first, then her feet's span): a real
    // floor, not a sensor, not the player, facing up. False when there is none.
    struct Sole {
        JPH::BodyID body;
        JPH::SubShapeID sub;
        JPH::RVec3 point = JPH::RVec3::sZero();
        JPH::Vec3 local = JPH::Vec3::sZero();
        JPH::Vec3 normal = JPH::Vec3::sAxisY();
        JPH::Vec3 velocity = JPH::Vec3::sZero();
    };
    bool sole_contact(JPH::RVec3 at, JPH::Vec3 velocity, float yaw, Sole &sole) const;
    // A push of her feet against her sole contact: the force she wants,
    // granted from this tick's feet_budget_, the equal and opposite force on
    // what she stands on (if it moves), and reported like her stride. Nothing
    // under her, no push. Returns what was granted.
    JPH::Vec3 feet_push(float dt, JPH::Vec3 want_force);
    [[nodiscard]] float step_reach() const noexcept;
    [[nodiscard]] float recover_accel() const noexcept;
    [[nodiscard]] float down_seconds() const noexcept;

    MatronRoutecraft routecraft_;
    bool routecraft_recovering_ = false;
    bool routecraft_mine_ = false;            // a climb she started herself (released at its goal)
    JPH::RVec3 climb_back_ = JPH::RVec3::sZero();   // where her own climb set out from
    JPH::Vec3 climb_back_facing_ = JPH::Vec3::sAxisZ();
    bool climb_back_tried_ = false;           // ended short, she went back the way she came once
    bool climb_hand_known_ = false;           // climb_hand_ is this climb's
    std::int64_t climbing_key_ = 0;
    FoundClimb found_;
    std::array<ClimbBlock, 8> climb_blocks_{};
    int find_wait_ticks_ = 0;
    std::unique_ptr<traversal::Climber> climber_;
    std::unique_ptr<traversal::AffordanceMap> own_scan_;   // her own read of a world no one read for her
    // Her whole-tower travel (travel_to, matron_journey.cpp).
    const traversal::AffordanceMap *scanned_ = nullptr;
    // The chase-time scan plan, cached by its two ends (matron.cpp): a plan
    // over her scanned map from where she stands to her lead's floor.
    const traversal::AffordanceMap *chase_plan_map_ = nullptr;
    int chase_plan_from_ = -1;
    int chase_plan_to_ = -1;
    bool chase_plan_ok_ = false;
    // Held once a chase has had a way (matron.cpp): arriving on her lead's
    // floor ends the ways, not the chase, so the extended window survives
    // the arrival tick instead of dumping her into a search at the spot.
    bool chase_way_held_ = false;
    Journey travel_;
    std::vector<int> travel_edges_;
    std::vector<int> travel_avoid_;   // edges this journey found it could not start
    std::vector<JPH::Vec3> travel_walk_;
    std::size_t travel_walk_i_ = 0;
    float travel_wp_best_ = 1.0e9F;
    float travel_wp_stall_s_ = 0.0F;
    bool travel_rerouted_ = false;
    int travel_stuck_ = 0;   // fresh searches of her way in a row with no waypoint passed
    JPH::Vec3 travel_still_at_ = JPH::Vec3::sZero();   // where she last moved from on a walk
    float travel_still_s_ = 0.0F;
    float travel_yield_s_ = 0.0F;   // how long she has given way to the player at this move
    static constexpr float kYieldGiveUpS = 12.0F;
    std::uint32_t travel_replans0_ = 0;
    float travel_wait_s_ = 0.0F;
    static constexpr float kYieldReachM = 1.3F;
    // How old a sighting, a sound or a touch of him on her climb may be and
    // still make her give way (evidence with an age).
    static constexpr float kYieldMemoryS = 0.5F;
    static constexpr float kYieldClearM = 1.6F;
    void step_travel(float dt);
    [[nodiscard]] int region_under() const;
    std::vector<traversal::Edge> journey_;
    std::vector<JPH::Vec3> journey_legs_;
    std::size_t journey_at_ = 0;
    std::size_t journey_leg_ = 0;
    // Where the current leg walk started (for the segment below).
    bool journey_active_ = false;
    bool journey_moving_ = false;   // a move (not a walk) under way
    int journey_leg_ticks_ = 0;
    bool journey_retreating_ = false;   // walking back along reached legs after a leg timed out
    int journey_wait_ticks_ = 0;    // a scan at most every 2 s; 30 s after a failure
    float routecraft_release_floor_ = 0;
    JPH::PhysicsSystem &physics_;
    std::uint32_t static_layer_;
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
    // Her home (set_home): its floor and its level.
    float home_y_ = kHomeDeckTop;
    int home_level_ = 0;
    int patrol_ = 0;   // which of her level's loops, and how far round it

    State state_ = State::Dormant;
    float strain_s_ = 0.0F;
    float emerge_s_ = 0.0F;
    float since_heard_s_ = 0.0F; // absolute evidence age, including time sealed/emerging
    float pursuit_s_ = 0.0F; // time available to act on that evidence after emerging
    float search_s_ = 0.0F;          // at the place she searches
    float search_budget_s_ = 1.0e9F;     // her way there may take this long (set as she sets out)
    float search_trip_s_ = 0.0F;    // on her way there so far
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
    float pull_n_ = kHandReachPullN;
    float lean_ = 0.0F;     // her trunk leant from the hips, rad
    bool fatal_heave_back_ = false;   // her let-go's heave: back in (else out)
    float pose_reach_ = kPoseReachM;   // how far from her shoulder her hand's poses go now
    float crouch_ = 0.0F;   // her hips down, m
    bool hand2_leads_ = false;
    int tip_step_ = 0;          // over the rail: 0 onto it, 1 out over it
    bool over_drop_ = false;    // tipped out over the rail, his middle past the side
    bool grip_free_ = false;    // he turns free about her fingers (tipped across the rail)   // her second hand's reach leads her lean (the wrist)
    JPH::RVec3 hand2_want_ = JPH::RVec3::sZero();
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
    bool grab_held_for_test_ = false;
    bool armplay_done_ = false;   // this hold's ArmPlay is over
    State armplay_then_ = State::Dangle;   // what follows it: the dangle's end, a Spare, or the carry's drop
    bool retake_ = false;                  // her hand on them again after a grip the world tore: the arm here
    int hold_retakes_ = 0;                 // this hold's re-takes (kMaxRetakes)
    int armplay_reposes_ = 0;              // this ArmPlay's re-poses (the way back she steps turns with each)
    bool wrist_reseat_ = false;            // re-posed: the world lets go of the wrist (take_wrist_reseat)
    JPH::Vec3 spare_side_ = JPH::Vec3::sZero();   // Spare: where beside her she sets them down (spare_side)
    bool spare_side_set_ = false;
    bool spare_deck_ = false;              // her deck under it
    int spare_tries_ = 0;                  // set-downs that ran out of time, this hold
    std::uint32_t hold_tears_at_ = 0;      // arm_tears_ as this hold began
    JPH::RVec3 player_ankle_ = JPH::RVec3::sZero();
    // The fatality's hold (kFatalTargetS); a restore clears all but the counts.
    bool fatal_hold_ = false;              // this catch is the second: his one arm left
    FatalPath fatal_path_ = FatalPath::None;
    float fatal_carry_s_ = 0.0F;           // to the edge so far
    float fatal_window_s_ = 0.0F;          // into the hang's window
    bool fatal_hang_ = false;              // in the window
    JPH::RVec3 fatal_edge_ = JPH::RVec3::sZero();   // the lip she holds him out over
    bool hang_ruled_out_ = false;          // tests: no edge counts (rule_out_hang_for_test)
    bool no_kill_foreseen_ = false;        // tests: no let-go foreseen to kill (foresee_no_kill_for_test)
    bool reel_ = false;                    // his blow out over the drop: reeling back, him in her fist
    bool reel_to_floor_ = false;           // no lethal let-go in time: hauled back in for the floor version
    bool fatal_dropped_ = false;           // let go over the drop (or lost there): she watches him go
    float fatal_s_ = 0.0F;                 // since her fingers closed on his ankle (the fatality's hold)
    int fatal_check_ticks_ = 0;            // till the let-go is foreseen again
    bool fatal_lethal_ = false;            // the let-go foreseen: it kills
    float fatal_lethal_mps_ = 0.0F;        // and the speed of its first meeting with steel
    float fatal_toss_ = 0.0F;              // her toss out with it (kFatalTossMps)
    JPH::Vec3 fatal_rim0_ = JPH::Vec3::sZero();   // her first stand at the edge (fatal_shift's bound)
    float watch_still_s_ = 0.0F;           // let go, him still this long
    float watch_y_ = 0.0F;
    bool spare_walk_ = false;              // a set-down with her deck not under him: she walks him on
    float spare_to_x_ = 0.0F, spare_to_z_ = 0.0F;
    std::uint32_t fatal_release_beat_ = 0, fatal_escapes_ = 0, reels_ = 0;
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
                  spares_ = 0, grips_torn_ = 0, retakes_ = 0, arm_reposes_ = 0, brace_losses_ = 0, carry_drops_ = 0;
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
    std::uint64_t push_load_ = 0;   // the load in his hands: a contact filter only (sense_near)
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
    // The world's checkpoint rule (freezes_checkpoint), set at the boundary
    // (Perception) and read by nothing of hers.
    bool checkpoint_freeze_ = false;
    // The perception boundary (matron_perception.hpp): what the world tells
    // her of him between ticks (hers to write, never to read), and what she
    // perceived of him, with its age. Her decisions read ev_.
    friend class Perception;
    StimulusExtras extras_;
    Evidence ev_;
    // Where this tick's evidence has him, else where she last saw, heard or
    // felt him within `max_age_s` (the newest): false when she has nothing.
    [[nodiscard]] bool knows_where(float max_age_s, JPH::RVec3 &at) const noexcept;
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
    // The bay (bay.hpp): its bracket as a level of hers, and its rungs.
    int bay_level_ = -1;
    int bay_climb_ = -1;
    [[nodiscard]] int climb_count() const noexcept;
    // The scanned ways climb_of reads: any a hand can take (a ladder, or a lip from a floor), or ladders only.
    enum class Ways : std::uint8_t { Any, Ladders };
    [[nodiscard]] bool climb_of(int edge, Climb &out, Ways ways = Ways::Any) const noexcept;
    [[nodiscard]] Climb climb_spec(int edge) const noexcept;
    [[nodiscard]] int nearest_climb(float x, float z, float floor_y, bool down) const noexcept;
    [[nodiscard]] int bay_way_out() const noexcept;
    // Her decks' steel: where a level is the floor under her (off_its_steel),
    // the footprint of its steel at its top, for the levels whose steel she
    // knows; at their height outside it she is not on them.
    struct Steel {
        float min_x = 0.0F, max_x = 0.0F, min_z = 0.0F, max_z = 0.0F;
    };
    struct DeckSteel {
        int level = -1;
        float top = 0.0F;
        std::array<Steel, 6> patches{};
        std::uint8_t count = 0;
    };
    static constexpr float kOnSteelM = 0.35F;   // feet within this of a deck's top: on it
    std::array<DeckSteel, 7> deck_steel_{};
    float clock_s_ = 0.0F;
    float fall_vy_ = 0.0F;        // her vertical speed at the end of the last step
    JPH::BodyID bay_leaf_;
    bool bay_open_ = true;
    bool bay_hooked_ = false;
    bool bay_hook_known_ = false;   // the chain on the leaf, as she last saw or felt it
    Trap trap_ = Trap::Off;
    float trap_s_ = 0.0F;        // in this phase
    float trap_lost_s_ = 0.0F;   // holding with the player gone
    std::uint32_t traps_ = 0;
    static constexpr std::uint32_t kTrapsPerRun = 3;
    static constexpr float kTrapFromM = 12.0F;          // she and the player within this of the bay, either side
    static constexpr float kTrapLostM = 25.0F;          // holding, the player farther than this from her
    static constexpr float kTrapLostSeconds = 20.0F;    // for this long: she gives it up
    // Her trap's step (true: it moved her this tick), and whether she lets go.
    // `him`: where her evidence has him this tick, read only when she saw or heard him.
    bool trap_step(float dt, JPH::RVec3 at, JPH::RVec3 him, bool saw, bool heard);
    bool trap_lets_go(JPH::RVec3 him, float dt, bool perceived);
    int guess_climb_ = -1;
    int climb_ = -1;                 // the scanned climb her rung way has her on (-1: none)
    JPH::RVec3 climb_hand_ = JPH::RVec3::sZero();   // her hand on it: the hold she has, reaches for, or last had
    float climb_force_n_ = 0.0F;     // its move's force this tick
    float climb_retry_s_ = 0.0F;
    std::uint32_t climb_grips_ = 0;
    std::uint32_t climb_releases_ = 0;
    std::uint32_t climbs_done_ = 0;
    // Her mind (see above): what she has learned this run.
    std::array<bool, 2> ways_out_{true, true};
    std::array<bool, 2> rigged_{false, false};
    float unrig_s_ = 0.0F;
    int unrig_way_ = -1;
    std::uint32_t unrigs_ = 0;
    bool lead_valid_ = false;
    mutable std::uint32_t acoustic_queries_ = 0;
    bool lead_seen_ = false;
    Sense lead_sense_ = Sense::None;
    bool glimpsed_ = false;
    // The stalk (matron_stalk.cpp).
    bool watched_ = false;
    bool noticed_ = false;
    bool stalking_ = false;
    bool stalk_spent_ = false;
    float stalk_s_ = 0.0F;
    std::uint32_t stalk_attacks_ = 0;
    struct Ambush {
        bool active = false;
        bool waiting = false;
        int edge = -1;
        float head_x = 0.0F, head_z = 0.0F, head_y = 0.0F;   // where he tops out
        float line_x = 0.0F, line_z = 0.0F;                  // the ladder
        float wait_s = 0.0F;
    };
    Ambush ambush_;
    std::uint32_t ambushes_ = 0;
    int seen_climb_ = -1;          // the ladder she last saw him on
    float seen_climb_y_ = 0.0F;    // his height on it, and when
    float seen_climb_t_ = 0.0F;
    float seen_climb_watch_s_ = 0.0F;
    float seen_climb_rate_ = -1.0F;   // m/s up it as she saw it; -1 unknown
    float ambush_weighed_t_ = -1.0F;  // the reading of his pace her way round was last weighed against
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
    // How long walk_toward has seen no support under her (below).
    // Where the current stall episode started: the rung only starts over
    // when she has walked clear of it (below), not for every sidestep
    // wiggle inside it, or the watchdog would never reach its reroute.
    float stall_episode_x_ = 0.0F, stall_episode_z_ = 0.0F;
    float longest_stall_s_ = 0.0F;
    std::uint32_t stalls_ = 0;
    std::uint32_t step_ups_ = 0;
    std::uint32_t clambers_ = 0;
    std::uint32_t sidesteps_ = 0;
    std::uint32_t reroutes_ = 0;
    bool clamber_now_ = false;   // the watchdog's first rung: clamber or vault whatever her feet feel
    // A clamber that lands her back where she started is no way: pressed
    // against a face, contact kills her forward speed as she rises and
    // she falls back to the same spot, then clambers again. Three of
    // them from one spot in short order, and she leaves it alone a
    // while, so the watchdog's sidestep and reroute can walk her round
    // it instead of bouncing her on it forever. Temporary, like a
    // failed climb's block, so a way that failed for a passing reason
    // comes back.
    float clamber_hist_x_[3] = {0.0F, 0.0F, 0.0F};
    float clamber_hist_z_[3] = {0.0F, 0.0F, 0.0F};
    float clamber_hist_t_[3] = {0.0F, 0.0F, 0.0F};
    int clamber_hist_n_ = 0;
    float clamber_refused_x_ = 0.0F, clamber_refused_z_ = 0.0F, clamber_refused_until_ = 0.0F;
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
    bool player_blow_tag_ = false;  // his blow landed, for the blow take_blow reads next (player_blow_landed)
    bool blow_by_player_ = false;   // this blow has his in it
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
    // Her pipe's four ways (pipe_way): one that failed under her is left alone
    // kPipeWayBlockS; failed more than kPipeWayRetries times, until a restore;
    // ruled out (a test), for good.
    struct PipeWayMemory {
        std::uint8_t failures = 0;
        float blocked_until_s = 0.0F;
        bool ruled_out = false;
    };
    static constexpr float kPipeWayBlockS = 30.0F;
    static constexpr std::uint8_t kPipeWayRetries = 2;
    std::array<PipeWayMemory, 4> pipe_ways_{};
    int pipe_waypoint_ = 0;
    float pipe_s_ = 0.0F;      // her clock on it (the changed jerks)
    float pipe_hold_m_ = 0.0F; // her hands' way since the last hold
    float pipe_slide_mps_ = 0.0F;
    float pipe_stop_y_ = 0.0F; // the flange top she slides to
    float pipe_stun_s_ = 0.0F;
    float pipe_listen_s_ = 0.0F; // off the pipe, still and listening
    float pipe_slam_mps_ = 0.0F;
    // Her hand on the pipe's steel: the rail's top or the riser's face, kept
    // in the support's frame.
    JPH::BodyID pipe_support_;
    JPH::SubShapeID pipe_member_;
    JPH::Vec3 pipe_local_hand_ = JPH::Vec3::sZero();
    JPH::RVec3 pipe_hand_ = JPH::RVec3::sZero();
    bool pipe_on_face_ = false;       // the hand is on the riser (friction), not a rail's top
    bool pipe_over_ = false;          // over the rail she is crossing
    float pipe_best_ = 0.0F;          // her progress along it, for the stall
    float pipe_stall_s_ = 0.0F;
    float pipe_overload_s_ = 0.0F;
    float pipe_force_n_ = 0.0F;
    float pipe_peak_force_n_ = 0.0F;   // her grip's peak force on the pipe (budget-capped)
    float pipe_peak_power_w_ = 0.0F;
    JPH::Vec3 pipe_drive_dv_ = JPH::Vec3::sZero(); // what her drive changes her velocity by this step
    // Her feet under the force law (walk_toward): a planted sole on a real
    // support member during a vault, leap, step-up, or clamber stroke, and what
    // her feet's force changes her velocity by this step (for sense_near).
    bool foot_driving_ = false;
    bool foot_prep_ = false;
    JPH::BodyID foot_body_;
    JPH::SubShapeID foot_sub_;
    JPH::Vec3 foot_local_ = JPH::Vec3::sZero();
    float foot_start_y_ = 0.0F;
    float foot_stroke_m_ = 0.40F;
    float foot_prep_dist_ = 0.0F;
    JPH::RVec3 foot_obstacle_ = JPH::RVec3::sZero();
    JPH::Vec3 foot_heading_ = JPH::Vec3::sAxisZ();
    float foot_want_vx_ = 0.0F;
    float foot_want_vy_ = 0.0F;
    int foot_ticks_ = 0;
    float foot_force_n_ = 0.0F;
    float foot_power_w_ = 0.0F;
    bool foot_lunge_ = false;
    JPH::Vec3 foot_drive_dv_ = JPH::Vec3::sZero();
    // Her feet's one account for the tick (reset in pre_step): her stride,
    // her leg drives, her boarding push, her stagger's catch, her brace and
    // her crawl all draw from it, so together they stay within her Budget.
    traversal::Budget feet_budget_{};
    bool feet_budget_lunge_ = false;   // the Turn's lunge has raised it to its own caps this tick
    // A standing jump asked of her legs (follow_line): the take-off speed it
    // wants, driven through her planted foot by walk_toward, 0 for none.
    float leg_jump_vy_ = 0.0F;
    // The loose piece her pipe swat is pushing out of her way (a force over
    // the ticks her arm is on it, from her pipe's budget).
    JPH::BodyID pipe_swat_body_;
    std::uint32_t pipe_links_ = 0;
    std::uint32_t pipe_holds_ = 0;
    std::uint32_t pipe_flanges_ = 0;
    std::uint32_t pipe_slams_ = 0;
    View view_{};
    // Her ways (above), in the order her tick steps them: holder's order.
    JourneyWay journey_way_{*this};
    RoutecraftWay routecraft_way_{*this};
    TravelWay travel_way_{*this};
    PipeWay pipe_way_{*this};
    ClimbWay climb_way_{*this};
    LineWay line_way_{*this};
    std::array<Mover *, 6> ways_{&journey_way_, &routecraft_way_, &travel_way_, &pipe_way_, &climb_way_,
                                 &line_way_};
};

} // namespace scraperx::sim
