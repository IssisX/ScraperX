// J-038 slice 1: unit tests pin every tier boundary, the radius
// formula and the 8-cap merge; integration tests mirror the probe's
// forklift-ram and throw cases and assert the dent state they must give.
#include "sim/forklift.hpp"
#include "sim/loose_stock.hpp"
#include "sim/stock_damage.hpp"
#include "sim_tests.hpp"

#include <cmath>
#include <fstream>
#include <iostream>

#ifndef SCRAPERX_SOURCE_DIR
#error "SCRAPERX_SOURCE_DIR must name the repository (CMakeLists.txt sets it)"
#endif

using scraperx::sim::Forklift;
using scraperx::sim::LooseStock;
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
using scraperx::sim::Vector3;
using namespace scraperx::sim::stock_damage;

namespace {

std::uint32_t piece(const Simulation &s, const std::uint32_t i) {
    return s.kit_body_index(LooseStock::kFirstEntity + i);
}

void put(Simulation &s, const std::uint32_t body, const Vector3 at) {
    s.debug_launch_kit_body(body, at, {0.0, 0.0, 0.0});
}

// The forklift test's board path, copied: round the beam stack's west
// end, onto the truck's floor from its west side, to the wheel.
bool board(Simulation &s) {
    if (!(walk_to(s, 30.0, 10.0, 30.0) && walk_to(s, 36.4, 16.35, 20.0) &&
          walk_to(s, Forklift::kParkX, 16.35, 8.0, 0.1))) {
        return false;
    }
    (void)s.set_facing(0.0, 1.0);
    (void)s.advance_frame(0.5);
    return s.forklift_state().at_controls;
}

std::uint32_t first_of_kind(const Simulation &s, const LooseStock::Kind kind) {
    for (std::uint32_t i = 0; i < s.loose_stock_count(); ++i) {
        if (s.loose_stock_piece(i).kind == static_cast<std::uint8_t>(kind)) {
            return i;
        }
    }
    return 0;
}

// The forklift lane's container at (39, 70): clear above it, so a
// launch straight up and a set-down both have somewhere to go.
std::uint32_t lane_container(const Simulation &s) {
    for (std::uint32_t i = 0; i < s.loose_stock_count(); ++i) {
        const auto p = s.kit_body_position(piece(s, i));
        if (s.loose_stock_piece(i).kind == static_cast<std::uint8_t>(LooseStock::Kind::Container) &&
            std::abs(p.x - 39.0) < 0.1 && std::abs(p.z - 70.0) < 0.1) {
            return i;
        }
    }
    return 0;
}

enum class Target { Container, Drum, Crate };

// The probe's forklift case: the drums' pallet moved 40 m up the lane,
// the target stood in the lane at z = 30, then `pace` until 0.2 s after contact.
std::uint64_t forklift_into(Simulation &s, const Target what, const double pace) {
    (void)s.advance_frame(1.0);
    require(board(s) && s.request_forklift_action(), "stock damage: aboard, at the wheel, the controls taken");
    (void)s.advance_frame(0.2);
    const std::uint32_t pallet = first_of_kind(s, LooseStock::Kind::Pallet);
    for (std::uint32_t i = pallet; i < pallet + 15; ++i) {
        const auto b = piece(s, i);
        auto p = s.kit_body_position(b);
        p.z += 40.0;
        put(s, b, p);
    }
    std::uint32_t target = 0;
    std::uint64_t entity = 0;
    if (what == Target::Container) {
        for (std::uint32_t i = 0; i < s.loose_stock_count(); ++i) {
            const auto p = s.kit_body_position(piece(s, i));
            if (s.loose_stock_piece(i).kind == static_cast<std::uint8_t>(LooseStock::Kind::Container) &&
                std::abs(p.x - 39.0) < 0.1 && std::abs(p.z - 70.0) < 0.1) {
                target = piece(s, i);
                entity = LooseStock::kFirstEntity + i;
            }
        }
        put(s, target, {38.0, 1.31, 30.0});
    } else if (what == Target::Drum) {
        target = piece(s, pallet + 1);
        entity = LooseStock::kFirstEntity + pallet + 1;
        put(s, target, {38.0, 0.47, 30.0});
    } else {
        // Skid crate north of the catwalk's east end: origin on the ground, rests at y ~ 0; 0.46 would drop and break it pre-ram.
        target = piece(s, LooseStock::kDeckCrateNorth);
        entity = LooseStock::kFirstEntity + LooseStock::kDeckCrateNorth;
        put(s, target, {38.0, 0.01, 30.0});
    }
    (void)s.advance_frame(1.5);
    (void)s.set_facing(0.0, 1.0);
    const auto start = s.kit_body_position(target);
    bool contact = false;
    int hold = 0;
    for (int t = 0; t < 30 * static_cast<int>(Simulation::kTickRateHz); ++t) {
        (void)s.set_move_input(0.0, pace);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto now = s.kit_body_position(target);
        if (!contact && std::hypot(now.x - start.x, now.z - start.z) > 0.05) {
            contact = true;
        }
        if (contact && ++hold >= static_cast<int>(0.2 * Simulation::kTickRateHz)) {
            break;
        }
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.advance_frame(1.0);  // the hit window closes after 6 quiet ticks
    require(contact, "stock damage: the truck reached its target");
    return entity;
}

// The probe's throw case: the toolbox sent level at 7.5 m/s from 1 m short.
std::uint64_t throw_at(Simulation &s, const Target what) {
    (void)s.advance_frame(1.0);
    const std::uint32_t n = s.loose_stock_count();
    // The first of her throwables, by the table's own begin: the
    // throwables are no longer last (the east/west stock takes its
    // table places after them, J-014 #216), so `n - kMatronThrowables`
    // now names an east/west drum, not the toolbox. The begin
    // accessor is the table's own answer to which piece is first.
    const std::uint32_t toolbox = piece(s, s.loose_stock_throwables_begin());
    std::uint32_t target = 0;
    std::uint64_t entity = 0;
    double rest_y = 0.0, half = 0.0, tz = 64.0, launch_y = 0.0;
    if (what == Target::Drum) {
        // The last east/west drum: the upper stock trunk added after
        // the east/west stock sits last in the table now, so the
        // table's last piece is no longer a drum (J-014). The
        // east/west drums are the ones this throw was measured
        // against, so the range ends where the upper stock begins.
        const std::uint32_t ew_begin = s.loose_stock_east_west_begin();
        const std::uint32_t up_begin = s.loose_stock_upper_begin();
        std::uint32_t drum = 0;
        bool found = false;
        for (std::uint32_t i = up_begin; i > ew_begin;) {
            --i;
            if (s.loose_stock_piece(i).kind == static_cast<std::uint8_t>(LooseStock::Kind::Drum)) {
                drum = i;
                found = true;
                break;
            }
        }
        require(found, "stock damage: an east/west drum exists to throw at");
        target = piece(s, drum);
        entity = LooseStock::kFirstEntity + drum;
        rest_y = 0.47;
        launch_y = 0.47;
        half = 0.30;
    } else if (what == Target::Crate) {
        // The same skid crate, put at y ~ 0 (its box still spans
        // y 0.13..1.01, so the throw at y 0.46 meets it).
        target = piece(s, LooseStock::kDeckCrateNorth);
        entity = LooseStock::kFirstEntity + LooseStock::kDeckCrateNorth;
        rest_y = 0.01;
        launch_y = 0.46;
        half = 0.48;
    } else {
        for (std::uint32_t i = 0; i < n; ++i) {
            const auto p = s.kit_body_position(piece(s, i));
            if (s.loose_stock_piece(i).kind == static_cast<std::uint8_t>(LooseStock::Kind::Container) &&
                std::abs(p.x - 1.5) < 0.1 && std::abs(p.z - 70.0) < 0.1) {
                target = piece(s, i);
                entity = LooseStock::kFirstEntity + i;
            }
        }
        tz = 70.0;
        half = 1.22;
    }
    if (what != Target::Container) {
        put(s, target, {1.5, rest_y, tz});
    }
    (void)s.advance_frame(1.5);
    launch_y = what == Target::Container ? 1.3 : launch_y;
    s.debug_launch_kit_body(toolbox, {1.5, launch_y, tz - half - 1.0}, {0.0, 0.0, 7.5});
    (void)s.advance_frame(2.0);
    return entity;
}

void check_dent(const DamageState &state, const Tier tier, const float lo, const float hi, const char *what) {
    require(state.dent_count >= 1 && !state.broken, what);
    const Dent &dent = state.dents[0];
    std::cout << "  stock damage: first dent tier " << static_cast<int>(dent.tier) << " depth " << dent.depth_m
              << " radius " << dent.radius_m << "\n";
    require(dent.tier == tier && dent.depth_m >= lo && dent.depth_m <= hi, what);
}

// The fabricated test's crowbar hull, the smallest signed object.
std::vector<std::uint8_t> crowbar_hull_bytes() {
    const std::string path = std::string(SCRAPERX_SOURCE_DIR) + "/godot/tests/fabricated/crowbar.hull";
    std::ifstream in(path, std::ios::binary);
    require(in.good(), "stock damage fabricated break: the crowbar hull file is readable");
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::uint64_t fabricate_crowbar(Simulation &s) {
    const std::vector<std::uint8_t> hull = crowbar_hull_bytes();
    scraperx::sim::FabricatedRefusal why = scraperx::sim::FabricatedRefusal::None;
    const std::uint64_t entity = s.add_fabricated(hull.data(), hull.size(), {9.0, 0.0, -25.0}, 0.0, &why);
    require(entity != 0 && why == scraperx::sim::FabricatedRefusal::None,
            "stock damage fabricated break: the crowbar is fabricated");
    return entity;
}

bool quat_near(const scraperx::sim::Quaternion a, const scraperx::sim::Quaternion b, const double tol) {
    const bool same = std::abs(a.x - b.x) < tol && std::abs(a.y - b.y) < tol && std::abs(a.z - b.z) < tol &&
                      std::abs(a.w - b.w) < tol;
    const bool flipped = std::abs(a.x + b.x) < tol && std::abs(a.y + b.y) < tol && std::abs(a.z + b.z) < tol &&
                         std::abs(a.w + b.w) < tol;
    return same || flipped;  // q and -q are the same rotation
}

// One fabricate-and-break save/load case: fabricate one crowbar and break
// the deck crate north, in the given order, and save/load at the timing the
// spawn gives. HighDrop falls, so no checkpoint can commit before the break
// and the save predates it. ExteriorGrade stands, so the break tick itself
// commits and the save follows it. The fourth P1 on #299 (comment
// 6708123804): kit_built_count() assumes every fabricated body sits at the
// end of the Kit, but spawn_break_pieces appends eight bodies after any
// fabricated ones, so the base checkpoint list takes a prefix that includes
// the fabricated object and drops the last piece, and write_fabricated reads
// past the checkpoint vector (or a piece's state). The fix is at the Kit
// order and save mapping: true Kit index order with explicit fabricated
// indices. This test pins the save/load path that carries both.
void fabricated_break_case(const scraperx::sim::InitialSpawn spawn, const bool fabricate_first,
                           const char *what) {
    const std::string prefix = std::string("stock damage fabricated break (") + what + "): ";
    Simulation s(spawn);
    if (spawn == scraperx::sim::InitialSpawn::ExteriorGrade) {
        (void)s.advance_frame(2.0);  // settle; commits happen
    }
    const std::uint32_t crate = piece(s, LooseStock::kDeckCrateNorth);
    const std::uint64_t entity = LooseStock::kFirstEntity + LooseStock::kDeckCrateNorth;
    const double mass = s.kit_body_mass(crate);
    const std::uint64_t commits_before = s.snapshot().checkpoint_commit_count;
    const std::uint32_t bodies_before = s.kit_body_count();
    std::uint64_t fabricated_entity = 0;
    if (fabricate_first) {
        fabricated_entity = fabricate_crowbar(s);
    }
    // The drop breaks the crate on landing, far past the 300 N.s line.
    const auto at = s.kit_body_position(crate);
    put(s, crate, {at.x, at.y + 3.0, at.z});
    (void)s.advance_frame(spawn == scraperx::sim::InitialSpawn::HighDrop ? 1.0 : 2.0);
    require(s.stock_damage(entity).broken, (prefix + "the crate breaks").c_str());
    require(s.kit_body_count() == bodies_before + (fabricate_first ? 1 : 0) + 8,
            (prefix + "the break adds eight pieces").c_str());
    if (spawn == scraperx::sim::InitialSpawn::HighDrop) {
        require(s.snapshot().checkpoint_commit_count == commits_before,
                (prefix + "no checkpoint commits while falling").c_str());
    } else {
        require(s.snapshot().checkpoint_commit_count > commits_before,
                (prefix + "a checkpoint commits after the break").c_str());
    }
    if (!fabricate_first) {
        fabricated_entity = fabricate_crowbar(s);
        if (spawn == scraperx::sim::InitialSpawn::ExteriorGrade) {
            (void)s.advance_frame(2.0);  // the crowbar settles; a commit takes it
        }
    }
    // The source's fabricated body and pieces, as the save must carry them.
    const std::uint32_t fabricated_body = s.fabricated_object(0).body;
    const auto fabricated_at = s.kit_body_position(fabricated_body);
    const auto fabricated_rot = s.kit_body_rotation(fabricated_body);
    const auto fabricated_vel = s.kit_body_velocity(fabricated_body);
    const double fabricated_mass = s.kit_body_mass(fabricated_body);
    const std::vector<std::uint8_t> fabricated_bytes = s.fabricated_data(0);
    std::array<Vector3, 8> piece_at{};
    std::array<Vector3, 8> piece_vel{};
    for (std::uint32_t k = 0; k < 8; ++k) {
        const std::uint32_t body = s.kit_body_index(piece_entity_for(entity, k));
        piece_at[k] = s.kit_body_position(body);
        piece_vel[k] = s.kit_body_velocity(body);
    }
    const std::uint32_t bodies_after = s.kit_body_count();
    const std::vector<std::uint8_t> bytes = s.save_game();
    Simulation t(spawn);
    require(t.load_game(bytes.data(), bytes.size()), (prefix + "the fresh world loads the save").c_str());
    // The fabricated object: the same entity, hull, transform and state.
    require(t.fabricated_count() == 1, (prefix + "one fabricated object loads").c_str());
    require(t.fabricated_object(0).entity == fabricated_entity,
            (prefix + "the fabricated entity matches").c_str());
    require(t.fabricated_data(0) == fabricated_bytes, (prefix + "the fabricated hull matches").c_str());
    const std::uint32_t loaded_body = t.fabricated_object(0).body;
    const auto loaded_at = t.kit_body_position(loaded_body);
    require(std::hypot(loaded_at.x - fabricated_at.x, loaded_at.y - fabricated_at.y,
                       loaded_at.z - fabricated_at.z) < 0.05,
            (prefix + "the fabricated position matches").c_str());
    require(quat_near(t.kit_body_rotation(loaded_body), fabricated_rot, 0.01),
            (prefix + "the fabricated rotation matches").c_str());
    const auto loaded_vel = t.kit_body_velocity(loaded_body);
    require(std::hypot(loaded_vel.x - fabricated_vel.x, loaded_vel.y - fabricated_vel.y,
                       loaded_vel.z - fabricated_vel.z) < 0.05,
            (prefix + "the fabricated velocity matches").c_str());
    require(std::abs(t.kit_body_mass(loaded_body) - fabricated_mass) < 0.01,
            (prefix + "the fabricated mass matches").c_str());
    // The pieces: all eight, once, with their state.
    require(t.kit_body_count() == bodies_after, (prefix + "the piece count matches").c_str());
    require(!t.kit_body_enabled(crate), (prefix + "the crate stays parked").c_str());
    require(t.stock_damage(entity).broken, (prefix + "the crate stays broken").c_str());
    for (std::uint32_t k = 0; k < 8; ++k) {
        const std::uint32_t body = t.kit_body_index(piece_entity_for(entity, k));
        require(body != 0xFFFFFFFFU, (prefix + "every piece exists").c_str());
        require(std::abs(t.kit_body_mass(body) - mass / 8.0) < 0.01,
                (prefix + "every piece keeps its mass share").c_str());
        const auto pp = t.kit_body_position(body);
        require(std::hypot(pp.x - piece_at[k].x, pp.y - piece_at[k].y, pp.z - piece_at[k].z) < 0.05,
                (prefix + "every piece keeps its position").c_str());
        const auto pv = t.kit_body_velocity(body);
        require(std::hypot(pv.x - piece_vel[k].x, pv.y - piece_vel[k].y, pv.z - piece_vel[k].z) < 0.05,
                (prefix + "every piece keeps its velocity").c_str());
    }
    // A later round trip holds.
    const std::vector<std::uint8_t> bytes2 = t.save_game();
    Simulation u(spawn);
    require(u.load_game(bytes2.data(), bytes2.size()), (prefix + "the round trip loads").c_str());
    require(u.fabricated_count() == 1 && u.fabricated_object(0).entity == fabricated_entity,
            (prefix + "the round trip keeps the fabricated object").c_str());
    require(u.kit_body_count() == bodies_after, (prefix + "the round trip keeps the piece count").c_str());
    for (std::uint32_t k = 0; k < 8; ++k) {
        require(u.kit_body_index(piece_entity_for(entity, k)) != 0xFFFFFFFFU,
                (prefix + "the round trip keeps every piece").c_str());
    }
}

// Two crates break in reverse entity order: the save loads. South has
// the higher entity (kDeckCrateSouth = kDeckCrateNorth + 1), so breaking
// it first lists the pieces in break order, not entity order — and the
// save must load into a fresh world and back into the world that broke
// them, both succeeding.
void reverse_break_save_case() {
    const std::string prefix = "stock damage reverse break save: ";
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(2.0);  // the commit lands; the crates are live
    const std::uint32_t bodies_before = s.kit_body_count();
    const std::uint64_t south_entity = LooseStock::kFirstEntity + LooseStock::kDeckCrateSouth;
    const std::uint64_t north_entity = LooseStock::kFirstEntity + LooseStock::kDeckCrateNorth;
    const std::uint32_t south = piece(s, LooseStock::kDeckCrateSouth);
    const std::uint32_t north = piece(s, LooseStock::kDeckCrateNorth);
    const double south_mass = s.kit_body_mass(south);
    const double north_mass = s.kit_body_mass(north);
    // South first: the higher entity breaks first.
    {
        const auto at = s.kit_body_position(south);
        put(s, south, {at.x, at.y + 3.0, at.z});
        (void)s.advance_frame(1.0);  // the fall, the landing, the break executes
    }
    require(s.stock_damage(south_entity).broken, (prefix + "the south crate breaks first").c_str());
    require(s.kit_body_count() == bodies_before + 8,
            (prefix + "the south break adds eight pieces").c_str());
    // Then north.
    {
        const auto at = s.kit_body_position(north);
        put(s, north, {at.x, at.y + 3.0, at.z});
        (void)s.advance_frame(1.0);  // the fall, the landing, the break executes
    }
    require(s.stock_damage(north_entity).broken, (prefix + "the north crate breaks second").c_str());
    require(s.kit_body_count() == bodies_before + 16,
            (prefix + "the north break adds eight more pieces").c_str());
    // Every piece's place and motion, both crates, as the save must carry them.
    std::array<Vector3, 16> piece_at{};
    std::array<Vector3, 16> piece_vel{};
    for (std::uint32_t k = 0; k < 8; ++k) {
        const std::uint32_t body = s.kit_body_index(piece_entity_for(south_entity, k));
        require(body != 0xFFFFFFFFU, (prefix + "every south piece exists").c_str());
        piece_at[k] = s.kit_body_position(body);
        piece_vel[k] = s.kit_body_velocity(body);
        require(std::abs(s.kit_body_mass(body) - south_mass / 8.0) < 0.01,
                (prefix + "every south piece keeps its mass share").c_str());
    }
    for (std::uint32_t k = 0; k < 8; ++k) {
        const std::uint32_t body = s.kit_body_index(piece_entity_for(north_entity, k));
        require(body != 0xFFFFFFFFU, (prefix + "every north piece exists").c_str());
        piece_at[8 + k] = s.kit_body_position(body);
        piece_vel[8 + k] = s.kit_body_velocity(body);
        require(std::abs(s.kit_body_mass(body) - north_mass / 8.0) < 0.01,
                (prefix + "every north piece keeps its mass share").c_str());
    }
    const std::uint32_t bodies_after = s.kit_body_count();
    const std::vector<std::uint8_t> bytes = s.save_game();
    // A fresh world loads it, pieces in place.
    {
        Simulation t(scraperx::sim::InitialSpawn::ExteriorGrade);
        require(t.load_game(bytes.data(), bytes.size()), (prefix + "the fresh world loads the save").c_str());
        require(t.stock_damage(south_entity).broken && t.stock_damage(north_entity).broken,
                (prefix + "both crates stay broken").c_str());
        require(t.kit_body_count() == bodies_after, (prefix + "the piece count matches").c_str());
        require(!t.kit_body_enabled(south) && !t.kit_body_enabled(north),
                (prefix + "both crates stay parked").c_str());
        for (std::uint32_t k = 0; k < 8; ++k) {
            const std::uint32_t body = t.kit_body_index(piece_entity_for(south_entity, k));
            require(body != 0xFFFFFFFFU, (prefix + "every south piece loads").c_str());
            const auto pp = t.kit_body_position(body);
            require(std::hypot(pp.x - piece_at[k].x, pp.y - piece_at[k].y, pp.z - piece_at[k].z) < 0.05,
                    (prefix + "every south piece keeps its position").c_str());
            const auto pv = t.kit_body_velocity(body);
            require(std::hypot(pv.x - piece_vel[k].x, pv.y - piece_vel[k].y, pv.z - piece_vel[k].z) < 0.05,
                    (prefix + "every south piece keeps its velocity").c_str());
        }
        for (std::uint32_t k = 0; k < 8; ++k) {
            const std::uint32_t body = t.kit_body_index(piece_entity_for(north_entity, k));
            require(body != 0xFFFFFFFFU, (prefix + "every north piece loads").c_str());
            const auto pp = t.kit_body_position(body);
            require(std::hypot(pp.x - piece_at[8 + k].x, pp.y - piece_at[8 + k].y,
                               pp.z - piece_at[8 + k].z) < 0.05,
                    (prefix + "every north piece keeps its position").c_str());
            const auto pv = t.kit_body_velocity(body);
            require(std::hypot(pv.x - piece_vel[8 + k].x, pv.y - piece_vel[8 + k].y,
                               pv.z - piece_vel[8 + k].z) < 0.05,
                    (prefix + "every north piece keeps its velocity").c_str());
        }
    }
    // The same world loads it back, making nothing twice.
    {
        require(s.load_game(bytes.data(), bytes.size()), (prefix + "the same world loads the save").c_str());
        require(s.kit_body_count() == bodies_after,
                (prefix + "the same world makes no second set of pieces").c_str());
        require(s.stock_damage(south_entity).broken && s.stock_damage(north_entity).broken,
                (prefix + "both crates stay broken in the same world").c_str());
        for (std::uint32_t k = 0; k < 8; ++k) {
            const std::uint32_t body = s.kit_body_index(piece_entity_for(south_entity, k));
            require(body != 0xFFFFFFFFU, (prefix + "every south piece stays").c_str());
            const auto pp = s.kit_body_position(body);
            require(std::hypot(pp.x - piece_at[k].x, pp.y - piece_at[k].y, pp.z - piece_at[k].z) < 0.05,
                    (prefix + "every south piece keeps its position in the same world").c_str());
            const auto pv = s.kit_body_velocity(body);
            require(std::hypot(pv.x - piece_vel[k].x, pv.y - piece_vel[k].y, pv.z - piece_vel[k].z) < 0.05,
                    (prefix + "every south piece keeps its velocity in the same world").c_str());
        }
        for (std::uint32_t k = 0; k < 8; ++k) {
            const std::uint32_t body = s.kit_body_index(piece_entity_for(north_entity, k));
            require(body != 0xFFFFFFFFU, (prefix + "every north piece stays").c_str());
            const auto pp = s.kit_body_position(body);
            require(std::hypot(pp.x - piece_at[8 + k].x, pp.y - piece_at[8 + k].y,
                               pp.z - piece_at[8 + k].z) < 0.05,
                    (prefix + "every north piece keeps its position in the same world").c_str());
            const auto pv = s.kit_body_velocity(body);
            require(std::hypot(pv.x - piece_vel[8 + k].x, pv.y - piece_vel[8 + k].y,
                               pv.z - piece_vel[8 + k].z) < 0.05,
                    (prefix + "every north piece keeps its velocity in the same world").c_str());
        }
    }
}

// A fabricated object built before the break, loaded back into the
// same world: the crowbar sits at a kit index before the break
// pieces, so the save's states must land on the right bodies.
// read_save lists the non-fabricated bodies in kit order with the
// fabricated states appended at the end, and a restore by kit
// position gives piece 0's state to the crowbar and shifts every
// piece by one. (Claude review of #299 @ 730177b9, comment
// 6734603519: the kit is built bodies, then the crowbar, then the
// eight pieces, and the load must put each state back where it was.)
void fabricated_same_world_load_case() {
    const std::string prefix = "stock damage fabricated same-world load: ";
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(2.0);  // settle; commits happen
    const std::uint64_t fabricated_entity = fabricate_crowbar(s);
    const std::uint32_t crate = piece(s, LooseStock::kDeckCrateNorth);
    const std::uint64_t entity = LooseStock::kFirstEntity + LooseStock::kDeckCrateNorth;
    const double mass = s.kit_body_mass(crate);
    // The drop breaks the crate on landing, far past the 300 N.s line.
    const auto at = s.kit_body_position(crate);
    put(s, crate, {at.x, at.y + 3.0, at.z});
    (void)s.advance_frame(2.0);
    require(s.stock_damage(entity).broken, (prefix + "the crate breaks").c_str());
    // The kit is now built bodies, then the crowbar, then eight pieces.
    const std::uint32_t fabricated_body = s.fabricated_object(0).body;
    require(fabricated_body < s.kit_body_index(piece_entity_for(entity, 0)),
            (prefix + "the crowbar sits before the pieces").c_str());
    const auto fabricated_at = s.kit_body_position(fabricated_body);
    const auto fabricated_vel = s.kit_body_velocity(fabricated_body);
    std::array<Vector3, 8> piece_at{};
    std::array<Vector3, 8> piece_vel{};
    for (std::uint32_t k = 0; k < 8; ++k) {
        const std::uint32_t body = s.kit_body_index(piece_entity_for(entity, k));
        require(body != 0xFFFFFFFFU, (prefix + "every piece exists").c_str());
        piece_at[k] = s.kit_body_position(body);
        piece_vel[k] = s.kit_body_velocity(body);
        require(std::abs(s.kit_body_mass(body) - mass / 8.0) < 0.01,
                (prefix + "every piece keeps its mass share").c_str());
    }
    const std::uint32_t bodies_after = s.kit_body_count();
    const std::vector<std::uint8_t> bytes = s.save_game();
    // Back into the same world: the crowbar and every piece keep their place.
    require(s.load_game(bytes.data(), bytes.size()), (prefix + "the same world loads the save").c_str());
    require(s.kit_body_count() == bodies_after,
            (prefix + "the same world makes no second set of pieces").c_str());
    require(s.fabricated_count() == 1 && s.fabricated_object(0).entity == fabricated_entity,
            (prefix + "the fabricated object stays").c_str());
    const std::uint32_t loaded_body = s.fabricated_object(0).body;
    const auto loaded_at = s.kit_body_position(loaded_body);
    require(std::hypot(loaded_at.x - fabricated_at.x, loaded_at.y - fabricated_at.y,
                       loaded_at.z - fabricated_at.z) < 0.05,
            (prefix + "the crowbar keeps its position").c_str());
    const auto loaded_vel = s.kit_body_velocity(loaded_body);
    require(std::hypot(loaded_vel.x - fabricated_vel.x, loaded_vel.y - fabricated_vel.y,
                       loaded_vel.z - fabricated_vel.z) < 0.05,
            (prefix + "the crowbar keeps its velocity").c_str());
    for (std::uint32_t k = 0; k < 8; ++k) {
        const std::uint32_t body = s.kit_body_index(piece_entity_for(entity, k));
        require(body != 0xFFFFFFFFU, (prefix + "every piece stays").c_str());
        const auto pp = s.kit_body_position(body);
        require(std::hypot(pp.x - piece_at[k].x, pp.y - piece_at[k].y, pp.z - piece_at[k].z) < 0.05,
                (prefix + "every piece keeps its position").c_str());
        const auto pv = s.kit_body_velocity(body);
        require(std::hypot(pv.x - piece_vel[k].x, pv.y - piece_vel[k].y, pv.z - piece_vel[k].z) < 0.05,
                (prefix + "every piece keeps its velocity").c_str());
    }
}

// A save taken before a crate breaks, loaded back into the world after
// the break: the crate comes back whole and the pieces it made are
// parked, not left enabled beside it — and breaking the restored crate
// again reuses those pieces instead of making a second set with the
// same entities. (#299 review of 4e6ab1f4, comments 4513450833 and
// 4513450847.)
void pre_break_save_same_world_load_case() {
    const std::string prefix = "stock damage pre-break save same-world load: ";
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(2.0);  // settle; commits happen
    const std::uint32_t crate = piece(s, LooseStock::kDeckCrateNorth);
    const std::uint64_t entity = LooseStock::kFirstEntity + LooseStock::kDeckCrateNorth;
    const double mass = s.kit_body_mass(crate);
    const std::uint32_t bodies_before = s.kit_body_count();
    // The save predates the break: its damage holds the crate intact.
    const std::vector<std::uint8_t> bytes = s.save_game();
    // The drop breaks the crate on landing, far past the 300 N.s line.
    const auto at = s.kit_body_position(crate);
    put(s, crate, {at.x, at.y + 3.0, at.z});
    (void)s.advance_frame(2.0);
    require(s.stock_damage(entity).broken, (prefix + "the crate breaks").c_str());
    require(s.kit_body_count() == bodies_before + 8,
            (prefix + "the break adds eight pieces").c_str());
    std::array<std::uint32_t, 8> piece_body{};
    for (std::uint32_t k = 0; k < 8; ++k) {
        piece_body[k] = s.kit_body_index(piece_entity_for(entity, k));
        require(piece_body[k] != 0xFFFFFFFFU, (prefix + "every piece exists").c_str());
    }
    // Back into the same world: the crate comes back whole, its pieces parked.
    require(s.load_game(bytes.data(), bytes.size()),
            (prefix + "the same world loads the pre-break save").c_str());
    require(!s.stock_damage(entity).broken, (prefix + "the crate is whole again").c_str());
    require(s.kit_body_enabled(crate), (prefix + "the restored crate is live").c_str());
    require(s.kit_body_count() == bodies_before + 8,
            (prefix + "the load makes no second set of pieces").c_str());
    for (std::uint32_t k = 0; k < 8; ++k) {
        require(s.kit_body_index(piece_entity_for(entity, k)) == piece_body[k],
                (prefix + "every piece keeps its body").c_str());
        require(!s.kit_body_enabled(piece_body[k]), (prefix + "every piece is parked").c_str());
    }
    // Breaking the restored crate again reuses those pieces.
    const auto at2 = s.kit_body_position(crate);
    put(s, crate, {at2.x, at2.y + 3.0, at2.z});
    (void)s.advance_frame(2.0);
    require(s.stock_damage(entity).broken, (prefix + "the restored crate breaks again").c_str());
    require(s.kit_body_count() == bodies_before + 8,
            (prefix + "the second break makes no second set of pieces").c_str());
    for (std::uint32_t k = 0; k < 8; ++k) {
        require(s.kit_body_index(piece_entity_for(entity, k)) == piece_body[k],
                (prefix + "the second break reuses every piece").c_str());
        require(s.kit_body_enabled(piece_body[k]), (prefix + "every reused piece is live").c_str());
        require(std::abs(s.kit_body_mass(piece_body[k]) - mass / 8.0) < 0.01,
                (prefix + "every reused piece keeps its mass share").c_str());
    }
}

// A save taken before a crate breaks, loaded back after it, the
// restored crate broken again, and then a death before the next
// commit: the checkpoint is the loaded save, so the death must
// bring back the checkpoint world — the crate whole, its pieces
// parked — and the crate must break again after that. (#372
// review of 474fb0c9.)
void pre_break_save_rebreak_death_case() {
    const std::string prefix = "stock damage pre-break save re-break death: ";
    Simulation s(scraperx::sim::InitialSpawn::HighDrop);
    const std::uint32_t crate = piece(s, LooseStock::kDeckCrateNorth);
    const std::uint64_t entity = LooseStock::kFirstEntity + LooseStock::kDeckCrateNorth;
    const double mass = s.kit_body_mass(crate);
    const std::uint32_t bodies_before = s.kit_body_count();
    const auto home = s.kit_body_position(crate);
    // The player never grounds from the spawn to the death, so no
    // checkpoint can commit: the only checkpoint is the loaded save.
    // The load itself puts the player back on the ground at the
    // checkpoint (a save holds the last checkpoint, not the fall),
    // so the fall is re-entered with a launch after the load.
    (void)s.advance_frame(0.5);  // the placement settle expires; the fall is on, no commit
    // The save predates the break: its damage holds the crate intact.
    const std::vector<std::uint8_t> bytes = s.save_game();
    // The drop breaks the crate on landing, far past the 300 N.s line.
    const auto at = s.kit_body_position(crate);
    put(s, crate, {at.x, at.y + 3.0, at.z});
    (void)s.advance_frame(1.0);
    require(s.stock_damage(entity).broken, (prefix + "the crate breaks").c_str());
    require(s.kit_body_count() == bodies_before + 8,
            (prefix + "the break adds eight pieces").c_str());
    std::array<std::uint32_t, 8> piece_body{};
    for (std::uint32_t k = 0; k < 8; ++k) {
        piece_body[k] = s.kit_body_index(piece_entity_for(entity, k));
        require(piece_body[k] != 0xFFFFFFFFU, (prefix + "every piece exists").c_str());
    }
    // Back into the same world: the crate comes back whole, its pieces parked.
    require(s.load_game(bytes.data(), bytes.size()),
            (prefix + "the same world loads the pre-break save").c_str());
    require(!s.stock_damage(entity).broken, (prefix + "the crate is whole again").c_str());
    require(s.kit_body_enabled(crate), (prefix + "the restored crate is live").c_str());
    for (std::uint32_t k = 0; k < 8; ++k) {
        require(!s.kit_body_enabled(piece_body[k]), (prefix + "every piece is parked").c_str());
    }
    // The load strands the player at the checkpoint on the ground: a
    // save holds the last checkpoint, not the fall. Launch him back
    // into the HighDrop fall, so the re-break and the death both
    // land before any grounded tick can commit the broken crate.
    s.debug_launch_player({0.0, 62.0, -8.0}, {0.0, 0.0, 0.0});
    // Breaking the restored crate again reuses those pieces.
    const auto at2 = s.kit_body_position(crate);
    put(s, crate, {at2.x, at2.y + 3.0, at2.z});
    (void)s.advance_frame(1.0);
    require(s.stock_damage(entity).broken, (prefix + "the restored crate breaks again").c_str());
    for (std::uint32_t k = 0; k < 8; ++k) {
        require(s.kit_body_enabled(piece_body[k]), (prefix + "every reused piece is live").c_str());
    }
    // The fall kills before any checkpoint can commit: the death
    // restores the loaded save, the crate whole and its pieces
    // parked — not a world with neither.
    const std::uint64_t deaths = s.snapshot().death_count;
    require(advance_until(s, [deaths](const Snapshot &n) { return n.death_count > deaths; }, 6.0),
            (prefix + "the fall kills").c_str());
    require(s.kit_body_enabled(crate), (prefix + "the death brings the crate back live").c_str());
    require(!s.stock_damage(entity).broken, (prefix + "the death brings the crate back whole").c_str());
    require(s.kit_body_count() == bodies_before + 8,
            (prefix + "the death makes no second set of pieces").c_str());
    for (std::uint32_t k = 0; k < 8; ++k) {
        require(s.kit_body_index(piece_entity_for(entity, k)) == piece_body[k],
                (prefix + "every piece keeps its body").c_str());
        require(!s.kit_body_enabled(piece_body[k]), (prefix + "every piece is parked again").c_str());
    }
    const auto back = s.kit_body_position(crate);
    require(std::hypot(back.x - home.x, back.y - home.y, back.z - home.z) < 0.5,
            (prefix + "the death rewinds the crate to its checkpoint place").c_str());
    // And the restored crate breaks again: it is a whole crate in
    // the world, not a ghost the damage record has written off.
    const auto at3 = s.kit_body_position(crate);
    put(s, crate, {at3.x, at3.y + 3.0, at3.z});
    (void)s.advance_frame(1.0);
    require(s.stock_damage(entity).broken, (prefix + "the restored crate breaks a third time").c_str());
    require(s.kit_body_count() == bodies_before + 8,
            (prefix + "the third break makes no second set of pieces").c_str());
    for (std::uint32_t k = 0; k < 8; ++k) {
        require(s.kit_body_index(piece_entity_for(entity, k)) == piece_body[k],
                (prefix + "the third break reuses every piece").c_str());
        require(s.kit_body_enabled(piece_body[k]), (prefix + "every piece is live again").c_str());
        require(std::abs(s.kit_body_mass(piece_body[k]) - mass / 8.0) < 0.01,
                (prefix + "every piece keeps its mass share").c_str());
    }
}

} // namespace

void run_stock_damage() {
    // ---- the decision, at every tier boundary -------------------------
    using K = LooseStock::Kind;
    struct Case {
        K kind;
        float peak;
        Tier tier;
        bool breaks;
    };
    static constexpr Case cases[] = {
        {K::Container, 249.0F, Tier::None, false}, {K::Container, 250.0F, Tier::Shallow, false},
        {K::Container, 699.0F, Tier::Shallow, false}, {K::Container, 700.0F, Tier::Medium, false},
        {K::Container, 2499.0F, Tier::Medium, false}, {K::Container, 2500.0F, Tier::Severe, false},
        {K::Drum, 11.0F, Tier::None, false}, {K::Drum, 12.0F, Tier::Shallow, false},
        {K::Drum, 29.0F, Tier::Shallow, false}, {K::Drum, 30.0F, Tier::Medium, false},
        {K::Drum, 49.0F, Tier::Medium, false}, {K::Drum, 50.0F, Tier::Severe, false},
        {K::Crate, 64.0F, Tier::None, false}, {K::Crate, 65.0F, Tier::Damage, false},
        {K::Crate, 299.0F, Tier::Damage, false}, {K::Crate, 300.0F, Tier::Break, true},
        {K::SkidCrate, 300.0F, Tier::Break, true},
    };
    for (const Case &c : cases) {
        const Decision d = decide(c.kind, c.peak);
        require(d.tier == c.tier && d.breaks == c.breaks, "stock damage: the tier boundary holds");
    }
    require(decide(K::Container, 12000.0F).depth_m == 0.22F, "stock damage: container depth caps at 0.22");
    require(decide(K::Drum, 200.0F).depth_m == 0.10F, "stock damage: drum depth caps at 0.10");
    // ---- (a) 1 mm quantization, pinned ------------------------------------
    // 300 N.s on a container interpolates to 0.00556 m: the recorded depth
    // is the 1 mm grid, 0.006 m, not the raw interpolation.
    require(decide(K::Container, 300.0F).depth_m == 0.006F, "stock damage: depth quantizes to 1 mm");
    require(radius_for(0.0F) == 0.12F, "stock damage: radius at zero depth is 0.12");
    require(std::abs(radius_for(0.22F) - 0.67F) < 0.001F, "stock damage: radius at 0.22 depth is 0.67");
    require(radius_for(0.30F) == 0.75F, "stock damage: radius caps at 0.75");
    // ---- the 8-cap merge ------------------------------------------------
    {
        DamageState state;
        for (int i = 0; i < 8; ++i) {
            merge_dent(state, Dent{static_cast<float>(i), 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.02F, 0.17F, Tier::Shallow},
                       0.22F);
        }
        require(state.dent_count == 8, "stock damage: 8 dents recorded");
        merge_dent(state, Dent{0.1F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.04F, 0.22F, Tier::Medium}, 0.22F);
        require(state.dent_count == 8 && state.dents[0].depth_m > 0.02F,
                "stock damage: the 9th dent merges into the nearest, deepening it");
    }
    // ---- (a) the 9th dent merges into the NEAREST, ties to the lower index
    {
        DamageState state;
        for (int i = 0; i < 8; ++i) {
            merge_dent(state, Dent{static_cast<float>(i), 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.02F, 0.17F, Tier::Shallow},
                       0.22F);
        }
        // Halfway between dents 3 and 4: a tie, so the lower index deepens.
        merge_dent(state, Dent{3.5F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.04F, 0.22F, Tier::Medium}, 0.22F);
        require(state.dent_count == 8 && state.dents[3].depth_m == 0.03F && state.dents[4].depth_m == 0.02F,
                "stock damage: a merge tie goes to the lower index");
        // The deepening itself quantizes to 1 mm: 0.03 + 0.25 * 0.0333 = 0.038.
        merge_dent(state, Dent{3.5F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0333F, 0.22F, Tier::Medium}, 0.22F);
        require(std::abs(state.dents[3].depth_m - 0.038F) < 0.001F,
                "stock damage: a merge deepening quantizes to 1 mm");
    }
    // ---- (b) the settle exclusion, and (c) the support classifier --------
    // These drive observe() directly: the classifier is the gravity
    // adjustment, so the sequences are written in velocities and hints,
    // not physics. One tick per call, one entity, a 3,750 kg container.
    {
        const std::uint64_t entity = LooseStock::kFirstEntity;
        const ContactHint up{entity, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F};
        const ContactHint side{entity, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F};
        auto tick = [&](StockDamage &dmg, const float vx, const float vy,
                        const std::vector<ContactHint> &hints) {
            dmg.observe(entity, K::Container, 3750.0F, vx, vy, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F,
                        1.0F, hints);
        };
        auto quiet = [&](StockDamage &dmg, const int n, const std::vector<ContactHint> &hints) {
            for (int i = 0; i < n; ++i) {
                tick(dmg, 0.0F, 0.0F, hints);
            }
        };
        // (b) A fresh spawn's settle never dents, even a severe one, and
        // the exclusion is bounded: past it, the same hit dents.
        {
            StockDamage dmg;
            tick(dmg, 0.0F, 0.0F, {});  // baseline: the spawn, exclusion starts
            tick(dmg, 0.0F, -0.77F, {});  // falling, 0.77 m/s of settle drop
            tick(dmg, 0.0F, 0.0F, {up});  // the hard stop: 3,296 N.s, severe
            quiet(dmg, 6, {up});
            require(dmg.state(entity).dent_count == 0,
                    "stock damage: a fresh spawn's settle never dents, even a severe one");
            quiet(dmg, 20, {up});  // the exclusion runs out
            // A real fall, one gravity step a tick: quiet in flight,
            // and the impact reads with gravity's share.
            tick(dmg, 0.0F, -0.109F, {});
            tick(dmg, 0.0F, -0.218F, {});
            tick(dmg, 0.0F, -0.327F, {});
            tick(dmg, 0.0F, -0.436F, {});
            tick(dmg, 0.0F, -0.545F, {});
            tick(dmg, 0.0F, 0.0F, {up});  // a real vertical hit: medium band
            quiet(dmg, 6, {up});
            const auto &st = dmg.state(entity);
            require(st.dent_count == 1 && st.dents[0].tier == Tier::Medium,
                    "stock damage: past the settle exclusion, a real vertical hit dents");
        }
        // (c) An airborne side-scrape then impact counts as in flight:
        // the scrape's lateral normal is not support, so the impact
        // still carries gravity's share: 2,659 N.s, severe. Under the
        // old "a hint existed last tick" rule it read 2,250, medium.
        {
            StockDamage dmg;
            tick(dmg, 0.0F, 0.0F, {});  // baseline
            quiet(dmg, 20, {});  // the spawn exclusion runs out
            tick(dmg, 0.0F, -0.6F, {});  // falling, no contact
            tick(dmg, 0.0F, -0.6F, {side});  // side-scrape: lateral, not support
            tick(dmg, 0.0F, -0.6F, {side});  // still scraping, still in flight
            tick(dmg, 0.0F, 0.0F, {up});  // the impact
            quiet(dmg, 6, {up});
            const auto &st = dmg.state(entity);
            require(st.dent_count == 1 && st.dents[0].tier == Tier::Severe,
                    "stock damage: an airborne side-scrape then impact counts as in flight");
        }
        // (c) The first landing tick counts as in flight, and two ticks
        // of opposing contact make the body supported: the same raw hit
        // then reads without gravity's share: 2,250 N.s, medium.
        {
            StockDamage dmg;
            tick(dmg, 0.0F, 0.0F, {});  // baseline
            quiet(dmg, 20, {});  // the spawn exclusion runs out
            tick(dmg, 0.0F, -0.6F, {});  // falling
            tick(dmg, 0.0F, 0.0F, {up});  // first landing tick: still in flight
            quiet(dmg, 6, {up});  // the window closes
            const auto &first = dmg.state(entity);
            require(first.dent_count == 1 && first.dents[0].tier == Tier::Severe,
                    "stock damage: the first landing tick counts as in flight");
            // A second hit, now supported: shoved sideways while the
            // ground still holds it. A downward hit cannot be staged
            // here — stopping the body again would be a second event
            // of its own. The same raw 0.6 m/s reads without
            // gravity's share: 2,250 N.s, medium. The body slides on
            // at 0.6 m/s, so the window closes on quiet ticks.
            tick(dmg, 0.6F, 0.0F, {up});
            for (int i = 0; i < 6; ++i) {
                tick(dmg, 0.6F, 0.0F, {up});
            }
            const auto &second = dmg.state(entity);
            require(second.dent_count == 2 && second.dents[1].tier == Tier::Medium &&
                        std::abs(second.dents[1].depth_m - 0.136F) < 0.001F,
                    "stock damage: two ticks of opposing contact make the body supported");
        }
    }
    // ---- at rest, nothing dents -----------------------------------------
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        (void)s.advance_frame(2.0);
        bool any = false;
        for (std::uint32_t i = 0; i < s.loose_stock_count(); ++i) {
            const auto state = s.stock_damage(LooseStock::kFirstEntity + i);
            any = any || state.dent_count > 0 || state.broken;
        }
        require(!any, "stock damage: at rest, no dents and no breaks");
    }
    // ---- the forklift's ram, by the probe's numbers ----------------------
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        const std::uint64_t entity = forklift_into(s, Target::Container, 1.0);
        check_dent(s.stock_damage(entity), Tier::Severe, 0.15F, 0.22F, "stock damage: full ram dents a container severe");
    }
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        const std::uint64_t entity = forklift_into(s, Target::Container, 0.25);
        check_dent(s.stock_damage(entity), Tier::Shallow, 0.0F, 0.05F, "stock damage: creep dents a container shallow");
    }
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        const std::uint64_t entity = forklift_into(s, Target::Drum, 1.0);
        check_dent(s.stock_damage(entity), Tier::Severe, 0.05F, 0.10F, "stock damage: full ram dents a drum severe");
    }
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        const std::uint64_t entity = forklift_into(s, Target::Drum, 0.25);
        check_dent(s.stock_damage(entity), Tier::Shallow, 0.0F, 0.02F, "stock damage: creep dents a drum shallow");
    }
    // ---- her throw, by the probe's numbers -------------------------------
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        const std::uint64_t entity = throw_at(s, Target::Drum);
        check_dent(s.stock_damage(entity), Tier::Severe, 0.05F, 0.10F, "stock damage: her throw dents a drum severe");
    }
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        const std::uint64_t entity = throw_at(s, Target::Crate);
        const auto state = s.stock_damage(entity);
        require(state.dent_count >= 1 && !state.broken && state.dents[0].tier == Tier::Damage,
                "stock damage: her throw damages a crate, it does not break");
    }
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        const std::uint64_t entity = throw_at(s, Target::Container);
        const auto state = s.stock_damage(entity);
        require(state.dent_count == 0 && !state.broken, "stock damage: her throw leaves a container undented");
    }
    // ---- (b) a placed body's own settle never dents ----------------------
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        (void)s.advance_frame(2.0);  // the spawn exclusion is long past
        const std::uint32_t i = lane_container(s);
        const std::uint32_t body = piece(s, i);
        const std::uint64_t entity = LooseStock::kFirstEntity + i;
        const auto p = s.kit_body_position(body);
        put(s, body, {p.x, p.y + 0.03, p.z});  // a 3 cm set-down: a severe settle
        (void)s.advance_frame(2.0);
        const auto state = s.stock_damage(entity);
        require(state.dent_count == 0 && !state.broken,
                "stock damage: a 3 cm set-down after placement never dents");
    }
    // ---- (b) a real vertical hit dents by the same bands ------------------
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        (void)s.advance_frame(2.0);
        const std::uint32_t i = lane_container(s);
        const std::uint32_t body = piece(s, i);
        const std::uint64_t entity = LooseStock::kFirstEntity + i;
        const auto p = s.kit_body_position(body);
        // Up, not placed: a launch, so the settle exclusion does not
        // apply. The recorded dent is the landing, not the launch:
        // the launch window closes hintless (0 dents at t = 0.10 s)
        // and the dent appears by t = 0.15 s, when the landing
        // window closes. Measured landing: 898 N.s, medium, depth
        // 0.061 m. The earlier 1,721 N.s / 0.107 m prediction assumed
        // a free-fall landing the 6 mm hop does not produce; it only
        // looked right before because the launch tick itself was
        // phantom-inflated to 1,721. The window pins the measured
        // landing and sits below the launch tick's own 0.084 m.
        s.debug_launch_kit_body(body, {p.x, p.y, p.z}, {0.0, 0.35, 0.0});
        (void)s.advance_frame(2.0);
        const auto state = s.stock_damage(entity);
        require(state.dent_count >= 1 && !state.broken && state.dents[0].tier == Tier::Medium &&
                    state.dents[0].depth_m >= 0.05F && state.dents[0].depth_m <= 0.08F,
                "stock damage: a dropped container dents by the medium band");
    }
    // ---- dents ride a save into a fresh world ------------------------------
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        const std::uint64_t entity = throw_at(s, Target::Drum);
        const auto before = s.stock_damage(entity);
        require(before.dent_count >= 1, "stock damage save: the dent is recorded before the save");
        const std::vector<std::uint8_t> bytes = s.save_game();
        Simulation t(scraperx::sim::InitialSpawn::ExteriorGrade);
        require(t.load_game(bytes.data(), bytes.size()), "stock damage save: a fresh world loads the save");
        const auto after = t.stock_damage(entity);
        require(after.dent_count == before.dent_count && after.dents[0].tier == before.dents[0].tier &&
                    std::abs(after.dents[0].depth_m - before.dents[0].depth_m) < 0.001F,
                "stock damage save: the dent rides the save, tier and depth kept");
    }
    // ---- the full ram breaks a crate into 8 carryable pieces ------------
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        const std::uint32_t crate = piece(s, LooseStock::kDeckCrateNorth);
        const double mass = s.kit_body_mass(crate);
        const std::uint64_t entity = forklift_into(s, Target::Crate, 1.0);
        const auto state = s.stock_damage(entity);
        require(state.broken, "stock damage break: the full ram breaks the crate");
        for (std::uint32_t k = 0; k < 8; ++k) {
            const std::uint32_t body = s.kit_body_index(piece_entity_for(entity, k));
            require(body != 0xFFFFFFFFU, "stock damage break: every piece exists");
            require(std::abs(s.kit_body_mass(body) - mass / 8.0) < 0.01,
                    "stock damage break: each piece carries a eighth of the mass");
        }
        (void)s.advance_frame(3.0);
        for (std::uint32_t k = 0; k < 8; ++k) {
            const auto v = s.kit_body_velocity(s.kit_body_index(piece_entity_for(entity, k)));
            require(std::hypot(v.x, v.y, v.z) < 0.5, "stock damage break: the pieces settle");
        }
        // Back the truck off the debris, get out, and carry the first
        // piece: the walk to it must not thread the truck's corner
        // through the pieces it just made (the capsule pinches there).
        // The truck is still driving when the ram returns, so reverse
        // on the stick as-is; asking first would only let go of it.
        // The pick-up reaches 1.20 m from the player's centre, so a
        // standoff 0.9 m south of the piece, faced at it, is enough.
        (void)s.set_move_input(0.0, -1.0);
        (void)s.advance_frame(2.0);
        (void)s.set_move_input(0.0, 0.0);
        (void)s.request_forklift_action();
        (void)s.advance_frame(0.5);
        require(!s.forklift_state().driving, "stock damage break: the truck lets go");
        const std::uint32_t first = s.kit_body_index(piece_entity_for(entity, 0));
        const auto pp = s.kit_body_position(first);
        require(walk_to(s, pp.x, pp.z - 0.9, 30.0, 0.3), "stock damage break: to the first piece");
        const auto me = s.snapshot().player_position;
        (void)s.set_facing(pp.x - me.x, pp.z - me.z);
        (void)s.advance_frame(0.3);
        require(s.request_pick_up() && s.advance_frame(0.5).accepted &&
                    s.snapshot().carrying_entity_id == piece_entity_for(entity, 0),
                "stock damage break: the first piece is carried");
    }
    // ---- (a) a save after a break loads into a fresh world, pieces and all ----
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        (void)s.advance_frame(2.0);
        const std::uint32_t crate = piece(s, LooseStock::kDeckCrateNorth);
        const std::uint64_t entity = LooseStock::kFirstEntity + LooseStock::kDeckCrateNorth;
        const double mass = s.kit_body_mass(crate);
        const std::uint32_t bodies_before = s.kit_body_count();
        const auto at = s.kit_body_position(crate);
        // Dropped from 3 m: it lands at ~7.7 m/s, a 1,230 N.s hit, far
        // past the 300 N.s break line, so the break executes on landing.
        // A downward launch from the ground cannot break it: the ground
        // eats the velocity inside one tick, so the observer, which
        // reads post-step velocities, never sees a change.
        put(s, crate, {at.x, at.y + 3.0, at.z});
        (void)s.advance_frame(2.0);  // the fall, the landing, the break, a checkpoint commits
        require(s.stock_damage(entity).broken, "stock damage break save: the launch breaks the crate");
        require(s.kit_body_count() == bodies_before + 8,
                "stock damage break save: the break adds eight bodies");
        const std::vector<std::uint8_t> bytes = s.save_game();
        Simulation t(scraperx::sim::InitialSpawn::ExteriorGrade);
        require(t.load_game(bytes.data(), bytes.size()),
                "stock damage break save: a fresh world loads the save");
        require(t.stock_damage(entity).broken, "stock damage break save: the break rides the save");
        require(!t.kit_body_enabled(crate), "stock damage break save: the loaded crate stays parked");
        require(t.kit_body_count() == bodies_before + 8,
                "stock damage break save: the loaded world holds the eight pieces");
        for (std::uint32_t k = 0; k < 8; ++k) {
            const std::uint32_t body = t.kit_body_index(piece_entity_for(entity, k));
            require(body != 0xFFFFFFFFU,
                    "stock damage break save: every piece exists in the loaded world");
            require(std::abs(t.kit_body_mass(body) - mass / 8.0) < 0.01,
                    "stock damage break save: each loaded piece carries an eighth of the mass");
        }
    }
    // ---- (a2) a save whose checkpoint predates the break loads too ----
    // The seeded checkpoint never held the pieces: the save names eight
    // bodies more than its checkpoint's states. The loader makes the
    // pieces at the break geometry around the crate's checkpoint place.
    {
        Simulation s(scraperx::sim::InitialSpawn::HighDrop);
        const std::uint32_t crate = piece(s, LooseStock::kDeckCrateNorth);
        const std::uint64_t entity = LooseStock::kFirstEntity + LooseStock::kDeckCrateNorth;
        const double mass = s.kit_body_mass(crate);
        const std::uint32_t bodies_before = s.kit_body_count();
        const auto home = s.kit_body_position(crate);
        // Dropped from 3 m while the fall is on: no checkpoint can
        // commit before the break, so the seeded checkpoint predates it.
        put(s, crate, {home.x, home.y + 3.0, home.z});
        (void)s.advance_frame(1.0);  // the fall, the landing, the break executes
        require(s.stock_damage(entity).broken, "stock damage break save early: the drop breaks the crate");
        require(s.kit_body_count() == bodies_before + 8,
                "stock damage break save early: the break adds eight bodies");
        const std::vector<std::uint8_t> bytes = s.save_game();
        Simulation t(scraperx::sim::InitialSpawn::HighDrop);
        require(t.load_game(bytes.data(), bytes.size()),
                "stock damage break save early: a fresh world loads the save");
        require(t.stock_damage(entity).broken, "stock damage break save early: the break rides the save");
        require(!t.kit_body_enabled(crate), "stock damage break save early: the loaded crate stays parked");
        require(t.kit_body_count() == bodies_before + 8,
                "stock damage break save early: the loaded world holds the eight pieces");
        const auto back = t.kit_body_position(crate);
        require(std::hypot(back.x - home.x, back.y - home.y, back.z - home.z) < 0.5,
                "stock damage break save early: the loaded crate sits at its checkpoint place");
        for (std::uint32_t k = 0; k < 8; ++k) {
            const std::uint32_t body = t.kit_body_index(piece_entity_for(entity, k));
            require(body != 0xFFFFFFFFU,
                    "stock damage break save early: every piece exists in the loaded world");
            require(std::abs(t.kit_body_mass(body) - mass / 8.0) < 0.01,
                    "stock damage break save early: each loaded piece carries an eighth of the mass");
            const auto pp = t.kit_body_position(body);
            require(std::hypot(pp.x - home.x, pp.y - home.y, pp.z - home.z) < 2.0,
                    "stock damage break save early: each loaded piece sits by the break");
        }
    }
    // ---- (b) a death after a break still rewinds the kit --------------------
    // The seeded checkpoint predates the break: the crate is broken
    // mid-fall, while no checkpoint can commit, so the death restores
    // from a checkpoint that never held the pieces. The rewind must
    // still run for every other body, and the break itself must
    // persist: a crate breaks once, its pieces are not made twice.
    {
        Simulation s(scraperx::sim::InitialSpawn::HighDrop);
        const std::uint32_t crate = piece(s, LooseStock::kDeckCrateNorth);
        const std::uint64_t entity = LooseStock::kFirstEntity + LooseStock::kDeckCrateNorth;
        const double mass = s.kit_body_mass(crate);
        const std::uint32_t bodies_before = s.kit_body_count();
        const auto home = s.kit_body_position(crate);
        // Two metres north, onto open ground: far enough to tell the
        // checkpoint's place from the break's, close enough to be clear.
        const Vector3 moved{home.x, home.y, home.z + 2.0};
        put(s, crate, moved);
        (void)s.advance_frame(0.5);  // the placement settle expires; the fall is on, no commit
        // Dropped from 3 m above the moved spot: it lands at ~7.7 m/s,
        // a 1,230 N.s hit, far past the 300 N.s break line.
        const auto at = s.kit_body_position(crate);
        put(s, crate, {at.x, at.y + 3.0, at.z});
        (void)s.advance_frame(1.0);  // the fall, the landing, the break executes
        require(s.stock_damage(entity).broken, "stock damage break death: the launch breaks the crate");
        require(s.kit_body_count() == bodies_before + 8,
                "stock damage break death: the break adds eight bodies");
        const std::uint64_t deaths = s.snapshot().death_count;
        require(advance_until(s, [deaths](const Snapshot &n) { return n.death_count > deaths; }, 6.0),
                "stock damage break death: the fall kills");
        const auto after = s.snapshot();
        require(std::abs(after.player_position.x - after.checkpoint_position.x) < 0.05 &&
                    std::abs(after.player_position.y - after.checkpoint_position.y) < 0.05 &&
                    std::abs(after.player_position.z - after.checkpoint_position.z) < 0.05,
                "stock damage break death: the death restores the player to the checkpoint");
        const auto back = s.kit_body_position(crate);
        require(std::hypot(back.x - home.x, back.y - home.y, back.z - home.z) < 0.5,
                "stock damage break death: the death rewinds the crate to its checkpoint place");
        require(!s.kit_body_enabled(crate), "stock damage break death: the rewound crate stays parked");
        require(s.stock_damage(entity).broken, "stock damage break death: the break survives the death");
        require(s.kit_body_count() == bodies_before + 8,
                "stock damage break death: the death makes no second set of pieces");
        for (std::uint32_t k = 0; k < 8; ++k) {
            const std::uint32_t body = s.kit_body_index(piece_entity_for(entity, k));
            require(body != 0xFFFFFFFFU, "stock damage break death: every piece survives the death");
            require(std::abs(s.kit_body_mass(body) - mass / 8.0) < 0.01,
                    "stock damage break death: each surviving piece keeps an eighth of the mass");
        }
    }
    // ---- (c) fabricate-and-break save/load, both orders, both timings -----
    fabricated_break_case(scraperx::sim::InitialSpawn::HighDrop, true, "fabricate first, before commit");
    fabricated_break_case(scraperx::sim::InitialSpawn::ExteriorGrade, true, "fabricate first, after commit");
    fabricated_break_case(scraperx::sim::InitialSpawn::HighDrop, false, "break first, before commit");
    fabricated_break_case(scraperx::sim::InitialSpawn::ExteriorGrade, false, "break first, after commit");
    // ---- (d) two crates break in reverse entity order: the save loads ----
    reverse_break_save_case();
    // ---- (d) a fabricated object built before the break, loaded back ----
    fabricated_same_world_load_case();
    // ---- (d) a save taken before the break, loaded back after it ----
    pre_break_save_same_world_load_case();
    // ---- (d) that save, a re-break, and a death before the next commit ----
    pre_break_save_rebreak_death_case();
}

// J-074 (#352): the group registers itself beside its function, so no
// shared list edits. Key 985 keeps the old main() order (loose_stock
// 980, look_target 990); the ONLY name stays "stock-damage".
SCRAPERX_GROUP("stock_damage", 985, run_stock_damage);
SCRAPERX_SELECT("stock-damage", run_stock_damage);
