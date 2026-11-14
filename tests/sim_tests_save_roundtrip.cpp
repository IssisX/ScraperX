// J-116 (#438): a relaunch puts the world back exactly as a death restore
// does, and a bad save never breaks a load.
//
// The probe (save-probe slice) answers what
// each climb point's rigging actually commits to the checkpoint, whether the
// Matron wakes on the rides, and how the player dies at each height. The
// round-trip test (SCRAPERX_SLICE save-roundtrip) is built on the same rig
// functions: each leg rigs a checkpoint in world A, A dies and restores, and
// a fresh world B loads A's save; A and B must agree, byte for byte on the
// re-saved checkpoint and live after the same ticks.

#include "sim_tests.hpp"
#include "sim/bay.hpp"
#include "sim/mechanism_kit.hpp"
#include "sim/physics_world_support.hpp"
#include "sim/slingshot.hpp"
#include "sim/swing.hpp"
#include "sim/vertical/vertical_machine.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#ifndef SCRAPERX_SOURCE_DIR
#error "SCRAPERX_SOURCE_DIR must name the repository (CMakeLists.txt sets it)"
#endif

namespace {

using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
using scraperx::sim::Vector3;
using Bytes = std::vector<std::uint8_t>;

void print_state(const char *tag, const Simulation &s) {
    const Snapshot snap = s.snapshot();
    const auto matron = s.matron_state();
    std::cout << "  probe " << tag << ": pos=(" << snap.player_position.x << "," << snap.player_position.y
              << "," << snap.player_position.z << ") grounded=" << int(snap.player_grounded)
              << " support=" << snap.support_entity_id << " carrying=" << snap.carrying_entity_id
              << " deaths=" << snap.death_count << " cp=(" << snap.checkpoint_position.x << ","
              << snap.checkpoint_position.y << "," << snap.checkpoint_position.z
              << ") commits=" << snap.checkpoint_commit_count << " matron_state=" << int(matron.state)
              << " stage=" << int(matron.stage) << " rage=" << matron.rage
              << " exposure=" << matron.exposure << '\n';
}

// Walk toward the shaft until the death count rises, or `seconds` pass.
// True once the walk killed the player.
bool try_walkoff(Simulation &s, const double dx, const double dz, const double seconds) {
    const std::uint64_t deaths = s.snapshot().death_count;
    const std::uint32_t ticks = static_cast<std::uint32_t>(seconds * Simulation::kTickRateHz);
    for (std::uint32_t tick = 0; tick < ticks && s.snapshot().death_count == deaths; ++tick) {
        (void)s.set_move_input(s.snapshot().player_grounded ? dx : 0.0, s.snapshot().player_grounded ? dz : 0.0);
        (void)s.set_facing(dx, dz);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    return s.snapshot().death_count == deaths + 1;
}

// Stage a lethal fall where no walk-off exists: straight up from where the
// player stands, then down onto the same spot (sim_tests_corpse.cpp stages
// its falls the same way, through this test-only hook).
bool die_by_launch(Simulation &s, const Vector3 offset = {0.0, 0.0, 0.0}) {
    const std::uint64_t deaths = s.snapshot().death_count;
    const Vector3 at = s.snapshot().player_position;
    s.debug_launch_player({at.x + offset.x, at.y + 60.0 + offset.y, at.z + offset.z}, {0.0, 0.0, 0.0});
    const std::uint32_t ticks = static_cast<std::uint32_t>(10.0 * Simulation::kTickRateHz);
    for (std::uint32_t tick = 0; tick < ticks && s.snapshot().death_count == deaths; ++tick) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    return s.snapshot().death_count == deaths + 1;
}

Bytes read_file(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    Bytes out((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    require(!out.empty(), "save-roundtrip: the test objects are in godot/tests/fabricated");
    return out;
}

std::uint64_t add_keg(Simulation &s, const Bytes &hull, const Vector3 at) {
    scraperx::sim::FabricatedRefusal why = scraperx::sim::FabricatedRefusal::None;
    const std::uint64_t entity = s.add_fabricated(hull.data(), hull.size(), at, 0.0, &why);
    require(entity != 0 && why == scraperx::sim::FabricatedRefusal::None, "save-roundtrip: the keg is delivered");
    return entity;
}

// ---- the swing (copied from sim_tests_rides.cpp: swing_board/swing_ride) --

bool swing_board(Simulation &s) {
    return walk_to(s, -13.35, -127.5, 20.0) && walk_to(s, -13.35, -98.8, 25.0) &&
           walk_to(s, -15.0, -98.85, 6.0, 0.08);
}

// ---- G's platform (copied from sim_tests_plate.cpp: board_shop_g) ----------

bool board_shop_g(Simulation &s) {
    return walk_to(s, 11.3, -138.0, 6.0) && walk_to(s, 1.0, -141.0, 16.0) &&
           walk_to(s, -8.0, -147.0, 12.0) && walk_to(s, -8.0, -150.0, 4.0) &&
           walk_to(s, -11.2, -150.0, 6.0, 0.08);
}

// ---- J's joint and chock (copied from sim_tests_crane.cpp) -----------------

bool lay_crane_joint(Simulation &s) {
    if (!walk_to(s, 2.8, -139.0, 12.0, 0.08)) {
        return false;
    }
    (void)s.set_facing(1.0, 0.0);
    (void)s.advance_frame(0.4);
    if (s.snapshot().carry_target_entity_id != Simulation::kCraneJJointEntityId) {
        return false;
    }
    (void)s.request_pick_up();
    (void)s.advance_frame(0.4);
    if (s.snapshot().carrying_entity_id != Simulation::kCraneJJointEntityId ||
        !(walk_to(s, -0.3, -138.3, 8.0) && walk_to(s, -0.3, -136.2, 6.0, 0.08) &&
          walk_to(s, 0.9, -136.2, 6.0, 0.08))) {
        return false;
    }
    (void)s.set_facing(1.0, 0.0);
    (void)s.advance_frame(0.8);
    (void)s.request_set_down();
    (void)s.advance_frame(1.5);
    return s.crane_state().j_rail_whole;
}

bool pull_crane_chock(Simulation &s) {
    return pull_facing(s, 0.0, -135.5, 0.0, 1.0, Simulation::kCraneJHandleEntityId, 0.5, 4.0,
                       [&](const Snapshot &) { return !s.crane_state().j_wagon_latched; });
}

// ---- the five legs' rigging -------------------------------------------------
// Each rigs its checkpoint and leaves the player standing where the
// checkpoint holds him, with the rigging in the committed machine state.
// The rig functions are shared by the probe and the round-trip test.

bool rig_yard(Simulation &s, const Bytes &keg_hull) {
    (void)s.advance_frame(0.5);
    const std::uint64_t keg = add_keg(s, keg_hull, {6.0, 0.0, -27.0});
    (void)s.advance_frame(1.0);
    if (!walk_to(s, 6.0, -26.25, 6.0, 0.08)) {
        return false;
    }
    (void)s.set_facing(0.0, -1.0);
    (void)s.advance_frame(0.3);
    if (s.snapshot().carry_target_entity_id != keg) {
        return false;
    }
    (void)s.request_pick_up();
    (void)s.advance_frame(0.6);
    if (s.snapshot().carrying_entity_id != keg) {
        return false;
    }
    (void)s.advance_frame(2.0);
    return s.snapshot().player_grounded && s.snapshot().carrying_entity_id == keg;
}

bool rig_154(Simulation &s) {
    (void)s.advance_frame(1.0);
    if (!board_well_a(s) || !rig_well_a(s)) {
        return false;
    }
    (void)s.advance_frame(2.0);
    return s.snapshot().well_a_rope_end_entity_id == Simulation::kWellACageEntityId;
}

bool rig_221(Simulation &s) {
    (void)s.advance_frame(1.0);
    if (!swing_board(s)) {
        return false;
    }
    (void)s.set_facing(0.0, -1.0);
    (void)s.advance_frame(5.0);
    if (!s.swing_state().ram_held) {
        return false;
    }
    (void)s.request_swing_action();
    (void)s.advance_frame(0.3);
    if (!s.swing_state().seated) {
        return false;
    }
    (void)s.advance_frame(5.0);
    if (!s.swing_state().ram_held) {
        return false;
    }
    (void)s.request_swing_action();
    if (!wait_for(s, 0.5, [&s](const Snapshot &) { return s.swing_state().tripped; }) ||
        !wait_for(s, 4.0, [&s](const Snapshot &) { return s.swing_state().peak_buffer_force_n > 0.0; }) ||
        !wait_for(s, 12.0, [&s](const Snapshot &) {
            const auto state = s.swing_state();
            return state.held_at_top && state.seat_speed_mps < 0.05 && state.seat_angle_rad > 1.2;
        })) {
        return false;
    }
    (void)s.advance_frame(2.0);
    (void)s.request_swing_action();
    (void)s.advance_frame(0.3);
    if (s.swing_state().seated || !walk_to(s, -15.0, -129.3, 6.0)) {
        return false;
    }
    (void)s.advance_frame(1.0);
    return s.swing_state().held_at_top && s.snapshot().player_grounded;
}

bool rig_340(Simulation &s) {
    (void)s.advance_frame(1.0);
    if (s.shop_state().g_rope_on_eye || !s.shop_state().g_tower_latched) {
        return false;
    }
    if (!board_shop_g(s) || !pull_shop_g(s)) {
        return false;
    }
    (void)s.advance_frame(10.0);
    return !s.shop_state().g_tower_latched && s.shop_state().g_platform_travel < 0.02;
}

bool rig_484(Simulation &s) {
    (void)s.advance_frame(1.0);
    if (!lay_crane_joint(s) || !pull_crane_chock(s)) {
        return false;
    }
    if (!wait_for(s, 60.0, [&](const Snapshot &) { return s.crane_state().j_traveler_travel >= 43.9; })) {
        return false;
    }
    (void)s.advance_frame(1.0);
    return s.snapshot().player_grounded;
}

// ---- the round-trip comparison ----------------------------------------------

// The live state, after the same ticks from the same checkpoint: everything
// a restore can move, except the counters that legitimately differ (the tick
// and the sim time, and the death count: A died once more than B).
void require_same_live(const Simulation &a, const Simulation &b, const char *leg) {
    const Snapshot as = a.snapshot();
    const Snapshot bs = b.snapshot();
    const auto close = [](const double x, const double y) { return std::abs(x - y) <= 1.0e-3; };
    // Body rotations get a centiradian after a settle, for the same reason
    // positions get a centimetre: a body still relaxing (a beam at ~314 m
    // settles a milliradian a second) is at a slightly different point of
    // its relaxation in the two worlds, because the solver's warm-start
    // data is not saved. A centiradian is a third of a degree; a body
    // actually turned wrong is off by far more.
    const auto close_rot = [](const double x, const double y) { return std::abs(x - y) <= 1.0e-2; };
    // Positions and velocities get five centimetres after a settle: a body
    // that is still relaxing (a stack item at ~89 m creeps a millimetre a
    // second, a part at ~221 m settles a centimetre and a half) is at a
    // slightly different point of its relaxation in the two worlds, because
    // the solver's warm-start data is not saved and its noise changes the
    // trajectory. Five centimetres absorbs that; a body in the wrong place
    // by more than that is a real difference. The save comparison above is
    // the strict one (a millimetre); this one is the behavioural one.
    const auto close_pos = [](const double x, const double y) { return std::abs(x - y) <= 5.0e-2; };
    const auto close3 = [&close_pos](const Vector3 &x, const Vector3 &y) {
        return close_pos(x.x, y.x) && close_pos(x.y, y.y) && close_pos(x.z, y.z);
    };
    const auto check = [&](const bool ok, const char *what) {
        if (!ok) {
            std::cout << "  save-roundtrip live: " << leg << " differs: " << what << " A player=("
                      << as.player_position.x << "," << as.player_position.y << "," << as.player_position.z
                      << ") vel=(" << as.player_linear_velocity.x << "," << as.player_linear_velocity.y << ","
                      << as.player_linear_velocity.z << ") support=" << as.support_entity_id
                      << " carrying=" << as.carrying_entity_id << " deaths=" << as.death_count << " B player=("
                      << bs.player_position.x << "," << bs.player_position.y << "," << bs.player_position.z
                      << ") vel=(" << bs.player_linear_velocity.x << "," << bs.player_linear_velocity.y << ","
                      << bs.player_linear_velocity.z << ") support=" << bs.support_entity_id
                      << " carrying=" << bs.carrying_entity_id << " deaths=" << bs.death_count << '\n';
        }
        require(ok, leg);
    };
    check(close3(as.player_position, bs.player_position), "player position");
    check(close3(as.player_linear_velocity, bs.player_linear_velocity), "player velocity");
    check(as.support_entity_id == bs.support_entity_id, "support");
    check(as.carrying_entity_id == bs.carrying_entity_id, "carrying");
    check(as.death_count == bs.death_count + 1, "death count");
    check(a.kit_body_count() == b.kit_body_count(), "body count");
    for (std::uint32_t body = 0; body < a.kit_body_count(); ++body) {
        const bool enabled_ok = a.kit_body_enabled(body) == b.kit_body_enabled(body);
        if (!enabled_ok) {
            std::cout << "  save-roundtrip live: " << leg << " body " << body << " entity=" << a.kit_body_entity(body)
                      << " enabled differs\n";
        }
        require(enabled_ok, leg);
        // The carried body hangs from the player's hand on a point
        // constraint: it is a pendulum, free to rotate, so its swing after a
        // settle is not a stable quantity. A hair of solver noise in the
        // player's settle swings it a couple of degrees and a centimetre.
        // It is still checked — it must stay in the hands, not fall, not
        // flip — just to a pendulum's tolerance, not a bolted body's.
        const bool carried = as.carrying_entity_id != 0 && a.kit_body_entity(body) == as.carrying_entity_id;
        const auto close_carry = [](const double x, const double y) { return std::abs(x - y) <= 5.0e-2; };
        const auto close_carry_rot = [](const double x, const double y) { return std::abs(x - y) <= 1.0e-1; };
        // A carried pendulum's instantaneous velocity is phase-dependent:
        // it swings at a few centimetres a second, and a hair of solver
        // noise sets the phase. Half a metre a second still catches a keg
        // that has been dropped or thrown; a swinging one stays well under it.
        const auto close_carry_vel = [](const double x, const double y) { return std::abs(x - y) <= 5.0e-1; };
        if (carried) {
            const auto cpos = a.kit_body_position(body), cposb = b.kit_body_position(body);
            const auto crot = a.kit_body_rotation(body), crotb = b.kit_body_rotation(body);
            const auto cvel = a.kit_body_velocity(body), cvelb = b.kit_body_velocity(body);
            const bool cpos_ok = close_carry(cpos.x, cposb.x) && close_carry(cpos.y, cposb.y) &&
                                 close_carry(cpos.z, cposb.z);
            const bool crot_ok = close_carry_rot(crot.x, crotb.x) && close_carry_rot(crot.y, crotb.y) &&
                                 close_carry_rot(crot.z, crotb.z) && close_carry_rot(crot.w, crotb.w);
            const bool cvel_ok = close_carry_vel(cvel.x, cvelb.x) && close_carry_vel(cvel.y, cvelb.y) &&
                                 close_carry_vel(cvel.z, cvelb.z);
            if (!cpos_ok || !crot_ok || !cvel_ok) {
                std::cout << "  save-roundtrip live: " << leg << " carried body " << body << " entity="
                          << a.kit_body_entity(body) << (cpos_ok ? " pos=ok" : " pos DIFF")
                          << (crot_ok ? " rot=ok" : " rot DIFF") << (cvel_ok ? " vel=ok" : " vel DIFF") << " A pos=("
                          << cpos.x << "," << cpos.y << "," << cpos.z << ") B pos=(" << cposb.x << "," << cposb.y
                          << "," << cposb.z << ") A rot=(" << crot.x << "," << crot.y << "," << crot.z << "," << crot.w
                          << ") B rot=(" << crotb.x << "," << crotb.y << "," << crotb.z << "," << crotb.w
                          << ") A vel=(" << cvel.x << "," << cvel.y << "," << cvel.z << ") B vel=(" << cvelb.x << ","
                          << cvelb.y << "," << cvelb.z << ")\n";
            }
            require(cpos_ok && crot_ok && cvel_ok, leg);
            continue;
        }
        const auto apos = a.kit_body_position(body), bpos = b.kit_body_position(body);
        const auto aq = a.kit_body_rotation(body);
        const auto bq = b.kit_body_rotation(body);
        const auto avel = a.kit_body_velocity(body), bvel = b.kit_body_velocity(body);
        const bool pos_ok = close3(apos, bpos);
        const bool rot_ok = close_rot(aq.x, bq.x) && close_rot(aq.y, bq.y) && close_rot(aq.z, bq.z) &&
                            close_rot(aq.w, bq.w);
        const bool vel_ok = close3(avel, bvel);
        if (!pos_ok || !rot_ok || !vel_ok) {
            std::cout << "  save-roundtrip live: " << leg << " body " << body << " entity=" << a.kit_body_entity(body)
                      << (pos_ok ? " pos=ok" : " pos DIFF") << (rot_ok ? " rot=ok" : " rot DIFF")
                      << (vel_ok ? " vel=ok" : " vel DIFF") << " A pos=(" << apos.x << "," << apos.y << "," << apos.z
                      << ") B pos=(" << bpos.x << "," << bpos.y << "," << bpos.z << ") A rot=(" << aq.x << "," << aq.y
                      << "," << aq.z << "," << aq.w << ") B rot=(" << bq.x << "," << bq.y << "," << bq.z << "," << bq.w
                      << ") A vel=(" << avel.x << "," << avel.y << "," << avel.z << ") B vel=(" << bvel.x << ","
                      << bvel.y << "," << bvel.z << ")\n";
        }
        require(pos_ok && rot_ok && vel_ok, leg);
    }
    const auto am = a.matron_state();
    const auto bm = b.matron_state();
    const auto asling = a.slingshot_state();
    const auto bsling = b.slingshot_state();
    const auto aswing = a.swing_state();
    const auto bswing = b.swing_state();
    const auto abay = a.bay_state();
    const auto bbay = b.bay_state();
    const auto alift = a.lift_state();
    const auto blift = b.lift_state();
    std::cout << "  save-roundtrip live: " << leg << " matron A state=" << static_cast<int>(am.state)
              << " stage=" << static_cast<int>(am.stage) << " rage=" << am.rage << " strain=" << am.strain
              << " exposure=" << am.exposure << " dmg=" << am.damage_ns << " pdmg=" << am.player_damage_ns
              << " B state=" << static_cast<int>(bm.state) << " stage=" << static_cast<int>(bm.stage)
              << " rage=" << bm.rage << " strain=" << bm.strain << " exposure=" << bm.exposure << " dmg=" << bm.damage_ns
              << " pdmg=" << bm.player_damage_ns << " sling A seated=" << asling.seated << " rel=" << asling.released
              << " draw=" << asling.draw_m << " E=" << asling.energy_j << " B seated=" << bsling.seated
              << " rel=" << bsling.released << " draw=" << bsling.draw_m << " E=" << bsling.energy_j
              << " swing A seat=" << aswing.seat_angle_rad << " ram=" << aswing.ram_angle_rad << " tooth=" << aswing.tooth
              << " held=" << aswing.held_at_top << " B seat=" << bswing.seat_angle_rad << " ram=" << bswing.ram_angle_rad
              << " tooth=" << bswing.tooth << " held=" << bswing.held_at_top << " bay A leaf=" << abay.leaf_angle
              << " open=" << abay.leaf_open << " hooked=" << abay.hooked << " chain=" << abay.chain_n << " B leaf="
              << bbay.leaf_angle << " open=" << bbay.leaf_open << " hooked=" << bbay.hooked << " chain=" << bbay.chain_n
              << " lift A mach=" << static_cast<int>(alift.machine) << " travel=" << alift.travel << " target="
              << alift.target << " moving=" << alift.moving << " B mach=" << static_cast<int>(blift.machine)
              << " travel=" << blift.travel << " target=" << blift.target << " moving=" << blift.moving << '\n';
    check(am.state == bm.state && am.stage == bm.stage && close(am.rage, bm.rage) && close(am.strain, bm.strain) &&
              close(am.exposure, bm.exposure) && am.damage_ns == bm.damage_ns &&
              am.player_damage_ns == bm.player_damage_ns,
          "matron");
    check(asling.seated == bsling.seated && asling.released == bsling.released && close(asling.draw_m, bsling.draw_m) &&
              close(asling.energy_j, bsling.energy_j),
          "slingshot");
    check(aswing.seated == bswing.seated && aswing.ram_held == bswing.ram_held && aswing.tripped == bswing.tripped &&
              aswing.held_at_top == bswing.held_at_top && aswing.tooth == bswing.tooth &&
              close(aswing.seat_angle_rad, bswing.seat_angle_rad) && close(aswing.ram_angle_rad, bswing.ram_angle_rad),
          "swing");
    check(close(abay.leaf_angle, bbay.leaf_angle) && abay.leaf_open == bbay.leaf_open && abay.hooked == bbay.hooked &&
              close(abay.chain_n, bbay.chain_n),
          "bay");
    check(alift.machine == blift.machine && close(alift.travel, blift.travel) && close(alift.target, blift.target) &&
              alift.moving == blift.moving,
          "lift");
}

// The re-saved checkpoints, byte for byte, except the fields that
// legitimately differ between a world that died and one that loaded:
// the format key and tick (bytes [0,16)), the commit count ([29,37)) and
// the death count ([37,45)). The header is key [0,8), tick [8,16),
// checkpoint position RVec3 [16,28) (Jolt is single precision here: 3
// floats, 12 bytes, not 24), crouched [28,29), commits [29,37), deaths
// [37,45), carrying [45,53). Everything else — every body, rope, catch,
// grip, machine and her meters — must be identical.
// The two re-saved checkpoints, field by field. A raw byte comparison
// cannot work: Jolt's Vec3 is a 16-byte SIMD value whose fourth lane (W)
// is padding the physics carries but never means, and the structs hold
// alignment padding besides, so two identical worlds write different
// garbage in those lanes (the first mismatch was body 4's position W:
// 3997.82 against 976.585, neither a coordinate nor a rotation). The
// production writer knows this: write_fabricated writes the thirteen
// meaningful floats of a fabricated body one by one, never the struct.
// So this walks both saves with the production reader, in write_save's
// order, and compares every meaningful field — positions and velocities
// within float rounding, counts and flags exactly — skipping the W lanes
// and the padding by reading whole structs and comparing their fields.
// The commit and death counts legitimately differ (A died once, B loaded),
// so they are read but not compared; the key and tick are skipped.
bool saves_agree(const Bytes &a, const Bytes &b) {
    using scraperx::sim::detail::SaveReader;
    using BodyState = scraperx::sim::kit::Kit::BodyState;
    using Pile = scraperx::sim::kit::Kit::Pile;
    using AnchorIndex = scraperx::sim::kit::AnchorIndex;
    // Tolerances match the live comparison (require_same_live): a millimetre
    // on positions and velocities, a milliradian on rotations. The physics
    // is deterministic, but a constrained body settles by solver noise — Well
    // B's striker sits a hair off identity, and a death restore and a load
    // leave the solver different warm-start data, so its rotation differs
    // by ~2.5e-05 rad. That is noise, not a save defect, and a tighter
    // tolerance would fail the test on it forever.
    const auto closef = [](const float x, const float y) { return std::abs(x - y) <= 1.0e-3F; };
    const auto closed = [](const double x, const double y) {
        return std::abs(x - y) <= 1.0e-4 * std::max(1.0, std::max(std::abs(x), std::abs(y)));
    };
    const auto close3 = [&closef](const JPH::RVec3 &x, const JPH::RVec3 &y) {
        return closef(static_cast<float>(x.GetX()), static_cast<float>(y.GetX())) &&
               closef(static_cast<float>(x.GetY()), static_cast<float>(y.GetY())) &&
               closef(static_cast<float>(x.GetZ()), static_cast<float>(y.GetZ()));
    };
    const auto closeq = [&closef](const JPH::Quat &x, const JPH::Quat &y) {
        return closef(x.GetX(), y.GetX()) && closef(x.GetY(), y.GetY()) && closef(x.GetZ(), y.GetZ()) &&
               closef(x.GetW(), y.GetW());
    };
    const auto closev = [&closef](const JPH::Vec3 &x, const JPH::Vec3 &y) {
        return closef(x.GetX(), y.GetX()) && closef(x.GetY(), y.GetY()) && closef(x.GetZ(), y.GetZ());
    };
    SaveReader ra(a.data(), a.size()), rb(b.data(), b.size());
    std::uint64_t key_a = 0, key_b = 0, tick_a = 0, tick_b = 0;
    if (!ra.pod(key_a) || !rb.pod(key_b) || key_a != key_b || !ra.pod(tick_a) || !rb.pod(tick_b)) {
        std::cout << "  save-roundtrip agree: the key or tick did not read\n";
        return false;
    }
    JPH::RVec3 pos_a, pos_b;
    std::uint8_t crouched_a = 0, crouched_b = 0;
    std::uint64_t commits_a = 0, commits_b = 0, deaths_a = 0, deaths_b = 0, carrying_a = 0, carrying_b = 0;
    if (!ra.pod(pos_a) || !rb.pod(pos_b) || !close3(pos_a, pos_b) || !ra.pod(crouched_a) ||
        !rb.pod(crouched_b) || crouched_a != crouched_b || !ra.pod(commits_a) || !rb.pod(commits_b) ||
        !ra.pod(deaths_a) || !rb.pod(deaths_b) || !ra.pod(carrying_a) || !rb.pod(carrying_b) ||
        carrying_a != carrying_b) {
        std::cout << "  save-roundtrip agree: the header differs\n";
        return false;
    }
    const auto same_f32_list = [&](const char *what) {
        std::vector<float> x, y;
        if (!ra.list_up_to(x, 100000) || !rb.list_up_to(y, 100000) || x.size() != y.size()) {
            std::cout << "  save-roundtrip agree: " << what << " differs\n";
            return false;
        }
        for (std::size_t i = 0; i < x.size(); ++i) {
            if (!closef(x[i], y[i])) {
                std::cout << "  save-roundtrip agree: " << what << "[" << i << "] differs\n";
                return false;
            }
        }
        return true;
    };
    const auto same_bits = [&](const char *what) {
        std::vector<bool> x, y;
        if (!ra.bits_up_to(x, 100000) || !rb.bits_up_to(y, 100000) || x != y) {
            std::cout << "  save-roundtrip agree: " << what << " differs\n";
            return false;
        }
        return true;
    };
    std::vector<std::uint64_t> entities_a, entities_b;
    if (!ra.list_up_to(entities_a, 100000) || !rb.list_up_to(entities_b, 100000) ||
        entities_a != entities_b) {
        std::cout << "  save-roundtrip agree: entities differ\n";
        return false;
    }
    std::vector<BodyState> bodies_a, bodies_b;
    if (!ra.list_up_to(bodies_a, 100000) || !rb.list_up_to(bodies_b, 100000) ||
        bodies_a.size() != bodies_b.size()) {
        std::cout << "  save-roundtrip agree: the bodies differ\n";
        return false;
    }
    for (std::size_t i = 0; i < bodies_a.size(); ++i) {
        const BodyState &x = bodies_a[i], &y = bodies_b[i];
        const bool pos_ok = close3(x.position, y.position), rot_ok = closeq(x.rotation, y.rotation),
                   lin_ok = closev(x.linear, y.linear), ang_ok = closev(x.angular, y.angular);
        if (!pos_ok || !rot_ok || !lin_ok || !ang_ok || x.enabled != y.enabled || x.asleep != y.asleep) {
            std::cout << "  save-roundtrip agree: body " << i << " entity=" << entities_a[i]
                      << (pos_ok ? " pos=ok" : " pos DIFF") << (rot_ok ? " rot=ok" : " rot DIFF")
                      << (lin_ok ? " lin=ok" : " lin DIFF") << (ang_ok ? " ang=ok" : " ang DIFF")
                      << " enabled " << int(x.enabled) << "/" << int(y.enabled) << " asleep "
                      << int(x.asleep) << "/" << int(y.asleep) << " x_pos=(" << x.position.GetX() << ","
                      << x.position.GetY() << "," << x.position.GetZ() << ") y_pos=(" << y.position.GetX()
                      << "," << y.position.GetY() << "," << y.position.GetZ() << ") x_lin=("
                      << x.linear.GetX() << "," << x.linear.GetY() << "," << x.linear.GetZ()
                      << ") y_lin=(" << y.linear.GetX() << "," << y.linear.GetY() << ","
                      << y.linear.GetZ() << ") x_rot=(" << x.rotation.GetX() << "," << x.rotation.GetY()
                      << "," << x.rotation.GetZ() << "," << x.rotation.GetW() << ") y_rot=("
                      << y.rotation.GetX() << "," << y.rotation.GetY() << "," << y.rotation.GetZ()
                      << "," << y.rotation.GetW() << ")\n";
            return false;
        }
    }
    const auto same_anchor_list = [&]() {
        std::vector<AnchorIndex> x, y;
        if (!ra.list_up_to(x, 100000) || !rb.list_up_to(y, 100000) || x != y) {
            std::cout << "  save-roundtrip agree: the rope anchors differ\n";
            return false;
        }
        return true;
    };
    if (!same_anchor_list() || !same_f32_list("dog_floor") || !same_f32_list("bin_contents") ||
        !same_bits("rope_parted") || !same_bits("catch_latched") || !same_bits("latch_set")) {
        return false;
    }
    std::vector<Pile> piles_a, piles_b;
    if (!ra.list_up_to(piles_a, 256) || !rb.list_up_to(piles_b, 256) || piles_a.size() != piles_b.size()) {
        std::cout << "  save-roundtrip agree: the piles differ\n";
        return false;
    }
    for (std::size_t i = 0; i < piles_a.size(); ++i) {
        if (!close3(piles_a[i].at, piles_b[i].at) || !closef(piles_a[i].kg, piles_b[i].kg)) {
            std::cout << "  save-roundtrip agree: pile " << i << " differs\n";
            return false;
        }
    }
    if (!same_f32_list("pool_water") || !same_f32_list("cell_air") || !same_f32_list("reel_paid")) {
        return false;
    }
    float drained_a = 0.0F, drained_b = 0.0F;
    if (!ra.pod(drained_a) || !rb.pod(drained_b) || !closef(drained_a, drained_b)) {
        std::cout << "  save-roundtrip agree: the drained water differs\n";
        return false;
    }
    const auto same_slingshot = [&]() {
        scraperx::sim::Slingshot::State x, y;
        if (!ra.pod(x) || !rb.pod(y)) {
            std::cout << "  save-roundtrip agree: the slingshot did not read\n";
            return false;
        }
        static const bool scraperx::sim::Slingshot::State::* kBools[] = {
            &scraperx::sim::Slingshot::State::station_available, &scraperx::sim::Slingshot::State::seated,
            &scraperx::sim::Slingshot::State::drawing, &scraperx::sim::Slingshot::State::released,
            &scraperx::sim::Slingshot::State::guide_latched, &scraperx::sim::Slingshot::State::can_retrieve,
            &scraperx::sim::Slingshot::State::recovering, &scraperx::sim::Slingshot::State::guided_launch,
            &scraperx::sim::Slingshot::State::track_exit, &scraperx::sim::Slingshot::State::release_ready,
            &scraperx::sim::Slingshot::State::ledger_valid,
            &scraperx::sim::Slingshot::State::pouch_pair_excluded,
            &scraperx::sim::Slingshot::State::aim_ready, &scraperx::sim::Slingshot::State::aim_locked};
        for (const auto member : kBools) {
            if ((x.*member) != (y.*member)) {
                std::cout << "  save-roundtrip agree: a slingshot flag differs\n";
                return false;
            }
        }
        static const double scraperx::sim::Slingshot::State::* kDoubles[] = {
            &scraperx::sim::Slingshot::State::leather_deflection_m,
            &scraperx::sim::Slingshot::State::leather_energy_j,
            &scraperx::sim::Slingshot::State::leather_port_violation_j,
            &scraperx::sim::Slingshot::State::draw_m, &scraperx::sim::Slingshot::State::ratchet_draw_m,
            &scraperx::sim::Slingshot::State::energy_j, &scraperx::sim::Slingshot::State::work_j,
            &scraperx::sim::Slingshot::State::source_work_j,
            &scraperx::sim::Slingshot::State::source_power_w,
            &scraperx::sim::Slingshot::State::transmission_loss_j,
            &scraperx::sim::Slingshot::State::dissipated_work_j,
            &scraperx::sim::Slingshot::State::kinetic_j, &scraperx::sim::Slingshot::State::gravity_j,
            &scraperx::sim::Slingshot::State::energy_residual_j,
            &scraperx::sim::Slingshot::State::retrieval_work_j,
            &scraperx::sim::Slingshot::State::retrieval_source_power_w,
            &scraperx::sim::Slingshot::State::retrieval_braking_work_j,
            &scraperx::sim::Slingshot::State::aim_control_work_j,
            &scraperx::sim::Slingshot::State::aim_source_power_w,
            &scraperx::sim::Slingshot::State::target_yaw_rad,
            &scraperx::sim::Slingshot::State::target_elevation_rad,
            &scraperx::sim::Slingshot::State::max_draw_m, &scraperx::sim::Slingshot::State::band_rest_m,
            &scraperx::sim::Slingshot::State::power_limit_w, &scraperx::sim::Slingshot::State::yaw_rad,
            &scraperx::sim::Slingshot::State::elevation_rad};
        for (const auto member : kDoubles) {
            if (!closed(x.*member, y.*member)) {
                std::cout << "  save-roundtrip agree: a slingshot number differs\n";
                return false;
            }
        }
        static const JPH::RVec3 scraperx::sim::Slingshot::State::* kVectors[] = {
            &scraperx::sim::Slingshot::State::harness_rest_local,
            &scraperx::sim::Slingshot::State::anchor_left, &scraperx::sim::Slingshot::State::anchor_right,
            &scraperx::sim::Slingshot::State::pouch_position,
            &scraperx::sim::Slingshot::State::guide_origin,
            &scraperx::sim::Slingshot::State::carrier_origin,
            &scraperx::sim::Slingshot::State::launch_track_start,
            &scraperx::sim::Slingshot::State::launch_track_end};
        for (const auto member : kVectors) {
            if (!close3(x.*member, y.*member)) {
                std::cout << "  save-roundtrip agree: a slingshot anchor differs\n";
                return false;
            }
        }
        if (x.launch_count != y.launch_count) {
            std::cout << "  save-roundtrip agree: the slingshot launch count differs\n";
            return false;
        }
        return true;
    };
    const auto same_swing = [&]() {
        scraperx::sim::Swing::State x, y;
        if (!ra.pod(x) || !rb.pod(y)) {
            std::cout << "  save-roundtrip agree: the swing did not read\n";
            return false;
        }
        if (x.station_available != y.station_available || x.seated != y.seated ||
            x.may_leave != y.may_leave || x.ram_held != y.ram_held || x.tripped != y.tripped ||
            x.kicking != y.kicking || x.tooth != y.tooth || x.ledger_valid != y.ledger_valid ||
            !closed(x.kick_elapsed_s, y.kick_elapsed_s) || !closed(x.kick_work_j, y.kick_work_j) ||
            !closed(x.kick_travel_m, y.kick_travel_m) || !closed(x.seat_angle_rad, y.seat_angle_rad) ||
            !closed(x.ram_angle_rad, y.ram_angle_rad) || !closed(x.seat_speed_mps, y.seat_speed_mps) ||
            !closed(x.ram_speed_mps, y.ram_speed_mps) || !closed(x.seat_floor_y, y.seat_floor_y) ||
            !closed(x.apex_floor_y, y.apex_floor_y) ||
            !closed(x.buffer_compression_m, y.buffer_compression_m) ||
            !closed(x.buffer_force_n, y.buffer_force_n) ||
            !closed(x.peak_buffer_force_n, y.peak_buffer_force_n) ||
            !closed(x.peak_seat_accel_mps2, y.peak_seat_accel_mps2) ||
            !closed(x.buffer_loss_j, y.buffer_loss_j) || !closed(x.buffer_stored_j, y.buffer_stored_j) ||
            !closed(x.mechanical_j, y.mechanical_j) ||
            !closed(x.initial_mechanical_j, y.initial_mechanical_j) ||
            !closed(x.energy_residual_j, y.energy_residual_j)) {
            std::cout << "  save-roundtrip agree: the swing differs\n";
            return false;
        }
        return true;
    };
    const auto same_bay = [&]() {
        scraperx::sim::Bay::State x, y;
        if (!ra.pod(x) || !rb.pod(y) || x.restart_s != y.restart_s || x.supplied != y.supplied ||
            x.leaf_running != y.leaf_running || x.fan_running != y.fan_running) {
            std::cout << "  save-roundtrip agree: the bay differs\n";
            return false;
        }
        return true;
    };
    if (!same_slingshot() || !same_swing() || !same_bay()) {
        return false;
    }
    std::uint32_t controls_a = 0, controls_b = 0;
    if (!ra.pod(controls_a) || !rb.pod(controls_b) || controls_a != controls_b) {
        std::cout << "  save-roundtrip agree: the vertical controls differ\n";
        return false;
    }
    for (std::uint32_t i = 0; i < controls_a; ++i) {
        scraperx::sim::vertical::Command command_a, command_b;
        if (!ra.pod(command_a) || !rb.pod(command_b) || !closef(command_a.travel, command_b.travel) ||
            command_a.powered != command_b.powered) {
            std::cout << "  save-roundtrip agree: vertical control " << i << " differs\n";
            return false;
        }
        if (!same_f32_list("vertical target speeds")) {
            return false;
        }
    }
    if (!same_f32_list("vertical targets")) {
        return false;
    }
    const auto same_grip_u32 = [&]() {
        std::vector<std::uint32_t> x, y;
        if (!ra.list_up_to(x, 100000) || !rb.list_up_to(y, 100000) || x != y) {
            std::cout << "  save-roundtrip agree: the grip loads differ\n";
            return false;
        }
        return true;
    };
    if (!same_grip_u32() || !same_f32_list("grip ratings")) {
        return false;
    }
    // A save made before her damage ends at the grips: nothing more to read.
    if (ra.done() || rb.done()) {
        return ra.done() && rb.done();
    }
    std::uint32_t mark_a = 0, mark_b = 0;
    if (!ra.pod(mark_a) || !rb.pod(mark_b) || mark_a != mark_b) {
        std::cout << "  save-roundtrip agree: the mark after the grips differs\n";
        return false;
    }
    if (mark_a == 0x314D414DU) {  // "MAD1": her damage.
        float damage_a = 0.0F, damage_b = 0.0F, player_a = 0.0F, player_b = 0.0F;
        std::uint8_t dead_a = 0, dead_b = 0;
        if (!ra.pod(damage_a) || !rb.pod(damage_b) || !closef(damage_a, damage_b) || !ra.pod(player_a) ||
            !rb.pod(player_b) || !closef(player_a, player_b) || !ra.pod(dead_a) || !rb.pod(dead_b) ||
            dead_a != dead_b) {
            std::cout << "  save-roundtrip agree: her damage differs\n";
            return false;
        }
        if (!ra.done() || !rb.done()) {
            if (!ra.pod(mark_a) || !rb.pod(mark_b) || mark_a != mark_b) {
                std::cout << "  save-roundtrip agree: the mark after her damage differs\n";
                return false;
            }
        }
    }
    if (mark_a == 0x3154414DU) {  // "MAT1": her state, stage and meters (J-116).
        std::uint8_t state_a = 0, state_b = 0, stage_a = 0, stage_b = 0;
        float strain_a = 0.0F, strain_b = 0.0F, rage_a = 0.0F, rage_b = 0.0F, exposure_a = 0.0F,
              exposure_b = 0.0F;
        std::uint64_t handled_a = 0, handled_b = 0;
        if (!ra.pod(state_a) || !rb.pod(state_b) || state_a != state_b || !ra.pod(strain_a) ||
            !rb.pod(strain_b) || !closef(strain_a, strain_b) || !ra.pod(rage_a) || !rb.pod(rage_b) ||
            !closef(rage_a, rage_b) || !ra.pod(stage_a) || !rb.pod(stage_b) || stage_a != stage_b ||
            !ra.pod(exposure_a) || !rb.pod(exposure_b) || !closef(exposure_a, exposure_b) ||
            !ra.pod(handled_a) || !rb.pod(handled_b) || handled_a != handled_b) {
            std::cout << "  save-roundtrip agree: her state differs\n";
            return false;
        }
        if (!ra.done() || !rb.done()) {
            if (!ra.pod(mark_a) || !rb.pod(mark_b) || mark_a != mark_b) {
                std::cout << "  save-roundtrip agree: the mark after her state differs\n";
                return false;
            }
        }
    }
    if (mark_a == 0x314A4E49U) {  // "INJ1": her per-region injury (J-059 #343 fix).
        float region_a[9] = {}, region_b[9] = {}, event_a[9] = {}, event_b[9] = {};
        if (!ra.pod(region_a) || !rb.pod(region_b) || !ra.pod(event_a) || !rb.pod(event_b)) {
            std::cout << "  save-roundtrip agree: her injury differs\n";
            return false;
        }
        for (int j = 0; j < 9; ++j) {
            if (!closef(region_a[j], region_b[j]) || !closef(event_a[j], event_b[j])) {
                std::cout << "  save-roundtrip agree: her injury differs\n";
                return false;
            }
        }
        if (!ra.done() || !rb.done()) {
            if (!ra.pod(mark_a) || !rb.pod(mark_b) || mark_a != mark_b) {
                std::cout << "  save-roundtrip agree: the mark after her injury differs\n";
                return false;
            }
        }
    }
    if (!ra.done() || !rb.done()) {
        if (mark_a != 0x31424146U) {  // "FAB1": the fabricated objects.
            std::cout << "  save-roundtrip agree: the mark after her matron sections is not the fabricated mark\n";
            return false;
        }
        std::uint32_t count_a = 0, count_b = 0;
        if (!ra.pod(count_a) || !rb.pod(count_b) || count_a != count_b) {
            std::cout << "  save-roundtrip agree: the fabricated count differs\n";
            return false;
        }
        for (std::uint32_t i = 0; i < count_a; ++i) {
            std::uint64_t entity_a = 0, entity_b = 0;
            std::vector<std::uint8_t> data_a, data_b;
            float numbers_a[13] = {}, numbers_b[13] = {};
            std::uint8_t enabled_a = 0, enabled_b = 0, asleep_a = 0, asleep_b = 0;
            if (!ra.pod(entity_a) || !rb.pod(entity_b) || entity_a != entity_b ||
                !ra.bytes(data_a, 0, 100000) || !rb.bytes(data_b, 0, 100000) || data_a != data_b ||
                !ra.pod(numbers_a) || !rb.pod(numbers_b) || !ra.pod(enabled_a) || !rb.pod(enabled_b) ||
                enabled_a != enabled_b || !ra.pod(asleep_a) || !rb.pod(asleep_b) || asleep_a != asleep_b) {
                std::cout << "  save-roundtrip agree: fabricated object " << i << " differs\n";
                return false;
            }
            for (int j = 0; j < 13; ++j) {
                if (!closef(numbers_a[j], numbers_b[j])) {
                    std::cout << "  save-roundtrip agree: fabricated object " << i << " number " << j
                              << " differs\n";
                    return false;
                }
            }
        }
    }
    if (!ra.done() || !rb.done()) {
        std::cout << "  save-roundtrip agree: trailing bytes\n";
        return false;
    }
    return true;
}

// One leg: rig the checkpoint in A, save, die and restore in A; load the
// save in a fresh world B and restore; A and B must agree. A dies by the
// staged launch (straight up, down onto the same spot): a walk-off
// re-commits the checkpoint on the way out, so A would restore to a
// checkpoint B never saw, and the two would disagree on the checkpoint
// position for no fault of the save. The probe shows the walk-offs kill at
// every height; the launch is the deterministic death the comparison needs.
void roundtrip_leg(const char *leg, const InitialSpawn spawn, bool (*rig)(Simulation &),
                   const Vector3 launch_offset = {0.0, 0.0, 0.0}) {
    Simulation a(spawn);
    require(rig(a), leg);
    const Bytes saved = a.save_game();
    std::cout << "  save-roundtrip " << leg << ": rigged, save_bytes=" << saved.size() << '\n';
    require(die_by_launch(a, launch_offset), leg);
    // The corpse out before anything else: a death leaves one where the player
    // died, a load clears them, and the corpse shoves the restored player's
    // settle by millimetres. The comparison is the saved world, and a
    // relaunch has no corpse in it.
    a.debug_clear_corpses();
    Simulation b(spawn);
    require(b.load_game(saved.data(), saved.size()), leg);
    // Compare the re-saves as restored and loaded, before either world steps:
    // the solver's warm-start data is not saved, so a settled world lets its
    // noise amplify into a sliding body's position. The save must agree
    // exactly as the two paths leave it.
    const Bytes a_resaved = a.save_game();
    const Bytes b_resaved = b.save_game();
    std::cout << "  save-roundtrip " << leg << ": A died and restored, deaths=" << a.snapshot().death_count
              << " cp_y=" << a.snapshot().checkpoint_position.y << " B deaths=" << b.snapshot().death_count
              << " cp_y=" << b.snapshot().checkpoint_position.y << '\n';
    require(saves_agree(a_resaved, b_resaved), leg);
    // The Well A shackle (entity 2002) is the canary for the rig: its
    // rotation is set by the rope, so print it as restored/loaded, before
    // the settle lets the rope pull it, and as the file itself holds it.
    for (std::uint32_t body = 0; body < a.kit_body_count(); ++body) {
        if (a.kit_body_entity(body) == 2002) {
            const auto aq = a.kit_body_rotation(body), bq = b.kit_body_rotation(body);
            std::cout << "  save-roundtrip " << leg << ": shackle live at re-save A rot=(" << aq.x << "," << aq.y
                      << "," << aq.z << "," << aq.w << ") B rot=(" << bq.x << "," << bq.y << "," << bq.z << ","
                      << bq.w << ")\n";
            break;
        }
    }
    {
        scraperx::sim::detail::SaveReader r(saved.data(), saved.size());
        std::uint64_t key = 0, tick = 0, commits = 0, deaths = 0, carrying = 0;
        JPH::RVec3 pos;
        std::uint8_t crouched = 0;
        (void)r.pod(key);
        (void)r.pod(tick);
        (void)r.pod(pos);
        (void)r.pod(crouched);
        (void)r.pod(commits);
        (void)r.pod(deaths);
        (void)r.pod(carrying);
        std::vector<std::uint64_t> entities;
        if (r.list_up_to(entities, 100000)) {
            std::uint32_t count = 0;
            if (r.pod(count)) {
                for (std::uint32_t i = 0; i < count && i < entities.size(); ++i) {
                    scraperx::sim::kit::Kit::BodyState state{};
                    if (!r.pod(state)) break;
                    if (entities[i] == 2002) {
                        std::cout << "  save-roundtrip " << leg << ": shackle in the file rot=("
                                  << state.rotation.GetX() << "," << state.rotation.GetY() << ","
                                  << state.rotation.GetZ() << "," << state.rotation.GetW() << ") enabled="
                                  << static_cast<int>(state.enabled) << " asleep=" << static_cast<int>(state.asleep)
                                  << "\n";
                        break;
                    }
                }
            }
        }
    }
    // Then the behavioural check: both settle the same seconds and the live
    // worlds must agree to a centimetre.
    (void)a.advance_frame(1.0);
    (void)b.advance_frame(1.0);
    if (a.kit_body_count() > 830) {
        const std::uint32_t last = a.kit_body_count() - 1;
        const auto apos = a.kit_body_position(last), bpos = b.kit_body_position(last);
        const auto arot = a.kit_body_rotation(last), brot = b.kit_body_rotation(last);
        std::cout << "  save-roundtrip " << leg << ": carried body " << last << " A pos=(" << apos.x << ","
                  << apos.y << "," << apos.z << ") rot=(" << arot.x << "," << arot.y << "," << arot.z
                  << "," << arot.w << ") B pos=(" << bpos.x << "," << bpos.y << "," << bpos.z
                  << ") rot=(" << brot.x << "," << brot.y << "," << brot.z << "," << brot.w << ")\n";
    }
    require_same_live(a, b, leg);
    std::cout << "  save-roundtrip " << leg << ": checkpoint_y=" << a.snapshot().checkpoint_position.y
              << " save_bytes=" << saved.size() << '\n';
}

bool die_walk_east(Simulation &s) { return try_walkoff(s, 0.6, 0.0, 10.0) || die_by_launch(s); }
bool die_walk_south(Simulation &s) { return try_walkoff(s, 0.0, -0.6, 10.0) || die_by_launch(s); }

// The yard leg rigs with a keg, which needs its hull file.
bool rig_yard_with_hull(Simulation &s) {
    static const Bytes hull = read_file(std::string(SCRAPERX_SOURCE_DIR) + "/godot/tests/fabricated/keg.hull");
    return rig_yard(s, hull);
}

// ---- the bad files -----------------------------------------------------------
// A save cut short, flipped, or from an older format must load whole or
// fall back to a clean start, with a reason, and never crash. The section
// offsets come from walking a real save with the production reader, in
// write_save order, so a truncation lands exactly on a section boundary.
struct SaveSections {
    std::size_t after_header = 0;
    std::size_t after_entities = 0;
    std::size_t after_bodies = 0;
    std::size_t after_ropes = 0;
    std::size_t after_piles = 0;
    std::size_t after_slingshot = 0;
    std::size_t after_swing = 0;
    std::size_t after_bay = 0;
    std::size_t after_vertical_controls = 0;
    std::size_t after_vertical_targets = 0;
    std::size_t after_grips = 0;
    std::size_t after_damage = 0;
    std::size_t after_matron = 0;
    std::size_t end = 0;
};

bool save_sections(const Bytes &bytes, SaveSections &out) {
    using scraperx::sim::detail::SaveReader;
    using BodyState = scraperx::sim::kit::Kit::BodyState;
    using Pile = scraperx::sim::kit::Kit::Pile;
    using AnchorIndex = scraperx::sim::kit::AnchorIndex;
    SaveReader r(bytes.data(), bytes.size());
    std::uint64_t key = 0, tick = 0, commits = 0, deaths = 0, carrying = 0;
    JPH::RVec3 pos;
    std::uint8_t crouched = 0;
    if (!r.pod(key) || !r.pod(tick) || !r.pod(pos) || !r.pod(crouched) || !r.pod(commits) || !r.pod(deaths) ||
        !r.pod(carrying)) {
        return false;
    }
    out.after_header = r.position();
    std::vector<std::uint64_t> entities;
    if (!r.list_up_to(entities, 100000)) {
        return false;
    }
    out.after_entities = r.position();
    std::vector<BodyState> bodies;
    if (!r.list_up_to(bodies, 100000)) {
        return false;
    }
    out.after_bodies = r.position();
    std::vector<AnchorIndex> anchors;
    std::vector<float> floats;
    std::vector<bool> bits;
    if (!r.list_up_to(anchors, 100000) || !r.list_up_to(floats, 100000) || !r.list_up_to(floats, 100000) ||
        !r.bits_up_to(bits, 100000) || !r.bits_up_to(bits, 100000) || !r.bits_up_to(bits, 100000)) {
        return false;
    }
    out.after_ropes = r.position();
    std::vector<Pile> piles;
    if (!r.list_up_to(piles, 256) || !r.list_up_to(floats, 100000) || !r.list_up_to(floats, 100000) ||
        !r.list_up_to(floats, 100000)) {
        return false;
    }
    float drained = 0.0F;
    if (!r.pod(drained)) {
        return false;
    }
    out.after_piles = r.position();
    scraperx::sim::Slingshot::State slingshot{};
    if (!r.pod(slingshot)) {
        return false;
    }
    out.after_slingshot = r.position();
    scraperx::sim::Swing::State swing{};
    if (!r.pod(swing)) {
        return false;
    }
    out.after_swing = r.position();
    scraperx::sim::Bay::State bay{};
    if (!r.pod(bay)) {
        return false;
    }
    out.after_bay = r.position();
    std::uint32_t controls = 0;
    if (!r.pod(controls)) {
        return false;
    }
    for (std::uint32_t i = 0; i < controls; ++i) {
        scraperx::sim::vertical::Command command{};
        if (!r.pod(command) || !r.list_up_to(floats, 100000)) {
            return false;
        }
    }
    out.after_vertical_controls = r.position();
    if (!r.list_up_to(floats, 100000)) {
        return false;
    }
    out.after_vertical_targets = r.position();
    if (!r.list_up_to(floats, 100000) || !r.list_up_to(floats, 100000)) {
        return false;
    }
    out.after_grips = r.position();
    // The damage mark and its three fields, then the matron-state mark and
    // its six, then her injury mark and its eighteen, then the fabricated
    // mark if the save holds any.
    std::uint32_t mark = 0;
    if (r.done()) {
        out.after_damage = out.after_matron = out.end = r.position();
        return true;
    }
    if (!r.pod(mark) || mark != 0x314D414DU) {
        return false;
    }
    float damage_ns = 0.0F, player_damage_ns = 0.0F;
    std::uint8_t dead = 0;
    if (!r.pod(damage_ns) || !r.pod(player_damage_ns) || !r.pod(dead)) {
        return false;
    }
    out.after_damage = r.position();
    if (r.done()) {
        out.after_matron = out.end = r.position();
        return true;
    }
    // Her state (MAT1, only when the save holds it) and her per-region
    // injury (INJ1, J-059 #343 fix), each under its own mark, in write_save
    // order; a save made before either ends at the damage, and either
    // absent the next mark is the fabricated objects'.
    while (!r.done()) {
        if (!r.peek(mark)) {
            return false;
        }
        if (mark == 0x3154414DU) {
            std::uint8_t state = 0, stage = 0;
            float strain = 0.0F, rage = 0.0F, exposure = 0.0F;
            std::uint64_t handled = 0;
            if (!r.pod(mark) || !r.pod(state) || !r.pod(strain) || !r.pod(rage) || !r.pod(stage) ||
                !r.pod(exposure) || !r.pod(handled)) {
                return false;
            }
        } else if (mark == 0x314A4E49U) {
            float region[9] = {}, event[9] = {};
            if (!r.pod(mark) || !r.pod(region) || !r.pod(event)) {
                return false;
            }
        } else {
            break;
        }
    }
    out.after_matron = r.position();
    out.end = bytes.size();
    return true;
}

void run_save_bad_files() {
    // A plain yard save: no keg, no fabricated section, so the file ends
    // at her injury.
    Simulation source(InitialSpawn::ExteriorGrade);
    (void)source.advance_frame(1.0);
    const Bytes saved = source.save_game();
    SaveSections sec;
    require(save_sections(saved, sec), "bad files: the section walk must read a real save");
    require(sec.end == saved.size(), "bad files: the walk must end at the file's end");
    std::uint32_t refused = 0, whole = 0;
    const auto expect_refuse = [&](const char *name, const Bytes &bytes, const char *reason) {
        Simulation w(InitialSpawn::ExteriorGrade);
        (void)w.advance_frame(1.0);
        const std::vector<std::uint64_t> before = world_bits(w);
        require(!w.load_game(bytes.data(), bytes.size()), name);
        require(world_bits(w) == before, name);
        require(!w.save_reject_reason().empty(), name);
        if (reason != nullptr) {
            require(w.save_reject_reason() == reason, name);
        }
        ++refused;
    };
    const auto expect_load = [&](const char *name, const Bytes &bytes) {
        Simulation w(InitialSpawn::ExteriorGrade);
        require(w.load_game(bytes.data(), bytes.size()), name);
        require(w.save_reject_reason().empty(), name);
        ++whole;
    };
    const auto truncated = [&](const std::size_t n) {
        return Bytes(saved.begin(), saved.begin() + static_cast<std::ptrdiff_t>(n));
    };
    // Cut short at every section boundary: a required section missing
    // refuses; an optional one missing loads whole with its defaults.
    struct Cut {
        const char *name;
        std::size_t at;
        bool loads;
        const char *reason;
    };
    const Cut cuts[] = {
        {"cut header", sec.after_header, false, "checkpoint bodies"},
        {"cut entities", sec.after_entities, false, "checkpoint bodies"},
        {"cut bodies", sec.after_bodies, false, "checkpoint bodies"},
        {"cut ropes", sec.after_ropes, false, "checkpoint bodies"},
        {"cut piles", sec.after_piles, false, "checkpoint bodies"},
        {"cut slingshot", sec.after_slingshot, false, "checkpoint bodies"},
        {"cut swing", sec.after_swing, false, "checkpoint bodies"},
        {"cut bay", sec.after_bay, false, "vertical controls"},
        {"cut vertical controls", sec.after_vertical_controls, false, "vertical targets"},
        {"cut vertical targets", sec.after_vertical_targets, true, nullptr},
        {"cut grips", sec.after_grips, true, nullptr},
        {"cut damage", sec.after_damage, true, nullptr},
        {"cut matron", sec.after_matron, true, nullptr},
    };
    for (const Cut &cut : cuts) {
        if (cut.loads) {
            expect_load(cut.name, truncated(cut.at));
        } else {
            expect_refuse(cut.name, truncated(cut.at), cut.reason);
        }
    }
    // One byte short of a boundary cuts the section itself: always refused.
    const std::size_t short_ats[] = {sec.after_bodies, sec.after_grips, sec.after_damage, sec.after_matron};
    for (const std::size_t at : short_ats) {
        expect_refuse("cut one byte short", truncated(at - 1), nullptr);
    }
    // Flipped bytes: structural bytes refuse with the section named; value
    // bytes load faithfully, corruption and all.
    {
        Bytes bad = saved;
        bad[0] ^= 0xFFU;
        expect_refuse("flipped key", bad, "format key");
    }
    {
        Bytes bad = saved;
        // The entities count is the u32 at 57 (after key, tick, position,
        // crouched, commits, deaths, carrying).
        std::uint32_t count = 0;
        std::memcpy(&count, bad.data() + 57, sizeof(count));
        ++count;
        std::memcpy(bad.data() + 57, &count, sizeof(count));
        expect_refuse("entities count +1", bad, "checkpoint bodies");
    }
    {
        Bytes bad = saved;
        // The dead byte is the last byte of the damage section.
        bad[sec.after_damage - 1] = 2;
        expect_refuse("dead byte 2", bad, "matron damage");
    }
    {
        Bytes bad = saved;
        // The checkpoint position's x is the float at 16: not finite.
        const float nan = std::numeric_limits<float>::quiet_NaN();
        std::memcpy(bad.data() + 16, &nan, sizeof(nan));
        expect_refuse("checkpoint position NaN", bad, "checkpoint position");
    }
    {
        // A body velocity is a value, not a structure: the loader is a
        // faithful bijection, so the corrupted value loads as written.
        // Checked on the re-save, not the live body: a parked body's
        // velocity is not a live quantity, but the checkpoint keeps it,
        // and the re-save writes the checkpoint.
        Bytes bad = saved;
        // A body's linear velocity x: bodies start after the entities
        // list, a u32 count, then position (16) + rotation (16) in each
        // 80-byte BodyState.
        const std::size_t at = sec.after_entities + 4 + 10 * 80 + 32;
        const float huge = 1.0e30F;
        std::memcpy(bad.data() + at, &huge, sizeof(huge));
        Simulation w(InitialSpawn::ExteriorGrade);
        require(w.load_game(bad.data(), bad.size()), "body velocity 1e30: the load must succeed");
        const Bytes resaved = w.save_game();
        float back = 0.0F;
        std::memcpy(&back, resaved.data() + at, sizeof(back));
        require(back == 1.0e30F, "body velocity 1e30: the value loads as written");
        ++whole;
    }
    // A wrong format key, a trailing byte, and the empty and one-byte files.
    {
        Bytes bad = saved;
        std::memset(bad.data(), 0, 8);
        expect_refuse("wrong key", bad, "format key");
    }
    {
        Bytes bad = saved;
        bad.push_back(0);
        // One byte past the matron state is read as the fabricated
        // section's mark, and it is not one: the file is refused for the
        // section it pretends to open, not for trailing bytes. The
        // trailing-bytes reason is proven below, on a save that holds a
        // fabricated section and so ends cleanly after it.
        expect_refuse("trailing byte", bad, "fabricated objects");
    }
    // The empty file refuses; its reason is "empty" when the buffer has no
    // pointer at all and "format key" when it has one but no key to read.
    // Either names the failure; the one-byte file pins "format key".
    expect_refuse("empty file", Bytes{}, nullptr);
    expect_refuse("one-byte file", Bytes{0x53}, "format key");
    // A different world body list: one fewer entity and body state (a
    // prefix of this world's) loads whole, the last body starting as built.
    {
        Bytes spliced;
        // Header + entities count - 1 + all but the last entity.
        std::uint32_t entities_count = 0;
        std::memcpy(&entities_count, saved.data() + 57, sizeof(entities_count));
        const std::size_t entities_bytes = sec.after_entities - 57 - 4;
        spliced.insert(spliced.end(), saved.begin(), saved.begin() + static_cast<std::ptrdiff_t>(57));
        const std::uint32_t fewer = entities_count - 1;
        spliced.insert(spliced.end(), reinterpret_cast<const std::uint8_t *>(&fewer),
                       reinterpret_cast<const std::uint8_t *>(&fewer) + sizeof(fewer));
        spliced.insert(spliced.end(), saved.begin() + static_cast<std::ptrdiff_t>(57 + 4),
                       saved.begin() + static_cast<std::ptrdiff_t>(57 + 4 + entities_bytes - 8));
        // Bodies count - 1 + all but the last BodyState (80 bytes).
        std::uint32_t bodies_count = 0;
        std::memcpy(&bodies_count, saved.data() + sec.after_entities, sizeof(bodies_count));
        const std::uint32_t fewer_bodies = bodies_count - 1;
        spliced.insert(spliced.end(), reinterpret_cast<const std::uint8_t *>(&fewer_bodies),
                       reinterpret_cast<const std::uint8_t *>(&fewer_bodies) + sizeof(fewer_bodies));
        const std::size_t bodies_bytes = sec.after_bodies - sec.after_entities - 4;
        spliced.insert(spliced.end(), saved.begin() + static_cast<std::ptrdiff_t>(sec.after_entities + 4),
                       saved.begin() + static_cast<std::ptrdiff_t>(sec.after_entities + 4 + bodies_bytes - 80));
        // The rest of the file unchanged.
        spliced.insert(spliced.end(), saved.begin() + static_cast<std::ptrdiff_t>(sec.after_bodies), saved.end());
        Simulation w(InitialSpawn::ExteriorGrade);
        require(w.load_game(spliced.data(), spliced.size()), "spliced body list: the load must succeed");
        Simulation fresh(InitialSpawn::ExteriorGrade);
        const std::uint32_t last = fresh.kit_body_count() - 1;
        const auto apos = w.kit_body_position(last), bpos = fresh.kit_body_position(last);
        require(apos.x == bpos.x && apos.y == bpos.y && apos.z == bpos.z,
                "spliced body list: the last body starts as built");
        ++whole;
    }
    // An older format: the same save cut after the damage (before MAT1)
    // loads with her damage kept and her state at its defaults.
    {
        Simulation w(InitialSpawn::ExteriorGrade);
        require(w.load_game(truncated(sec.after_damage).data(), sec.after_damage),
                "old format after damage: the load must succeed");
        const auto m = w.matron_state();
        require(m.stage == 0 && m.rage == 0.0, "old format after damage: her state is the default");
        ++whole;
    }
    // Trailing bytes, properly: a save that holds a fabricated section
    // ends cleanly after it, so a byte past that is trailing bytes.
    {
        static const Bytes hull = read_file(std::string(SCRAPERX_SOURCE_DIR) + "/godot/tests/fabricated/keg.hull");
        Simulation keg(InitialSpawn::ExteriorGrade);
        require(rig_yard(keg, hull), "trailing bytes: the keg must be carried");
        const Bytes keg_saved = keg.save_game();
        Bytes bad = keg_saved;
        bad.push_back(0);
        Simulation w(InitialSpawn::ExteriorGrade);
        (void)w.advance_frame(1.0);
        const std::vector<std::uint64_t> before = world_bits(w);
        require(!w.load_game(bad.data(), bad.size()), "trailing bytes: the load must refuse");
        require(world_bits(w) == before, "trailing bytes: the world is unchanged");
        require(w.save_reject_reason() == "trailing bytes", "trailing bytes: the reason names it");
        ++refused;
    }
    std::cout << "  save-roundtrip bad files: refused=" << refused << " whole=" << whole << '\n';
}

void run_save_roundtrip() {
    roundtrip_leg("yard", InitialSpawn::ExteriorGrade, rig_yard_with_hull);
    roundtrip_leg("154", InitialSpawn::Deck154, rig_154);
    // The 221 death launches south, clear of the swing: straight up and
    // back down lands on the swing's seat, which takes the fall.
    roundtrip_leg("221", InitialSpawn::Ring220North, rig_221, {0.0, 0.0, -10.0});
    roundtrip_leg("340", InitialSpawn::PlateTop, rig_340);
    roundtrip_leg("484", InitialSpawn::Ring484North, rig_484);
    run_save_bad_files();
    std::cout << "PASS scraperx_sim save-roundtrip: legs=5 bad_files=1\n";
}

// ---- the probe ---------------------------------------------------------------
// The plain A/B, no keg, no rigging: does a death restore and a file load
// agree when nothing is carried? This discriminates the corpse (a death
// leaves one at the checkpoint; a load clears them) from the carried keg.
void run_save_plain_probe() {
    Simulation a(InitialSpawn::ExteriorGrade);
    (void)a.advance_frame(1.0);
    const Bytes saved = a.save_game();
    require(die_by_launch(a), "plain probe: the staged fall must be lethal");
    a.debug_clear_corpses();
    Simulation b(InitialSpawn::ExteriorGrade);
    require(b.load_game(saved.data(), saved.size()), "plain probe: the save must load");
    const auto acp = a.snapshot().checkpoint_position;
    const auto bcp = b.snapshot().checkpoint_position;
    std::cout << "  plain probe: A cp=(" << acp.x << "," << acp.y << "," << acp.z << ") B cp=(" << bcp.x
              << "," << bcp.y << "," << bcp.z << ") A deaths=" << a.snapshot().death_count
              << " B deaths=" << b.snapshot().death_count << '\n';
    // As restored and loaded, before either world steps (see roundtrip_leg).
    const Bytes a_resaved = a.save_game();
    const Bytes b_resaved = b.save_game();
    require(saves_agree(a_resaved, b_resaved), "plain probe: the re-saves must match");
    std::cout << "PASS scraperx_sim save-plain-probe\n";
}

void run_save_roundtrip_probe() {
    Simulation plain(InitialSpawn::ExteriorGrade);
    (void)plain.advance_frame(1.0);
    std::cout << "probe: kit_body_count=" << plain.kit_body_count()
              << " plain_yard_save_bytes=" << plain.save_game().size() << '\n';
    {
        Simulation s(InitialSpawn::ExteriorGrade);
        const Bytes hull = read_file(std::string(SCRAPERX_SOURCE_DIR) + "/godot/tests/fabricated/keg.hull");
        require(rig_yard(s, hull), "probe yard: the keg carried");
        print_state("yard keg carried", s);
        std::cout << "  probe yard: save_bytes_with_keg=" << s.save_game().size() << '\n';
        require(die_by_launch(s), "probe yard: the staged fall must be lethal");
        (void)s.advance_frame(1.0);
        print_state("yard after death restore", s);
        // The slingshot, boarded and drawn: does a seated rider still commit?
        Simulation shot(InitialSpawn::ExteriorGrade);
        (void)shot.advance_frame(1.0);
        require(walk_to(shot, 6.0, -40.0, 20.0) && walk_to(shot, 6.0, -57.6, 20.0) &&
                    walk_to(shot, -3.0, -57.6, 10.0, 0.1) && walk_to(shot, -3.0, -55.0, 6.0, 0.08),
                "probe yard: the walk to the slingshot pouch");
        (void)shot.advance_frame(0.6);
        require(shot.slingshot_state().station_available, "probe yard: the pouch offers ENTER");
        const std::uint64_t commits_before_board = shot.snapshot().checkpoint_commit_count;
        (void)shot.set_slingshot_input(0.0, 0.0, 1.4311699866353502);
        (void)shot.request_slingshot_action();
        (void)shot.advance_frame(0.2);
        print_state("yard slingshot boarded", shot);
        std::cout << "  probe yard: seated=" << int(shot.slingshot_state().seated)
                  << " commits_before_board=" << commits_before_board
                  << " commits_after_board=" << shot.snapshot().checkpoint_commit_count << '\n';
        (void)shot.set_slingshot_input(1.0, 0.0, 1.4311699866353502);
        (void)shot.advance_frame(1.0);
        (void)shot.set_slingshot_input(0.0, 0.0, 1.4311699866353502);
        (void)shot.advance_frame(0.1);
        print_state("yard slingshot drawn", shot);
        std::cout << "  probe yard: draw_m=" << shot.slingshot_state().draw_m
                  << " energy_j=" << shot.slingshot_state().energy_j << '\n';
    }
    {
        Simulation s(InitialSpawn::Deck154);
        require(rig_154(s), "probe 154: board and rig Stage A");
        print_state("154 rigged", s);
        std::cout << "  probe 154: save_bytes=" << s.save_game().size() << '\n';
        require(die_walk_east(s), "probe 154: the death must be lethal");
        (void)s.advance_frame(1.0);
        print_state("154 after death restore", s);
    }
    {
        Simulation s(InitialSpawn::Ring220North);
        require(rig_221(s), "probe 221: the swing ridden to the 242 ring");
        print_state("221 on the 242 ring", s);
        std::cout << "  probe 221: held_at_top=" << int(s.swing_state().held_at_top)
                  << " tooth=" << s.swing_state().tooth << " seat_angle=" << s.swing_state().seat_angle_rad
                  << " save_bytes=" << s.save_game().size() << '\n';
        require(die_walk_south(s), "probe 221: the death must be lethal");
        (void)s.advance_frame(1.0);
        print_state("221 after death restore", s);
    }
    {
        Simulation s(InitialSpawn::PlateTop);
        require(rig_340(s), "probe 340: board G and pull its pin");
        print_state("340 pin pulled", s);
        std::cout << "  probe 340: tower_latched=" << int(s.shop_state().g_tower_latched)
                  << " platform_travel=" << s.shop_state().g_platform_travel
                  << " save_bytes=" << s.save_game().size() << '\n';
        require(die_walk_south(s) || die_by_launch(s), "probe 340: the death must be lethal");
        (void)s.advance_frame(1.0);
        print_state("340 after death restore", s);
    }
    {
        Simulation s(InitialSpawn::Ring484North);
        require(rig_484(s), "probe 484: J ridden to the 528 ring");
        print_state("484 at the top of J", s);
        const Vector3 traveler_at = s.kit_body_position(s.kit_body_index(Simulation::kCraneJTravelerEntityId));
        std::cout << "  probe 484: traveler_travel=" << s.crane_state().j_traveler_travel << " traveler_at=("
                  << traveler_at.x << "," << traveler_at.y << "," << traveler_at.z << ")"
                  << " save_bytes=" << s.save_game().size() << '\n';
        const bool stepped_off = walk_to(s, 0.0, -128.8, 8.0);
        (void)s.advance_frame(1.0);
        print_state("484 after stepping off", s);
        std::cout << "  probe 484: stepped_off=" << int(stepped_off) << '\n';
        require(die_walk_south(s) || die_by_launch(s), "probe 484: the death must be lethal");
        (void)s.advance_frame(1.0);
        print_state("484 after death restore", s);
    }
    std::cout << "PASS scraperx_sim save-probe\n";
}

}  // namespace

SCRAPERX_GROUP("save_roundtrip", 1090, run_save_roundtrip);
SCRAPERX_SLICE("save-roundtrip", run_save_roundtrip);
SCRAPERX_SLICE("save-probe", run_save_roundtrip_probe);
SCRAPERX_SLICE("save-plain-probe", run_save_plain_probe);
