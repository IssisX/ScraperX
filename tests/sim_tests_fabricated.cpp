// Live fabrication, build step 1 (03_EXECUTION/LIVE_FABRICATION/DESIGN.md): an
// object made outside the simulation (tools/fab) becomes a real body from its
// hull file. Its falsifiers, on the three test objects (godot/tests/fabricated:
// a steel crowbar, a timber keg, a rubber tire, made from CC0 models):
//
//  - the delivery: each one's signature verifies against the game's key, its
//    GLB against the hash the hull file names; a byte changed anywhere, or
//    another key, is refused;
//  - the range: no body in the world has an entity at or above
//    fabricated::kEntityBase (above the corpses' wide window too) until one is
//    made; each made takes the next;
//  - the same data, the same end: two worlds given the same objects and the
//    same inputs end in the same state, bit for bit;
//  - body order: one added in a band, after everything else, leaves the drive
//    at grade and the walk on the 706 m deck bit-identical, as a box does
//    (sim_tests_body_order.cpp);
//  - refused: too many hulls, a NaN, a box over the carrier's bound, too heavy,
//    and every other bound, each refused and the world left as it was;
//  - saved: a save holding one round-trips, two fresh worlds given it play on
//    bit for bit, and a save of a world with none (as every older save) still
//    loads;
//  - loose stock: picked up, carried, thrown along the view at what the arms
//    give its mass, heard landing, and asleep where it lies.
#include "bridge/fabricated_delivery.hpp"
#include "sim/fabricated.hpp"
#include "sim/forklift.hpp"
#include "sim_tests.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#ifndef SCRAPERX_SOURCE_DIR
#error "SCRAPERX_SOURCE_DIR must name the repository (CMakeLists.txt sets it)"
#endif

namespace fab = scraperx::sim::fabricated;
using scraperx::bridge::DeliveryCheck;
using scraperx::sim::FabricatedRefusal;
using scraperx::sim::Forklift;
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;
using Bytes = std::vector<std::uint8_t>;

