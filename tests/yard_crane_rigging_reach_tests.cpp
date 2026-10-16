// Full-world regression: a real lifting-eye bar must not occlude itself.
// Initial staging uses the public restart; contacts and reach use actual solves.
#include "sim/simulation/world.hpp"
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <type_traits>
#include <utility>
using namespace scraperx::sim;
namespace {
// Test-local access only to add an owned physical obstacle and read transforms.
// No contact, player, support, authorization or crane state is assigned.
template <auto Member, class Tag> struct Access {
    friend auto member(Tag) { return Member; }
};
struct WorldTag { friend auto member(WorldTag); };
template struct Access<&Simulation::physics_world_, WorldTag>;
using World = std::remove_reference_t<decltype(*(
    std::declval<Simulation &>().*member(WorldTag{})))>;
struct KitTag { friend auto member(KitTag); };
template struct Access<&World::kit_, KitTag>;
struct PhysicsTag { friend auto member(PhysicsTag); };
template struct Access<&World::physics_system_, PhysicsTag>;
struct PlayerTag { friend auto member(PlayerTag); };
template struct Access<&World::player_id_, PlayerTag>;
void require(bool ok, const char *message) {
    if (!ok) { std::cerr << "FAIL crane eye reach: " << message << std::endl; std::exit(1); }
}
void tick(Simulation &s, unsigned count) {
    while (count--) require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "native tick");
}
void observe(Simulation &s, unsigned leg, const char *label) {
    const auto &p = s.snapshot();
    const auto t = s.kit_body_position(s.kit_body_index(2440));
    const auto c = s.yard_crane_state();
    std::cout << label << " intended=" << leg << " observed=" << c.rigging_leg
              << " reachable=" << c.rigging_reachable
              << " player=" << p.player_position.x << ',' << p.player_position.y << ',' << p.player_position.z
              << " truck=" << t.x << ',' << t.y << ',' << t.z
              << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
              << " gravity=" << p.player_gravity_factor
              << " cab=" << c.controls_reachable << std::endl;
}
void run(unsigned leg, bool blocked_only) {
    Simulation s(InitialSpawn::ExteriorGrade, WorldContent::Slingshot);
    const auto t = s.kit_body_position(s.kit_body_index(2440));
    require(s.debug_restart_at({t.x + (leg % 2 ? 1.15 : -1.15), .9,
                               t.z + (leg < 2 ? .75 : -.75)}), "initial grade staging");
    tick(s, 90);
    const auto &p = s.snapshot();
    observe(s, leg, "CLEAR");
    require(p.player_grounded && p.support_entity_id != 2440 &&
            p.player_gravity_factor == 1 && p.traversal_hand_constraint_count == 0 &&
            p.step_up_pose_count == 0, "real grade support without traversal or pose changes");
    if (!blocked_only)
        require(s.yard_crane_state().rigging_reachable && s.yard_crane_state().rigging_leg == int(leg),
                "actual eye is reachable through its own steel top bar");
    if (leg != 0) return;
    auto &world = *(s.*member(WorldTag{}));
    auto &kit = *(world.*member(KitTag{}));
    auto &bodies = (world.*member(PhysicsTag{})).GetBodyInterface();
    const auto chassis = kit.body_id(kit::BodyIndex{s.kit_body_index(2440)});
    const auto eye = bodies.GetWorldTransform(chassis) * JPH::Vec3(-.52F, .315F, .75F);
    const auto shoulder = bodies.GetPosition(world.*member(PlayerTag{})) + JPH::Vec3(0, .30F, 0);
    // A small steel block beyond the capsule, separated from grade and truck.
    // This is actual Kit-owned collidable geometry, not a fake raycast result.
    const auto obstacle = shoulder + (eye - shoulder) * .70F;
    const auto horizontal = JPH::Vec3(obstacle - shoulder);
    require(std::hypot(horizontal.GetX(), horizontal.GetZ()) > .37,
            "obstacle clears player capsule");
    kit::Part steel;
    steel.half = JPH::Vec3(.035F, .035F, .035F);
    (void)kit.add_body(900001, {steel}, obstacle, JPH::Quat::sIdentity(), 0, .7F);
    tick(s, 1); // Refresh public reach from a real native contact solve.
    observe(s, leg, "BLOCKED");
    require(!s.yard_crane_state().rigging_reachable && s.yard_crane_state().rigging_leg == -1,
            "another real steel body blocks hand reach");
}
}
int main(int argc, char **) {
    std::cout << std::setprecision(9);
    // Optional blocker-only baseline proves the existing obstruction contract.
    const bool blocked_only = argc > 1;
    for (unsigned leg = 0; leg < (blocked_only ? 1U : 4U); ++leg) run(leg, blocked_only);
    std::cout << "PASS crane eye reach actual grade, steel eyes and solid obstruction\n";
}
