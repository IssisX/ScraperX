// The player's strike (sim/physics_world_strike.cpp): the Action button's
// punch. Offered with her body close in front; thrown, it winds up, reaches
// and recoils; landed, it is a force on her held over its contact ticks and
// drawn from his strike limits (player_strike_limits.hpp), never a velocity
// written. On air it lands nothing. The same inputs give the same strike.

#include "sim_tests.hpp"

#include "sim/matron.hpp"
#include "sim/physics_world_support.hpp"
#include "sim/player_strike_limits.hpp"

#include <cmath>
#include <iostream>
#include <vector>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

struct Thrown {
    bool offered = false;
    int arm = 1;
    std::uint64_t strikes = 0;
    std::uint64_t hits = 0;
    double impulse_ns = 0.0;
    double her_moved_m = 0.0;
    double max_player_step_m = 0.0;
    double her_blow_mps = 0.0;
    Vector3 point{};
};

// Her `her_ahead_m` in front of him on the Deck154 start, one press: what it
// threw. `solids`: test steel in the world; `lost`, `broken`: an arm torn off,
// an arm broken (0 the left, 1 the right, -1 none) before she comes.
Thrown throw_one(const double her_ahead_m, const std::vector<Simulation::TestExtraSolid> &solids = {},
                 const int lost = -1, const int broken = -1) {
    Simulation s(InitialSpawn::Deck154, solids);
    s.place_matron_home(154.0F);
    for (int i = 0; i < 30; ++i) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    if (lost >= 0) {
        s.debug_lose_arm(lost);
    }
    if (broken >= 0) {
        s.debug_break_arm(broken);
    }
    const Vector3 p = s.snapshot().player_position;
    (void)s.set_facing(0.0, 1.0);
    s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + her_ahead_m});
    (void)s.advance_frame(Simulation::kFixedStepSeconds);
    Thrown out;
    out.offered = s.snapshot().strike_offered;
    out.arm = s.snapshot().strike_arm;
    const Vector3 her_before = s.matron_state().position;
    (void)s.request_strike();
    Vector3 last = s.snapshot().player_position;
    for (int i = 0; i < 40; ++i) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const Vector3 now = s.snapshot().player_position;
        out.max_player_step_m = std::max(out.max_player_step_m,
                                         std::hypot(now.x - last.x, now.y - last.y, now.z - last.z));
        last = now;
        out.her_blow_mps = std::max(out.her_blow_mps, s.matron_state().blow_mps);
    }
    const auto &snap = s.snapshot();
    out.strikes = snap.strike_count;
    out.hits = snap.strike_hit_count;
    out.impulse_ns = snap.strike_impulse_ns;
    out.point = snap.strike_hit_point;
    const Vector3 her_after = s.matron_state().position;
    out.her_moved_m = std::hypot(her_after.x - her_before.x, her_after.z - her_before.z);
    return out;
}

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

bool go(Simulation &s, const double x, const double z, const bool sprint, const double seconds) {
    for (int i = 0; i < static_cast<int>(seconds * Simulation::kTickRateHz); ++i) {
        const auto p = s.snapshot().player_position;
        const double dx = x - p.x, dz = z - p.z, l = std::hypot(dx, dz);
        if (l < 0.5) {
            (void)s.set_move_input(0.0, 0.0);
            (void)s.set_sprint_input(false);
            return true;
        }
        (void)s.set_move_input(dx / l, dz / l);
        (void)s.set_facing(dx / l, dz / l);
        (void)s.set_sprint_input(sprint);
        tick(s);
    }
    return false;
}

// As matron grab's: woken, called to a spot north-west of her locker, changed.
void meet_her(Simulation &s, const double x, const double z) {
    s.debug_fix_matron_pace(3);
    require(s.advance_frame(1.0).accepted, "strike: settle");
    require(go(s, 10.0, -128.5, true, 4.0), "strike: a sprint to wake her");
    require(go(s, x, z, false, 30.0), "strike: to the spot");
    (void)s.set_move_input(0.0, 0.0);
    constexpr int kStraining = static_cast<int>(scraperx::sim::Matron::State::Straining);
    constexpr int kEmerging = static_cast<int>(scraperx::sim::Matron::State::Emerging);
    for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz) &&
                    (s.matron_state().state == kStraining || s.matron_state().state == kEmerging);
         ++i) {
        tick(s);
    }
    require(s.set_room_loudness(1.0), "strike: a noise from the spot");
    tick(s);
    require(s.set_room_loudness(0.0), "strike: quiet again");
    s.debug_set_matron_stage(2);
}
} // namespace