namespace {

Bytes read_file(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    Bytes out((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (out.empty()) {
        std::cerr << "fabricated: cannot read " << path << '\n';
    }
    require(!out.empty(), "fabricated: the test objects are in godot/tests/fabricated");
    return out;
}

struct TestObject final {
    std::string name;
    Bytes hull;
    Bytes signature;
    Bytes model;
};

std::vector<TestObject> test_objects() {
    std::vector<TestObject> out;
    for (const char *name : {"crowbar", "keg", "tire"}) {
        const std::string stem = std::string(SCRAPERX_SOURCE_DIR) + "/godot/tests/fabricated/" + name;
        out.push_back({name, read_file(stem + ".hull"), read_file(stem + ".sig"), read_file(stem + ".glb")});
    }
    return out;
}

const TestObject &object_named(const std::vector<TestObject> &all, const char *name) {
    for (const TestObject &object : all) {
        if (object.name == name) {
            return object;
        }
    }
    require(false, "fabricated: a test object by that name");
    return all.front();
}

// world_bits (sim_tests_machines.cpp), word for word, but for the fabricated
// objects' own bodies: what every other body, rope and bin is.
std::vector<std::uint64_t> bits_except_fabricated(const Simulation &simulation) {
    std::vector<std::uint64_t> bits;
    const auto put = [&bits](const double value) {
        std::uint64_t word = 0;
        std::memcpy(&word, &value, sizeof(word));
        bits.push_back(word);
    };
    const auto put3 = [&put](const Vector3 &v) {
        put(v.x);
        put(v.y);
        put(v.z);
    };
    const auto state = simulation.snapshot();
    bits.push_back(state.tick_index);
    put(state.simulation_time_seconds);
    put3(state.player_position);
    put3(state.player_linear_velocity);
    bits.push_back(state.support_entity_id);
    bits.push_back(state.carrying_entity_id);
    bits.push_back(state.accepted_traversal_count);
    bits.push_back(state.death_count);
    for (std::uint32_t body = 0; body < simulation.kit_body_count(); ++body) {
        const std::uint64_t entity = simulation.kit_body_entity(body);
        if (entity >= fab::kEntityBase && entity < fab::kEntityLimit) {
            continue;
        }
        bits.push_back(simulation.kit_body_enabled(body) ? 1U : 0U);
        put3(simulation.kit_body_position(body));
        const auto q = simulation.kit_body_rotation(body);
        put(q.x);
        put(q.y);
        put(q.z);
        put(q.w);
        put3(simulation.kit_body_velocity(body));
        put(simulation.kit_body_mass(body));
    }
    std::array<Vector3, 8> points{};
    for (std::uint32_t cable = 0; cable < simulation.kit_cable_count(); ++cable) {
        const std::uint32_t n =
            simulation.kit_cable_points(cable, points.data(), static_cast<std::uint32_t>(points.size()));
        bits.push_back(n);
        for (std::uint32_t i = 0; i < n; ++i) {
            put3(points[i]);
        }
    }
    for (std::uint32_t bin = 0; bin < simulation.kit_bin_count(); ++bin) {
        put(simulation.kit_bin(bin).contents_kg);
    }
    return bits;
}

std::uint64_t add(Simulation &s, const Bytes &hull, const Vector3 at, const double yaw, const char *what) {
    FabricatedRefusal why = FabricatedRefusal::None;
    const std::uint64_t entity = s.add_fabricated(hull.data(), hull.size(), at, yaw, &why);
    if (entity == 0) {
        std::cerr << "fabricated: " << what << " refused (" << fab::refusal_name(why) << ")\n";
    }
    require(entity != 0 && why == FabricatedRefusal::None, what);
    return entity;
}

Vector3 at_of(const Simulation &s, const std::uint64_t entity) {
    return s.kit_body_position(s.kit_body_index(entity));
}

bool asleep(const Simulation &s, const std::uint64_t entity) {
    const Vector3 v = s.kit_body_velocity(s.kit_body_index(entity));
    return v.x == 0.0 && v.y == 0.0 && v.z == 0.0;
}

// ---- the delivery -----------------------------------------------------------

void delivery(const std::vector<TestObject> &objects) {
    using scraperx::bridge::check_delivery;
    for (const TestObject &o : objects) {
        require(check_delivery(o.hull.data(), o.hull.size(), o.signature.data(), o.signature.size(), o.model.data(),
                               o.model.size()) == DeliveryCheck::Ok,
                "fabricated delivery: each test object's signature and model verify against the game's key");
    }
    const TestObject &o = objects.front();
    const auto check = [](Bytes hull, Bytes signature, Bytes model) {
        return check_delivery(hull.data(), hull.size(), signature.data(), signature.size(), model.data(), model.size());
    };
    Bytes hull = o.hull;
    hull[60] ^= 0x01U;
    require(check(hull, o.signature, o.model) == DeliveryCheck::Signature,
            "fabricated delivery: a hull file changed by one bit is refused");
    hull = o.hull;
    hull[hull.size() - 1] ^= 0x80U;
    require(check(hull, o.signature, o.model) == DeliveryCheck::Signature,
            "fabricated delivery: a hull point changed is refused");
    Bytes signature = o.signature;
    signature[10] ^= 0x01U;
    require(check(o.hull, signature, o.model) == DeliveryCheck::Signature,
            "fabricated delivery: a signature changed by one bit is refused");
    Bytes model = o.model;
    model[model.size() / 2] ^= 0x01U;
    require(check(o.hull, o.signature, model) == DeliveryCheck::Model,
            "fabricated delivery: a model changed by one bit is refused");
    require(check(o.hull, o.signature, objects[1].model) == DeliveryCheck::Model,
            "fabricated delivery: another object's model is refused");
    require(check(o.hull, objects[1].signature, o.model) == DeliveryCheck::Signature,
            "fabricated delivery: another object's signature is refused");
    scraperx::bridge::PublicKey other = scraperx::bridge::game_public_key();
    other[0] ^= 0x01U;
    require(check_delivery(o.hull.data(), o.hull.size(), o.signature.data(), o.signature.size(), o.model.data(),
                           o.model.size(), other) == DeliveryCheck::Signature,
            "fabricated delivery: signed by another key is refused");
    require(check(o.hull, Bytes(o.signature.begin(), o.signature.end() - 1), o.model) == DeliveryCheck::Size &&
                check(o.hull, o.signature, Bytes(scraperx::bridge::kMaxModelBytes + 1, 0)) == DeliveryCheck::Size &&
                check(Bytes(scraperx::bridge::kMaxHullBytes + 1, 0), o.signature, o.model) == DeliveryCheck::Size,
            "fabricated delivery: a short signature, a model over 2 MB or a hull file over its bound is refused");
    std::cout << "  fabricated: " << objects.size() << " deliveries verify; a changed byte of hull, signature or "
              << "model, another key, and every size out of bounds are refused\n";
}

// ---- the same data, the same end --------------------------------------------

// From the game's start: the three objects set down east of the climber, the
// keg 2 m up so it falls; ten seconds of the climber walking among them.
std::vector<std::uint64_t> fabricated_run(const std::vector<TestObject> &objects, double &keg_drop) {
    Simulation s(InitialSpawn::ExteriorGrade);
    require(s.advance_frame(0.5).accepted, "fabricated same data: the settle interval");
    const std::uint64_t crowbar = add(s, object_named(objects, "crowbar").hull, {9.0, 0.0, -25.0}, 0.0,
                                      "fabricated same data: the crowbar is taken");
    const std::uint64_t keg = add(s, object_named(objects, "keg").hull, {10.5, 2.0, -26.5}, 0.4,
                                  "fabricated same data: the keg is taken, 2 m up");
    (void)add(s, object_named(objects, "tire").hull, {12.5, 0.0, -25.0}, -0.8,
              "fabricated same data: the tire is taken");
    require(crowbar == fab::kEntityBase && keg == fab::kEntityBase + 1,
            "fabricated same data: each takes the next entity of its range");
    const double keg_y = at_of(s, keg).y;
    require(walk_to(s, 8.0, -27.5, 6.0) && walk_to(s, 13.0, -27.5, 8.0), "fabricated same data: the walk past them");
    require(s.advance_frame(4.0).accepted, "fabricated same data: the rest");
    keg_drop = keg_y - at_of(s, keg).y;
    return world_bits(s);
}

std::size_t same_data(const std::vector<TestObject> &objects) {
    double drop_a = 0.0, drop_b = 0.0;
    const auto first = fabricated_run(objects, drop_a);
    const auto second = fabricated_run(objects, drop_b);
    require(drop_a > 1.5, "fabricated same data: the keg fell, so the compared state moved");
    require(first.size() == second.size(), "fabricated same data: the same bodies in both worlds");
    const auto differ = std::mismatch(first.begin(), first.end(), second.begin());
    if (differ.first != first.end()) {
        std::cerr << "fabricated same data: first different word " << (differ.first - first.begin()) << " of "
                  << first.size() << '\n';
    }
    require(differ.first == first.end(),
            "fabricated same data: the same data and the same inputs end in the same state, bit for bit");
    std::cout << "  fabricated: the same three objects and inputs, twice: " << first.size()
              << " words the same, the keg fallen " << drop_a << " m\n";
    return first.size();
}

// ---- body order ---------------------------------------------------------------

struct Placed final {
    const char *object;
    Vector3 at;
    double yaw;
};

struct World final {
    const char *name;
    std::vector<Simulation::TestExtraSolid> extras;   // what an object is set on, past the yard
    std::vector<Placed> objects;
    bool mid_run = false;                             // added after the settle second, not before it
};

struct End final {
    std::vector<std::uint64_t> bits;
    Vector3 truck{};
    Vector3 player{};
    std::vector<std::uint64_t> made;
};

void place(Simulation &s, const World &world, const std::vector<TestObject> &objects, End &end) {
    for (const Placed &p : world.objects) {
        end.made.push_back(add(s, object_named(objects, p.object).hull, p.at, p.yaw, "fabricated body order: set down"));
    }
}

// Each object at rest where it was set (an upright one may have toppled on
// its slab: within 0.6 m of where it stood), asleep, above what it was set on.
void check_resting(const Simulation &s, const World &world, const End &end, const char *what) {
    for (std::size_t i = 0; i < end.made.size(); ++i) {
        const Vector3 at = at_of(s, end.made[i]);
        require(asleep(s, end.made[i]) && std::abs(at.x - world.objects[i].at.x) < 0.6 &&
                    std::abs(at.z - world.objects[i].at.z) < 0.6 && at.y > world.objects[i].at.y,
                what);
    }
    // With none, the compared words are world_bits' own.
    require(!end.made.empty() || end.bits == world_bits(s), "fabricated body order: the words are world_bits'");
}

// body order's drive at grade, its every input, with the world's objects.
End drive_at_grade(const World &world, const std::vector<TestObject> &objects) {
    Simulation s(InitialSpawn::ExteriorGrade, world.extras);
    End end;
    if (!world.mid_run) {
        place(s, world, objects, end);
    }
    require(s.advance_frame(1.0).accepted, "fabricated body order: the settle interval");
    if (world.mid_run) {
        place(s, world, objects, end);
    }
    require(walk_to(s, 30.0, 10.0, 30.0) && walk_to(s, 36.4, 16.35, 20.0) &&
                walk_to(s, Forklift::kParkX, 16.35, 8.0, 0.1),
            "fabricated body order: the climber reaches the yard's forklift");
    (void)s.set_facing(0.0, 1.0);
    (void)s.advance_frame(0.5);
    require(s.forklift_state().at_controls && s.request_forklift_action(),
            "fabricated body order: at the wheel, DRIVE takes the controls");
    for (int tick = 0; tick < 2 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
        (void)s.set_move_input(0.0, 1.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.advance_frame(1.0);
    end.bits = bits_except_fabricated(s);
    end.truck = s.kit_body_position(s.kit_body_index(Forklift::kChassisEntity));
    end.player = s.snapshot().player_position;
    check_resting(s, world, end, "fabricated body order: at grade, each object lies where it was set, asleep");
    return end;
}

// body order's walk on the 706 m deck, with the world's objects.
End walk_high(const World &world, const std::vector<TestObject> &objects) {
    Simulation s(InitialSpawn::Deck706, world.extras);
    End end;
    if (!world.mid_run) {
        place(s, world, objects, end);
    }
    require(s.advance_frame(1.0).accepted, "fabricated body order: the high settle interval");
    if (world.mid_run) {
        place(s, world, objects, end);
    }
    for (int tick = 0; tick < 3 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
        const double east = tick < static_cast<int>(1.5 * Simulation::kTickRateHz) ? 1.0 : -1.0;
        (void)s.set_facing(east, 0.0);
        (void)s.set_move_input(east, 0.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.advance_frame(1.0);
    end.bits = bits_except_fabricated(s);
    end.player = s.snapshot().player_position;
    check_resting(s, world, end, "fabricated body order: high, each object lies where it was set, asleep");
    return end;
}

long first_difference(const End &a, const End &b) {
    if (a.bits.size() != b.bits.size()) {
        return static_cast<long>(std::min(a.bits.size(), b.bits.size()));
    }
    const auto differ = std::mismatch(a.bits.begin(), a.bits.end(), b.bits.begin());
    return differ.first == a.bits.end() ? -1L : static_cast<long>(differ.first - a.bits.begin());
}

std::size_t body_order(const std::vector<TestObject> &objects) {
    // A slab 300 m east and south of the tower at a band's height, for an
    // object to be set on there: built after every body, as body order's own
    // box after every body is (which leaves everything as it was).
    const auto slab = [](const double top) {
        return Simulation::TestExtraSolid{300.0, top - 0.5, 300.0, 1.5F, 0.5F, 1.5F, true};
    };
    const std::vector<World> worlds = {
        {"as built", {}, {}},
        {"the keg on the yard's grade, 200 m east", {}, {{"keg", {200.0, 0.0, 150.0}, 0.3}}},
        {"the tire in the 143 m deck's band", {slab(143.0)}, {{"tire", {300.0, 143.0, 300.0}, 0.0}}},
        {"the crowbar in the crane band", {slab(500.0)}, {{"crowbar", {300.0, 500.0, 300.0}, 1.0}}},
        {"the keg in the 706 m deck's band", {slab(706.0)}, {{"keg", {300.0, 706.0, 300.0}, -0.6}}},
        {"all three on the yard's grade, made after the settle second",
         {},
         {{"crowbar", {200.0, 0.0, 154.0}, 0.0}, {"keg", {204.0, 0.0, 150.0}, 0.0}, {"tire", {196.0, 0.0, 150.0}, 0.0}},
         true},
    };
    std::vector<End> grade;
    std::vector<End> high;
    for (const World &world : worlds) {
        grade.push_back(drive_at_grade(world, objects));
        high.push_back(walk_high(world, objects));
        std::cout << "  fabricated body order: " << world.name << ": at grade the truck ends at (" << grade.back().truck.x
                  << ", " << grade.back().truck.y << ", " << grade.back().truck.z << "), on the 706 m deck the climber at ("
                  << high.back().player.x << ", " << high.back().player.y << ", " << high.back().player.z << ")\n";
    }
    require(grade.front().bits.size() > 64 && high.front().bits.size() > 64,
            "fabricated body order: the compared state holds the world's bodies");
    require(std::abs(grade.front().truck.z - Forklift::kParkZ) > 1.0, "fabricated body order: the truck was driven");
    bool same = true;
    for (std::size_t i = 1; i < worlds.size(); ++i) {
        require(grade[i].made.size() == worlds[i].objects.size() && high[i].made.size() == worlds[i].objects.size(),
                "fabricated body order: each world makes its objects");
        const long at_grade = first_difference(grade.front(), grade[i]);
        const long at_height = first_difference(high.front(), high[i]);
        if (at_grade >= 0 || at_height >= 0) {
            same = false;
            std::cerr << "fabricated body order: with " << worlds[i].name << " the state differs: at grade from word "
                      << at_grade << " of " << grade.front().bits.size() << ", on the 706 m deck from word " << at_height
                      << " of " << high.front().bits.size() << " (-1: the same)\n";
        }
    }
    require(same, "fabricated body order: an object made in one band leaves every simulation elsewhere unchanged, bit "
                  "for bit");
    return worlds.size();
}

// ---- refused ------------------------------------------------------------------

// A hull file written here (tools/fab/hullfile.py's layout), for the bounds.
Bytes hull_file(const std::uint32_t material, const float mass, const float volume, const std::array<float, 3> half,
                const std::vector<std::vector<std::array<float, 3>>> &hulls, const std::uint32_t version = 1) {
    Bytes out;
    const auto put = [&out](const void *value, const std::size_t size) {
        const auto *bytes = static_cast<const std::uint8_t *>(value);
        out.insert(out.end(), bytes, bytes + size);
    };
    put("SXFABHUL", 8);
    put(&version, 4);
    put(&material, 4);
    const auto count = static_cast<std::uint32_t>(hulls.size());
    put(&count, 4);
    put(&mass, 4);
    put(&volume, 4);
    put(half.data(), 12);
    const std::array<std::uint8_t, 64> digest{};
    put(digest.data(), digest.size());
    for (const auto &hull : hulls) {
        const auto points = static_cast<std::uint32_t>(hull.size());
        put(&points, 4);
        for (const auto &p : hull) {
            put(p.data(), 12);
        }
    }
    return out;
}

std::vector<std::array<float, 3>> box(const float h, const float dx = 0.0F) {
    std::vector<std::array<float, 3>> out;
    for (const float x : {-h, h}) {
        for (const float y : {-h, h}) {
            for (const float z : {-h, h}) {
                out.push_back({x + dx, y, z});
            }
        }
    }
    return out;
}

std::size_t refused(const std::vector<TestObject> &objects) {
    Simulation s(InitialSpawn::ExteriorGrade);
    require(s.advance_frame(0.5).accepted, "fabricated refused: the settle interval");
    const Vector3 there{100.0, 0.0, 100.0};
    // A steel box of 0.2 m, 62.8 kg: taken. Each case below is it, or a
    // test object's file, wrong in one way.
    constexpr float kSteel = 7850.0F;
    const Bytes good = hull_file(0, kSteel * 0.008F, 0.008F, {0.1F, 0.1F, 0.1F}, {box(0.1F)});
    Bytes nan_point = object_named(objects, "crowbar").hull;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    std::memcpy(nan_point.data() + fab::kHeaderBytes + 4, &nan, 4);
    Bytes truncated = good;
    truncated.resize(truncated.size() - 3);
    Bytes trailing = good;
    trailing.push_back(0);
    Bytes magic = good;
    magic[0] = 'X';
    std::vector<std::vector<std::array<float, 3>>> nine;
    for (int h = 0; h < 9; ++h) {
        nine.push_back(box(0.01F, -0.09F + 0.02F * static_cast<float>(h)));
    }
    std::vector<std::array<float, 3>> crowded = box(0.1F);
    for (int i = 0; crowded.size() < fab::kMaxHullPoints + 1; ++i) {
        crowded.push_back({0.0F, 0.0F, -0.09F + 0.005F * static_cast<float>(i)});
    }
    const std::vector<std::array<float, 3>> flat{{-0.1F, 0.0F, -0.1F}, {0.1F, 0.0F, -0.1F}, {0.1F, 0.0F, 0.1F},
                                                 {-0.1F, 0.0F, 0.1F}};
    struct Case final {
        const char *what;
        Bytes data;
        FabricatedRefusal expected;
        Vector3 at;
    };
    const std::vector<Case> cases = {
        {"too many hulls", hull_file(0, kSteel * 0.000072F, 0.000072F, {0.1F, 0.1F, 0.1F}, nine),
         FabricatedRefusal::HullCount, there},
        {"a NaN in a hull", nan_point, FabricatedRefusal::NotFinite, there},
        {"a box over the carrier's bound",
         hull_file(1, 600.0F * 0.091125F * 8.0F, 0.729F, {0.45F, 0.45F, 0.65F}, {box(0.45F)}),
         FabricatedRefusal::Oversized, there},
        {"a point outside its box", hull_file(0, kSteel * 0.008F, 0.008F, {0.1F, 0.1F, 0.05F}, {box(0.1F)}),
         FabricatedRefusal::Oversized, there},
        {"too heavy", hull_file(0, kSteel * 0.125F, 0.125F, {0.25F, 0.25F, 0.25F}, {box(0.25F)}),
         FabricatedRefusal::TooHeavy, there},
        {"too heavy, its mass understated", hull_file(0, 62.8F, 0.008F, {0.25F, 0.25F, 0.25F}, {box(0.25F)}),
         FabricatedRefusal::MassMismatch, there},
        {"too light", hull_file(1, 600.0F * 0.000008F, 0.000008F, {0.01F, 0.01F, 0.01F}, {box(0.01F)}),
         FabricatedRefusal::TooLight, there},
        {"too thin", hull_file(0, kSteel * 0.0004F, 0.0004F, {0.1F, 0.1F, 0.005F}, {box(0.1F)}),
         FabricatedRefusal::Undersized, there},
        {"its mass not its hulls'", hull_file(0, 40.0F, 0.008F, {0.1F, 0.1F, 0.1F}, {box(0.1F)}),
         FabricatedRefusal::MassMismatch, there},
        {"a hull of 33 points", hull_file(0, kSteel * 0.008F, 0.008F, {0.1F, 0.1F, 0.1F}, {crowded}),
         FabricatedRefusal::PointCount, there},
        {"a hull of 3 points",
         hull_file(0, kSteel * 0.008F, 0.008F, {0.1F, 0.1F, 0.1F},
                   {box(0.1F), {{0.0F, 0.0F, 0.0F}, {0.1F, 0.0F, 0.0F}, {0.0F, 0.1F, 0.0F}}}),
         FabricatedRefusal::PointCount, there},
        {"a flat hull", hull_file(0, kSteel * 0.008F, 0.008F, {0.1F, 0.1F, 0.1F}, {flat, box(0.1F)}),
         FabricatedRefusal::Degenerate, there},
        {"an unknown material", hull_file(7, kSteel * 0.008F, 0.008F, {0.1F, 0.1F, 0.1F}, {box(0.1F)}),
         FabricatedRefusal::Material, there},
        {"another version", hull_file(0, kSteel * 0.008F, 0.008F, {0.1F, 0.1F, 0.1F}, {box(0.1F)}, 2),
         FabricatedRefusal::Version, there},
        {"not a hull file", magic, FabricatedRefusal::Format, there},
        {"cut short", truncated, FabricatedRefusal::Format, there},
        {"a byte past its last hull", trailing, FabricatedRefusal::Format, there},
        {"empty", Bytes(10, 0), FabricatedRefusal::Size, there},
        {"over the largest a hull file is", Bytes(fab::kMaxFileBytes + 1, 0), FabricatedRefusal::Size, there},
        {"set inside the ground", good, FabricatedRefusal::Placement, {100.0, -0.5, 100.0}},
        {"set nowhere", good, FabricatedRefusal::Placement, {nan, 0.0, 100.0}},
        {"set out of the world", good, FabricatedRefusal::Placement, {1.0e7, 0.0, 100.0}},
    };
    const auto before = world_bits(s);
    const std::uint32_t bodies = s.kit_body_count();
    for (const Case &c : cases) {
        FabricatedRefusal why = FabricatedRefusal::None;
        const std::uint64_t entity = s.add_fabricated(c.data.data(), c.data.size(), c.at, 0.0, &why);
        if (entity != 0 || why != c.expected) {
            std::cerr << "fabricated refused: " << c.what << ": entity " << entity << ", refused as "
                      << fab::refusal_name(why) << ", not " << fab::refusal_name(c.expected) << '\n';
        }
        require(entity == 0 && why == c.expected, "fabricated refused: each bad delivery is refused for its fault");
    }
    require(world_bits(s) == before && s.kit_body_count() == bodies && s.fabricated_count() == 0,
            "fabricated refused: a refused delivery changes nothing");
    // The good one is taken; another set where it lies, inside it, is not.
    const std::uint64_t one = add(s, good, there, 0.0, "fabricated refused: the steel box is taken");
    FabricatedRefusal why = FabricatedRefusal::None;
    require(s.add_fabricated(good.data(), good.size(), {100.05, 0.0, 100.0}, 0.0, &why) == 0 &&
                why == FabricatedRefusal::Placement,
            "fabricated refused: set inside another, refused");
    require(std::abs(s.kit_body_mass(s.kit_body_index(one)) - 62.8) < 0.1,
            "fabricated refused: the taken box weighs its steel, 62.8 kg");
    // At most kMaxObjects: the next one is refused.
    for (std::uint32_t i = 1; i < fab::kMaxObjects; ++i) {
        (void)add(s, good, {100.0 + 0.5 * (i % 8), 0.0, 102.0 + 0.5 * (i / 8)}, 0.0, "fabricated refused: one of 32");
    }
    require(s.fabricated_count() == fab::kMaxObjects &&
                s.debug_bodies_with_entity_in(fab::kEntityBase, fab::kEntityLimit) == fab::kMaxObjects,
            "fabricated refused: 32 made, each its own entity");
    require(s.add_fabricated(good.data(), good.size(), {110.0, 0.0, 110.0}, 0.0, &why) == 0 &&
                why == FabricatedRefusal::Full,
            "fabricated refused: the 33rd is refused");
    std::cout << "  fabricated: " << cases.size() + 2 << " bad deliveries refused for their faults, the world as it "
              << "was; 32 taken and the 33rd refused\n";
    return cases.size() + 2;
}

// ---- saved --------------------------------------------------------------------

std::size_t saved(const std::vector<TestObject> &objects) {
    const Bytes &keg = object_named(objects, "keg").hull;
    const Bytes &crowbar = object_named(objects, "crowbar").hull;
    Simulation a(InitialSpawn::ExteriorGrade);
    require(a.advance_frame(1.0).accepted, "fabricated save: the settle interval");
    const std::vector<std::uint8_t> before = a.save_game();
    const std::uint64_t k = add(a, keg, {9.0, 0.0, -24.0}, 0.0, "fabricated save: the keg");
    const std::uint64_t c = add(a, crowbar, {10.5, 1.0, -24.0}, 0.3, "fabricated save: the crowbar, 1 m up");
    // The save's own layout is untouched: with the objects it is the same
    // bytes, and then theirs.
    const std::vector<std::uint8_t> just_made = a.save_game();
    require(just_made.size() > before.size() && std::equal(before.begin(), before.end(), just_made.begin()),
            "fabricated save: a save with them is the save without them, and then theirs");
    require(a.advance_frame(3.0).accepted && a.snapshot().player_grounded, "fabricated save: they settle, the climber stands");
    const std::vector<std::uint8_t> bytes = a.save_game();
    const Vector3 keg_at = at_of(a, k);
    const Vector3 crowbar_at = at_of(a, c);

    // Two fresh worlds take the save: the objects made again from it, where
    // it has them.
    Simulation b(InitialSpawn::ExteriorGrade);
    Simulation d(InitialSpawn::ExteriorGrade);
    require(b.load_game(bytes.data(), bytes.size()) && d.load_game(bytes.data(), bytes.size()),
            "fabricated save: a fresh world loads a save holding them");
    require(b.fabricated_count() == 2 && b.fabricated_object(0).entity == k && b.fabricated_object(1).entity == c &&
                b.fabricated_data(0) == keg && b.fabricated_data(1) == crowbar &&
                b.kit_body_count() == a.kit_body_count(),
            "fabricated save: loaded, the world has the same objects, made from the same hull files");
    const auto near = [](const Vector3 &p, const Vector3 &q) {
        return std::hypot(p.x - q.x, p.y - q.y, p.z - q.z) < 1.0e-4;
    };
    require(near(at_of(b, k), keg_at) && near(at_of(b, c), crowbar_at),
            "fabricated save: each where the save has it");
    for (Simulation *sim : {&b, &d}) {
        (void)walk_to(*sim, 10.0, -26.0, 6.0);
        (void)sim->advance_frame(2.0);
    }
    require(world_bits(b) == world_bits(d), "fabricated save: two fresh worlds given the save play on bit for bit");
    // The original, given its own save, has them already: the same ones.
    require(a.load_game(bytes.data(), bytes.size()) && a.fabricated_count() == 2 && near(at_of(a, k), keg_at),
            "fabricated save: the original loads its own save");

    // A save of a world with none (every save made before them) still loads,
    // into a fresh world and into one that has made one since.
    Simulation e(InitialSpawn::ExteriorGrade);
    require(e.load_game(before.data(), before.size()) && e.fabricated_count() == 0,
            "fabricated save: a save without them loads");
    Simulation f(InitialSpawn::ExteriorGrade);
    const std::uint64_t later = add(f, crowbar, {9.0, 0.0, -24.0}, 0.0, "fabricated save: one made since");
    require(f.load_game(before.data(), before.size()) && f.fabricated_count() == 1 && later == fab::kEntityBase,
            "fabricated save: a save without them loads into a world that has one, which stays");

    // Refused, and nothing changed: a save whose objects are not the world's
    // own (another object at the same entity), or whose hull file is bad.
    Simulation g(InitialSpawn::ExteriorGrade);
    (void)add(g, object_named(objects, "tire").hull, {9.0, 0.0, -24.0}, 0.0, "fabricated save: another object");
    const auto g_before = world_bits(g);
    require(!g.load_game(bytes.data(), bytes.size()) && world_bits(g) == g_before && g.fabricated_count() == 1,
            "fabricated save: a save whose objects are not the world's is refused and changes nothing");
    std::vector<std::uint8_t> bad = bytes;
    // The keg's hull file in the save's last section: its first hull's point
    // count made 40.
    std::size_t at = bytes.size();
    for (std::size_t i = before.size(); i + 8 <= bytes.size(); ++i) {
        if (std::memcmp(bytes.data() + i, "SXFABHUL", 8) == 0) {
            at = i;
            break;
        }
    }
    require(at < bytes.size(), "fabricated save: the save carries the hull file");
    const std::uint32_t too_many = 40;
    std::memcpy(bad.data() + at + fab::kHeaderBytes, &too_many, 4);
    Simulation h(InitialSpawn::ExteriorGrade);
    const auto h_before = world_bits(h);
    require(!h.load_game(bad.data(), bad.size()) && world_bits(h) == h_before && h.fabricated_count() == 0,
            "fabricated save: a save carrying a bad hull file is refused and changes nothing");
    std::cout << "  fabricated: a save holding two (" << bytes.size() << " bytes, " << bytes.size() - before.size()
              << " theirs) loads into fresh worlds that play on bit for bit; a save without them still loads; "
              << "a save with another's or a bad one is refused\n";
    return bytes.size();
}

// ---- loose stock ----------------------------------------------------------------

double carried_and_thrown(const std::vector<TestObject> &objects) {
    Simulation s(InitialSpawn::ExteriorGrade);
    require(s.advance_frame(0.5).accepted, "fabricated stock: the settle interval");
    const std::uint64_t keg = add(s, object_named(objects, "keg").hull, {6.0, 0.0, -27.0}, 0.0,
                                  "fabricated stock: the keg set down south of the climber");
    require(s.advance_frame(1.0).accepted, "fabricated stock: it settles");
    const std::uint32_t body = s.kit_body_index(keg);
    const double mass = s.kit_body_mass(body);
    require(walk_to(s, 6.0, -26.25, 6.0, 0.08), "fabricated stock: the walk to it");
    (void)s.set_facing(0.0, -1.0);
    (void)s.advance_frame(0.3);
    require(s.snapshot().carry_target_entity_id == keg, "fabricated stock: PICK UP is offered for it");
    std::uint64_t seen = 0;
    for (const auto &hit : s.impacts()) {
        seen = std::max(seen, hit.sequence);
    }
    require(s.request_pick_up(), "fabricated stock: the pick-up is accepted");
    (void)s.advance_frame(0.6);
    require(s.snapshot().carrying_entity_id == keg, "fabricated stock: it is in the hands");
    require(walk_to(s, 6.0, -23.5, 8.0), "fabricated stock: carried 2.5 m");
    (void)s.set_facing(1.0, 0.0);
    (void)s.advance_frame(0.8);
    require(s.throw_prediction().size() >= 3, "fabricated stock: the throw's path is drawn while it is held");
    const auto offered = s.snapshot();
    require(offered.up_move == scraperx::sim::UpMove::Throw, "fabricated stock: up offers THROW");
    require(s.request_up() && s.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "fabricated stock: up throws it");
    const auto after = s.snapshot();
    const Vector3 v = s.kit_body_velocity(body);
    const double relative = std::hypot(v.x - after.player_linear_velocity.x, v.y - after.player_linear_velocity.y,
                                       v.z - after.player_linear_velocity.z);
    // What 300 J gives its mass (physics_world_tuning.hpp kThrowEnergyJ), as
    // the toolbox and the drum are thrown.
    const double expected = std::min(7.0, std::sqrt(2.0 * 300.0 / mass));
    require(after.carrying_entity_id == 0 && after.throw_count == offered.throw_count + 1 &&
                after.up_resolved == scraperx::sim::UpMove::Throw,
            "fabricated stock: thrown, out of the hands");
    require(std::abs(relative - expected) < 0.3, "fabricated stock: thrown at what the arms give its mass");
    for (const auto &hit : s.impacts()) {
        require(!(hit.entity == keg && hit.sequence > seen), "fabricated stock: not heard at the hands");
    }
    bool landed = false;
    double landed_j = 0.0;
    for (int i = 0; i < 3 * static_cast<int>(Simulation::kTickRateHz) && !landed; ++i) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        for (const auto &hit : s.impacts()) {
            if (hit.entity == keg && hit.sequence > seen) {
                landed = true;
                landed_j = hit.energy_j;
                require(hit.sound == scraperx::sim::ImpactSound::Timber, "fabricated stock: timber, heard as timber");
            }
        }
    }
    require(landed, "fabricated stock: heard landing");
    require(wait_for(s, 10.0, [&](const scraperx::sim::Snapshot &) { return asleep(s, keg); }),
            "fabricated stock: asleep where it lies");
    const Vector3 rest = at_of(s, keg);
    require(rest.x > 7.0 && rest.y < 0.6, "fabricated stock: down on the ground, along the throw");
    std::cout << "  fabricated: the keg (" << mass << " kg) picked up, carried, thrown at " << relative << " m/s ("
              << expected << " expected), heard landing (" << landed_j << " J), asleep at (" << rest.x << ", "
              << rest.y << ", " << rest.z << ")\n";
    return relative;
}

// ---- the range ------------------------------------------------------------------

void range_free() {
    // Nothing in the world, at any spawn the tests use, nor after a death
    // leaves a corpse, has an entity in the fabricated range, nor anywhere
    // above the corpses' wide window (3300 + 0x10000).
    constexpr std::uint64_t kAboveCorpses = 3300 + 0x10000;
    for (const InitialSpawn spawn : {InitialSpawn::ExteriorGrade, InitialSpawn::Deck706, InitialSpawn::Deck154,
                                     InitialSpawn::HighDrop}) {
        Simulation s(spawn);
        require(s.advance_frame(1.0).accepted, "fabricated range: the settle interval");
        require(s.debug_bodies_with_entity_in(kAboveCorpses, std::numeric_limits<std::uint64_t>::max()) == 0,
                "fabricated range: no body above the corpses' window");
        require(s.debug_bodies_with_entity_in(fab::kEntityBase, fab::kEntityLimit) == 0,
                "fabricated range: no body in the fabricated range");
    }
    Simulation fall(InitialSpawn::HighDrop);
    require(advance_until(fall, [](const scraperx::sim::Snapshot &st) { return st.death_count > 0; }, 8.0) &&
                fall.snapshot().corpse_count == 1,
            "fabricated range: a lethal fall leaves a corpse");
    require(fall.debug_bodies_with_entity_in(kAboveCorpses, std::numeric_limits<std::uint64_t>::max()) == 0,
            "fabricated range: a corpse's parts are in the corpses' window");
    std::cout << "  fabricated: no body at or above " << kAboveCorpses << " in four worlds or after a death; "
              << "fabricated objects from " << fab::kEntityBase << "\n";
}

} // namespace

void run_fabricated() {
    const std::vector<TestObject> objects = test_objects();
    delivery(objects);
    range_free();
    const std::size_t words = same_data(objects);
    const std::size_t worlds = body_order(objects);
    const std::size_t refusals = refused(objects);
    const std::size_t save_bytes = saved(objects);
    const double throw_mps = carried_and_thrown(objects);
    std::cout << "PASS scraperx_sim fabricated: objects=" << objects.size() << " same_words=" << words
              << " body_order_worlds=" << worlds << " refused=" << refusals << " save_bytes=" << save_bytes
              << " throw_mps=" << throw_mps << '\n';
}