void run_strike() {
    using scraperx::sim::strike::kPlayerStrikeForceN;
    const Thrown close = throw_one(1.05);
    std::cout << "  strike close: offered=" << close.offered << " hits=" << close.hits
              << " impulse_ns=" << close.impulse_ns << " her_moved_m=" << close.her_moved_m
              << " max_step_m=" << close.max_player_step_m << "\n";
    require(close.offered, "strike: Action offers the punch with her body close in front");
    require(close.strikes == 1, "strike: one press throws one strike");
    require(close.hits == 1, "strike: a strike at her body within reach lands once");
    // Force times time: at most the limit held over its 3 contact ticks.
    const double most = kPlayerStrikeForceN * 3.0 * Simulation::kFixedStepSeconds + 1.0e-3;
    require(close.impulse_ns > 0.0 && close.impulse_ns <= most,
            "strike: the landing's impulse is his force held over the contact, within his limit");
    require(close.max_player_step_m <= 0.20, "strike: no tick moves the player more than 0.20 m");

    const Thrown far = throw_one(3.0);
    std::cout << "  strike far: offered=" << far.offered << " hits=" << far.hits << "\n";
    require(!far.offered, "strike: Action does not offer the punch with her out of reach");
    require(far.strikes == 1 && far.hits == 0, "strike: a strike on air lands nothing");

    const Thrown again = throw_one(1.05);
    require(again.hits == close.hits && again.impulse_ns == close.impulse_ns &&
                again.point.x == close.point.x && again.point.y == close.point.y && again.point.z == close.point.z,
            "strike: the same inputs give the same strike, to the bit");
    // It moves her: hunting (out of her locker's emergence, when blows are
    // read), her balance reads the blow as the impulse over her mass.
    double blow_mps = 0.0, dv = 0.0;
    std::uint64_t blow_hits = 0;
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        for (int i = 0; i < 30; ++i) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        const Vector3 p = s.snapshot().player_position;
        (void)s.set_facing(0.0, 1.0);
        s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + 1.05});
        constexpr int kHunting = static_cast<int>(scraperx::sim::Matron::State::Hunting);
        for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && s.matron_state().state != kHunting; ++i) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        for (int i = 0; i < 90 && !s.snapshot().strike_offered; ++i) {
            const Vector3 me = s.snapshot().player_position;
            const Vector3 her = s.matron_state().position;
            (void)s.set_facing(her.x - me.x, her.z - me.z);
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        (void)s.request_strike();
        for (int i = 0; i < 40; ++i) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
            blow_mps = std::max(blow_mps, s.matron_state().blow_mps);
        }
        blow_hits = s.snapshot().strike_hit_count;
        dv = s.snapshot().strike_impulse_ns / scraperx::sim::Matron::kMassKg;
        require(std::abs(s.matron_state().damage_ns - s.snapshot().strike_impulse_ns) < 0.05 &&
                    std::abs(s.matron_state().player_damage_ns - s.snapshot().strike_impulse_ns) < 0.05,
                "strike: landed punch accumulates its 50 N-s into damage_ns and player_damage_ns");
    }
    std::cout << "  strike blow: hits=" << blow_hits << " her_blow_mps=" << blow_mps << " impulse/mass=" << dv << "\n";
    require(blow_hits == 1 && dv > 0.0 && blow_mps >= 0.8 * dv && blow_mps <= 1.5 * dv + 0.05,
            "strike: her velocity change is about the impulse over her 220 kg");

    // Her hands on him (the owner's design): during her reach, before her
    // fingers close, the brace keeps Action, no punch; in her hand before
    // the rip the punch is offered and thrown, his escape, and lands on her
    // within his limit; once the rip has begun, none.
    constexpr int kGrab = static_cast<int>(scraperx::sim::Matron::State::Grab);
    constexpr int kCarry = static_cast<int>(scraperx::sim::Matron::State::Carry);
    constexpr int kArmPlay = static_cast<int>(scraperx::sim::Matron::State::ArmPlay);
    const auto until = [](Simulation &s, const auto &done) {
        for (int i = 0; i < 60 * static_cast<int>(Simulation::kTickRateHz) && !done(); ++i) {
            (void)s.set_move_input(0.0, 0.0);
            tick(s);
        }
        return done();
    };
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        meet_her(s, -6.0, -129.0);
        s.debug_matron_hold(0, 3.0);
        require(until(s, [&] { return s.matron_state().state == kGrab && !s.snapshot().held_by_matron; }),
                "strike: her reach for him (setup)");
        const std::uint64_t before = s.snapshot().strike_count;
        require(!s.snapshot().strike_offered, "strike: during her reach Action does not offer the punch (the brace)");
        (void)s.request_strike();
        tick(s);
        require(s.snapshot().strike_count == before, "strike: no strike starts during her reach");
        require(until(s, [&] { return s.matron_state().state == kCarry && s.snapshot().held_by_matron; }),
                "strike: she carries him in her hand (setup)");
        require(s.snapshot().strike_offered, "strike: in her hand before the rip Action offers the punch (his escape)");
        const std::uint64_t hits = s.snapshot().strike_hit_count;
        const std::uint64_t tears = s.snapshot().grip_tear_count;
        double grip_peak_n = 0.0;
        (void)s.request_strike();
        for (int i = 0; i < 40; ++i) {
            tick(s);
            grip_peak_n = std::max(grip_peak_n, s.snapshot().grip_load_n);
        }
        const auto &snap = s.snapshot();
        const double most = kPlayerStrikeForceN * 3.0 * Simulation::kFixedStepSeconds + 1.0e-3;
        std::cout << "  strike in her hand: strikes=" << snap.strike_count - before << " hits=" << snap.strike_hit_count - hits
                  << " impulse_ns=" << snap.strike_impulse_ns << " grip_peak_n=" << grip_peak_n
                  << " grip_tears=" << snap.grip_tear_count - tears << "\n";
        require(snap.strike_count == before + 1 && snap.strike_hit_count == hits + 1,
                "strike: thrown in her hand, the punch lands on her");
        require(snap.strike_impulse_ns > 0.0 && snap.strike_impulse_ns <= most,
                "strike: the punch in her hand is his force held over the contact, within his limit");
        require(snap.grip_tear_count == tears && grip_peak_n < scraperx::sim::Matron::kGripMaxForceN,
                "strike: his punch in her hand tears nothing: her grip's load stays under its limit");
    }
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        meet_her(s, -6.0, -129.0);
        s.debug_matron_hold(0, 3.0);
        require(until(s, [&] { return s.matron_state().state == kArmPlay && s.snapshot().held_by_matron; }),
                "strike: her rip begun, him in her hand (setup)");
        const std::uint64_t before = s.snapshot().strike_count;
        require(!s.snapshot().strike_offered, "strike: once the rip has begun Action does not offer the punch");
        (void)s.request_strike();
        tick(s);
        require(s.snapshot().strike_count == before, "strike: no strike starts once the rip has begun");
    }

    // A checkpoint load mid-strike: the fist back at guard, nothing lands.
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        for (int i = 0; i < 30; ++i) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        const std::vector<std::uint8_t> saved = s.save_game();
        const Vector3 p = s.snapshot().player_position;
        (void)s.set_facing(0.0, 1.0);
        s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + 1.05});
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        (void)s.request_strike();
        for (int i = 0; i < 14; ++i) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        require(s.snapshot().strike_phase != 0, "strike: mid-strike before the load (setup)");
        require(s.load_game(saved.data(), saved.size()), "strike: the checkpoint loads");
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto &snap = s.snapshot();
        require(snap.strike_phase == 0 && snap.strike_impulse_ns == 0.0,
                "strike: a checkpoint load mid-strike leaves the fist at guard with no blow");
        // No blow pending in her from before the load: a few ticks on, her
        // balance has taken none.
        for (int i = 0; i < 8; ++i) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        const auto m = s.matron_state();
        require(m.staggers == 0 && m.knockdowns == 0 && m.blow_mps == 0.0,
                "strike: a checkpoint load mid-strike carries no pending blow into her balance");
    }
    // At the toolbox by the north band's rail (PICK UP on offer), her body
    // dead ahead in reach: the punch (it would land) and the pick-up are both
    // there, each on its own button.
    const auto at_the_box = [](Simulation &s) {
        s.place_matron_home(154.0F);
        require(s.advance_frame(1.0).accepted, "strike: settle");
        s.debug_launch_player(Vector3{-4.4, 154.95, -125.85}, Vector3{0.0, 0.0, 0.0});
        (void)s.set_facing(0.0, 1.0);
        for (int i = 0; i < 90; ++i) {
            tick(s);
        }
    };
    {
        Simulation s(InitialSpawn::Deck154);
        at_the_box(s);
        const Vector3 p = s.snapshot().player_position;
        s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + 1.05});
        tick(s);
        require(s.snapshot().carry_target_entity_id != 0 && s.snapshot().strike_offered && s.snapshot().strike_ready,
                "strike: at the toolbox with her body in reach ahead, the punch and the pick-up are both on offer");
    }
    // A punch under way owns his hands: thrown on air at the toolbox, Action
    // pressed for the pick-up and the climb every tick of its wind-up, reach
    // and recoil starts neither; at guard again, the pick-up takes it.
    {
        Simulation s(InitialSpawn::Deck154);
        at_the_box(s);
        require(s.snapshot().carry_target_entity_id != 0, "strike: the toolbox in reach (setup)");
        const std::uint64_t before = s.snapshot().strike_count;
        (void)s.request_strike();
        tick(s);
        require(s.snapshot().strike_count == before + 1 && s.snapshot().strike_phase != 0, "strike: thrown on air (setup)");
        bool other = false;
        for (int i = 0; i < 60 && s.snapshot().strike_phase != 0; ++i) {
            (void)s.request_pick_up();
            (void)s.request_traversal();
            tick(s);
            other = other || s.snapshot().carrying_entity_id != 0 ||
                    s.snapshot().traversal_state != scraperx::sim::TraversalState::None;
        }
        require(!other && s.snapshot().strike_phase == 0,
                "strike: while his punch is under way no pick-up or climb starts (his hands are the punch's)");
        for (int i = 0; i < 30 && s.snapshot().carrying_entity_id == 0; ++i) {
            (void)s.request_pick_up();
            tick(s);
        }
        require(s.snapshot().carrying_entity_id != 0, "strike: at guard again, the pick-up takes the toolbox");
    }
    // The world takes his hands from a punch under way: his left torn off and
    // his right broken in its wind-up, no fist left; it ends there and lands
    // nothing, her body in reach in front all the while.
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        for (int i = 0; i < 30; ++i) {
            tick(s);
        }
        s.debug_lose_arm(0);
        const Vector3 p = s.snapshot().player_position;
        (void)s.set_facing(0.0, 1.0);
        s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + 1.05});
        tick(s);
        const std::uint64_t hits = s.snapshot().strike_hit_count;
        (void)s.request_strike();
        for (int i = 0; i < 4; ++i) {
            tick(s);
        }
        require(s.snapshot().strike_phase == 1, "strike: in its wind-up (setup)");
        s.debug_break_arm(1);
        tick(s);
        const bool ended = s.snapshot().strike_phase == 0;
        for (int i = 0; i < 40; ++i) {
            tick(s);
        }
        require(ended && s.snapshot().strike_hit_count == hits,
                "strike: his last fist broken in its wind-up, the punch ends there and lands nothing");
    }
    // The fist is a fist, not a line: a post 1 cm thick 4.5 cm off its way
    // (inside its 6 cm) takes it, no landing; 10 cm off, it lands.
    Vector3 start{};
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        for (int i = 0; i < 30; ++i) {
            tick(s);
        }
        start = s.snapshot().player_position;
    }
    const auto post = [&start](const double off_x) {
        return std::vector<Simulation::TestExtraSolid>{
            Simulation::TestExtraSolid{start.x + off_x, start.y + 0.25, start.z + 0.40, 0.005F, 0.5F, 0.005F, true}};
    };
    const Thrown grazed = throw_one(1.05, post(0.045));
    const Thrown clear = throw_one(1.05, post(0.10));
    std::cout << "  strike by a post: 4.5 cm off offered=" << grazed.offered << " hits=" << grazed.hits
              << ", 10 cm off offered=" << clear.offered << " hits=" << clear.hits << "\n";
    require(grazed.strikes == 1 && grazed.hits == 0 && !grazed.offered,
            "strike: a thin post within the fist's breadth of its way takes the fist (none offered, none landed)");
    require(clear.offered && clear.hits == 1, "strike: the same post clear of the fist's breadth, it lands");
    // No fist to throw, no punch: his right torn off and his left broken,
    // none offered and a press throws none; the right off alone, the left
    // throws it and it lands.
    const Thrown none = throw_one(1.05, {}, 1, 0);
    const Thrown left = throw_one(1.05, {}, 1, -1);
    std::cout << "  strike arms: torn and broken arm=" << none.arm << " offered=" << none.offered
              << " strikes=" << none.strikes << ", right torn arm=" << left.arm << " hits=" << left.hits << "\n";
    require(none.arm == -1 && !none.offered && none.strikes == 0,
            "strike: one arm torn off and the other broken, no fist: no punch offered or thrown");
    require(left.arm == 0 && left.offered && left.hits == 1, "strike: the right torn off, the left throws it and it lands");
    // In the air no punch: the PUNCH button greyed and a press ignored, so a
    // lunge's catch and a wall run's kick are never eaten by one.
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        for (int i = 0; i < 30; ++i) {
            tick(s);
        }
        const Vector3 p = s.snapshot().player_position;
        s.debug_launch_player(Vector3{p.x, p.y + 0.05, p.z}, Vector3{0.0, 4.0, 0.0});
        for (int i = 0; i < 6; ++i) {
            tick(s);
        }
        const std::uint64_t before = s.snapshot().strike_count;
        const bool airborne = !s.snapshot().player_grounded;
        const bool ready = s.snapshot().strike_ready;
        (void)s.request_strike();
        tick(s);
        require(airborne && !ready && s.snapshot().strike_count == before,
                "strike: in the air the punch is greyed and a press throws none");
    }
    // Fighting S7: persistent damage_ns, player_damage_ns, stage boundaries (0..4, 6000 DEAD),
    // mixed-source window shares, (1 + 0.15 * stage) stagger/Down scaling, checkpoints, and Down pause.
    {
        using namespace JPH;
        using scraperx::sim::Matron;
        using scraperx::sim::Stimulus;
        namespace detail = scraperx::sim::detail;
        static_assert(Matron::damage_stage_for(0.0F) == 0 && Matron::damage_stage_for(1199.0F) == 0 &&
                      Matron::damage_stage_for(1200.0F) == 1 && Matron::damage_stage_for(2699.0F) == 1 &&
                      Matron::damage_stage_for(2700.0F) == 2 && Matron::damage_stage_for(4199.0F) == 2 &&
                      Matron::damage_stage_for(4200.0F) == 3 && Matron::damage_stage_for(5399.0F) == 3 &&
                      Matron::damage_stage_for(5400.0F) == 4 && Matron::damage_stage_for(5999.0F) == 4);
        struct DamageWorld {
            detail::JoltRuntimeLease runtime;
            detail::BroadPhaseLayerInterface layers;
            detail::ObjectVsBroadPhaseFilter broad;
            detail::ObjectLayerPairFilter pairs;
            TempAllocatorImpl temp{8 * 1024 * 1024};
            JobSystemThreadPool jobs{cMaxPhysicsJobs, cMaxPhysicsBarriers, 1};
            PhysicsSystem physics;
            BodyID floor{}, player{}, striker{};
            const float dt = 1.0F / 90.0F;
            DamageWorld() {
                physics.Init(512, 0, 512, 512, layers, broad, pairs);
                BodyInterface &b = physics.GetBodyInterface();
                floor = b.CreateAndAddBody(
                    BodyCreationSettings(new BoxShape(Vec3(200.0F, 0.5F, 200.0F)), RVec3(0.0, 153.5, -130.0),
                                         Quat::sIdentity(), EMotionType::Static, detail::object_layers::kStatic),
                    EActivation::DontActivate);
                player = b.CreateAndAddBody(
                    BodyCreationSettings(new SphereShape(0.35F), RVec3(20.0, 155.0, -150.0), Quat::sIdentity(),
                                         EMotionType::Kinematic, detail::object_layers::kMoving),
                    EActivation::DontActivate);
                BodyCreationSettings crate(new SphereShape(0.25F), RVec3(20.0, 158.0, -150.0), Quat::sIdentity(),
                                           EMotionType::Dynamic, detail::object_layers::kMoving);
                crate.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
                crate.mMassPropertiesOverride.mMass = 40.0F;
                crate.mGravityFactor = 0.0F;
                striker = b.CreateAndAddBody(crate, EActivation::DontActivate);
            }
            ~DamageWorld() {
                BodyInterface &b = physics.GetBodyInterface();
                for (const BodyID id : {striker, player, floor}) {
                    if (b.IsAdded(id)) b.RemoveBody(id);
                    if (!id.IsInvalid()) b.DestroyBody(id);
                }
            }
            void step(Matron &m) {
                m.pre_step(dt, Stimulus(RVec3(20.0, 155.0, -150.0), 0.0F));
                physics.Update(dt, 1, &temp, &jobs);
                m.post_step(dt);
            }
            void wake(Matron &m, const float damage_ns = 0.0F, const float player_ns = 0.0F) {
                m.restore({Matron::State::Hunting, 0.0F, 0.5F, Matron::Stage::Dangerous, 0.0F, 0, damage_ns, player_ns});
                m.place_for_test(RVec3(0.0, 154.0, -130.0));
                m.face_for_test(0.0F);
                step(m);
                require(m.view().alive && m.view().state == Matron::State::Hunting, "S7: hunting on floor");
            }
            void punch_ns(Matron &m, const float ns, const bool quiet_after = true) {
                const RVec3 at = physics.GetBodyInterface().GetPosition(m.body());
                m.pre_step(dt, Stimulus(RVec3(20.0, 155.0, -150.0), 0.0F));
                m.struck_by_player(Vec3(0.0F, 0.0F, -ns), at + RVec3(0.0, 0.0, 1.0), false);
                physics.Update(dt, 1, &temp, &jobs);
                m.post_step(dt);
                for (int i = 0; quiet_after && m.view().alive && i < Matron::kBlowQuietTicks; ++i) step(m);
            }
            void punch_to(Matron &m, const float target_ns) {
                while (m.view().alive && m.view().damage_ns + 1.0e-3F < target_ns)
                    punch_ns(m, std::min(40.0F, target_ns - m.view().damage_ns), true);
            }
            void fire_striker(Matron &m, const float speed_mps) {
                BodyInterface &b = physics.GetBodyInterface();
                b.SetPositionAndRotation(striker, b.GetPosition(m.body()) + RVec3(0.0, 0.2, 0.56), Quat::sIdentity(),
                                         EActivation::Activate);
                b.SetLinearAndAngularVelocity(striker, Vec3(0.0F, 0.0F, -speed_mps), Vec3::sZero());
            }
            void park_striker() {
                BodyInterface &b = physics.GetBodyInterface();
                b.SetPositionAndRotation(striker, RVec3(20.0, 158.0, -150.0), Quat::sIdentity(), EActivation::DontActivate);
                b.SetLinearAndAngularVelocity(striker, Vec3::sZero(), Vec3::sZero());
            }
        };
        DamageWorld w;
        Matron m(w.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                 detail::object_layers::kStatic, w.player);
        w.wake(m);
        require(m.view().damage_ns == 0.0F && m.view().player_damage_ns == 0.0F && m.view().damage_stage == 0,
                "S7 boundary: 0 N-s starts at stage 0");
        const float thresholds[5] = {1200.0F, 2700.0F, 4200.0F, 5400.0F, 6000.0F};
        for (int s = 0; s < 5; ++s) {
            const float below = thresholds[s] - 1.0F;
            w.punch_to(m, below);
            require(std::abs(m.view().damage_ns - below) < 0.05F && std::abs(m.view().player_damage_ns - below) < 0.05F &&
                        m.view().damage_stage == static_cast<std::uint8_t>(s) && m.view().alive,
                    "S7 boundary: 1 N-s below threshold stays in lower stage and alive");
            w.punch_ns(m, 1.0F, true);
            if (s < 4) {
                require(std::abs(m.view().damage_ns - thresholds[s]) < 0.05F &&
                            m.view().damage_stage == static_cast<std::uint8_t>(s + 1) && m.view().alive,
                        "S7 boundary: crossing threshold advances stage");
            } else {
                require(std::abs(m.view().damage_ns - Matron::kKillDamageNs) < 0.05F &&
                            std::abs(m.view().player_damage_ns - Matron::kKillDamageNs) < 0.05F &&
                            m.view().damage_stage == 4 && !m.view().alive && m.view().state == Matron::State::Dead,
                        "S7 boundary: 6000 N-s kills her without a stage 5");
            }
        }
        w.wake(m);
        w.fire_striker(m, 4.0F);
        w.step(m);
        w.park_striker();
        for (int i = 0; i < Matron::kBlowQuietTicks; ++i) w.step(m);
        const float env_only_ns = m.view().damage_ns;
        require(env_only_ns > 80.0F && m.view().player_damage_ns == 0.0F,
                "S7 env: environmental blow counts in damage_ns with 0 player_damage_ns");
        w.wake(m);
        w.fire_striker(m, 4.0F);
        w.step(m);   // tick 0: environmental striker contacts her, opening the window
        w.park_striker();
        w.punch_ns(m, 50.0F, true);   // tick 1 (same judged window): 50 N-s player punch
        require(std::abs(m.view().player_damage_ns - 50.0F) < 0.05F &&
                    std::abs(m.view().damage_ns - (env_only_ns + 50.0F)) < 5.0F,
                "S7 mixed window: each source contributes its real share only (no double-count, no whole-window-to-player)");
        float stagger0 = 0.0F, down0 = 0.0F, pace0 = 0.0F;
        const float stage_ns[5] = {0.0F, 1200.0F, 2700.0F, 4200.0F, 5400.0F};
        for (int st = 0; st < 5; ++st) {
            w.wake(m, stage_ns[st], stage_ns[st]);
            if (st == 0) pace0 = m.view().pace_mps;
            require(std::abs(m.view().pace_mps - pace0) < 1.0e-5F && m.view().damage_stage == st,
                    "S7 scaling: offensive pace never scales with damage stage");
            w.punch_ns(m, 150.0F, false);
            require(m.view().state == Matron::State::Staggering, "S7 scaling: 150 N-s staggers her");
            if (st == 0) stagger0 = m.view().stagger_left_s;
            const float scale = 1.0F + 0.15F * static_cast<float>(st);
            require(std::abs(m.view().stagger_left_s - stagger0 * scale) < 0.02F,
                    "S7 scaling: stagger time scales by 1 + 0.15 * stage");
            w.wake(m, stage_ns[st], stage_ns[st]);
            w.punch_ns(m, 500.0F, false);
            require(m.view().state == Matron::State::Down, "S7 scaling: 500 N-s knocks her down");
            if (st == 0) down0 = m.view().down_left_s;
            require(std::abs(m.view().down_left_s - down0 * scale) < 0.02F,
                    "S7 scaling: Down time scales by 1 + 0.15 * stage");
        }
        const float down_dmg = m.view().damage_ns, down_plr = m.view().player_damage_ns;
        w.punch_ns(m, 200.0F, false);
        w.fire_striker(m, 5.0F);
        for (int i = 0; i < 6; ++i) w.step(m);
        w.park_striker();
        require(m.view().state == Matron::State::Down && std::abs(m.view().damage_ns - down_dmg) < 0.01F &&
                    std::abs(m.view().player_damage_ns - down_plr) < 0.01F,
                "S7 Down pause: blows during Down are ignored");
        w.wake(m, 400.0F, 150.0F);
        const Matron::Saved cp1 = m.save();
        w.punch_ns(m, 150.0F, true);
        w.punch_to(m, 1300.0F);
        const Matron::Saved cp2 = m.save();
        w.punch_ns(m, 4700.0F, false);
        require(!m.view().alive && m.view().state == Matron::State::Dead, "S7 checkpoint: killed before restore");
        m.restore(cp2);
        require(m.view().alive && std::abs(m.view().damage_ns - 1300.0F) < 0.05F &&
                    std::abs(m.view().player_damage_ns - 1050.0F) < 0.05F && m.view().damage_stage == 1,
                "S7 checkpoint: restore returns cp2 damage_ns and player_damage_ns");
        m.restore(cp1);
        require(m.view().alive && std::abs(m.view().damage_ns - 400.0F) < 0.05F &&
                    std::abs(m.view().player_damage_ns - 150.0F) < 0.05F && m.view().damage_stage == 0,
                "S7 checkpoint: rewinding to earlier checkpoint cp1 rewinds damage");
        std::cout << "PASS scraperx_sim fighting S7: env_ns=" << env_only_ns << " stagger0=" << stagger0
                  << " down0=" << down0 << "\n";
    }
    std::cout << "PASS scraperx_sim strike: impulse_ns=" << close.impulse_ns << "\n";
}
