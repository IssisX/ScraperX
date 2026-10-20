#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace scraperx::sim;
namespace {
void require(bool value, const char *message) {
    if (value) return;
    std::cerr << "FAIL STORY_ACTOR_PUBLIC " << message << '\n';
    std::exit(EXIT_FAILURE);
}
void tick(Simulation &sim, int count = 1) {
    while (count--) require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "native tick accepted");
}
double distance(Vector3 a, Vector3 b) {
    return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));
}
void normal_world() {
    Simulation sim(InitialSpawn::ExteriorGrade, WorldContent::Slingshot);
    require(sim.entity_body_count(Simulation::kStoryActorEntityId) == 1, "one actual native actor body exists");
    const auto moving = sim.moving_body_count();
    tick(sim, 90);
    auto actor = sim.story_actor_state();
    std::cout << "STORY_ACTOR initial p=" << actor.position.x << ',' << actor.position.y << ',' << actor.position.z
              << " supported=" << actor.supported << " support=" << actor.support_entity_id << std::endl;
    require(actor.present && actor.supported && actor.entity_id == 63200 && actor.support_entity_id == 1993,
            "actor earns actual barrel exit1993 footing");
    require(std::abs(actor.position.y-253.9) < .04, "capsule soles rest on existing253m concrete exit");
    double min_z = actor.position.z, max_z = actor.position.z;
    for (int i=0; i<900; ++i) {
        tick(sim);
        actor = sim.story_actor_state();
        min_z = std::min(min_z, actor.position.z); max_z = std::max(max_z, actor.position.z);
        require(actor.supported && actor.support_entity_id == 1993 && std::abs(actor.position.y-253.9) < .04,
                "inspection walk remains on real unmodified exit");
        require(actor.position.z > -143.5 && actor.position.z < -140.9 &&
                std::abs(actor.position.x+26.75) < .03, "bounded inspection stays clear of pole and exit edges");
        require(std::hypot(actor.drive_force_n.x, actor.drive_force_n.z) <= 220.001 &&
                std::abs(actor.drive_force_n.y) < .00001, "native finite horizontal actuator owns walking");
    }
    require(max_z-min_z > 1.4, "actor performs purposeful inspection walk");
    std::cout << "STORY_ACTOR inspection_z=" << min_z << ',' << max_z << std::endl;

    // Stage only the player in flight; this public debug entry commits the
    // existing whole-world checkpoint, including the actor's moving state.
    actor = sim.story_actor_state();
    require(sim.debug_restart_at({0,20,0}), "player flight staging creates existing retry point");
    const auto saved = sim.story_actor_state();
    tick(sim, 30);
    require(distance(sim.story_actor_state().position,saved.position) > .05, "actor continues independently while player falls");
    require(sim.restart_checkpoint(), "whole-world restart succeeds");
    auto restored = sim.story_actor_state();
    require(distance(restored.position,saved.position) < .00001 &&
            distance(restored.velocity,saved.velocity) < .00001,
            "checkpoint restores actual actor pose and momentum");
    require(!restored.supported && distance(restored.drive_force_n,{}) < .00001 &&
            restored.episode == saved.episode+1, "restore clears stale footing/drive and starts next episode");
    for (int i=0; i<6; ++i) {
        require(sim.restart_checkpoint(), "repeated restart succeeds");
        require(sim.entity_body_count(63200) == 1 && sim.moving_body_count() == moving,
                "repeated restart preserves actor identity/count without leaks");
    }
    tick(sim, 30);
    require(sim.story_actor_state().velocity.z*saved.velocity.z > 0, "restored inspection leg retains direction");

    actor = sim.story_actor_state();
    const double side = actor.position.z < -142.2 ? 1 : -1;
    require(sim.debug_restart_at({actor.position.x,253.9,actor.position.z+side*.85}),
            "player can approach on actual exit beside capsule");
    tick(sim, 90);
    actor = sim.story_actor_state();
    require(sim.snapshot().player_grounded && sim.snapshot().support_entity_id == 1993,
            "player has actual exit footing near actor");
    require(std::hypot(actor.velocity.x,actor.velocity.z) < .02, "actor stops inspection for nearby physical player");
    const auto player = sim.snapshot().player_position;
    require(sim.story_actor_line_of_sight({player.x,player.y+.72,player.z}),
            "actual actor head is visible while player capsule is excluded");
    require(!sim.story_actor_line_of_sight({actor.position.x,252.2,actor.position.z}),
            "existing native concrete exit blocks visibility from below");
    require(!sim.story_actor_line_of_sight({actor.position.x+7,actor.position.y+.72,actor.position.z}) &&
            !sim.story_actor_line_of_sight({std::numeric_limits<double>::quiet_NaN(),254,0}),
            "far and nonfinite eyes cannot authorize encounter visibility");
    const double expected_heading = std::atan2(-(player.x-actor.position.x),-(player.z-actor.position.z));
    require(std::abs(std::remainder(actor.heading_radians-expected_heading,2*3.141592653589793)) < .01,
            "native encounter intent faces actual nearby player");
    const auto before_push = actor.position;
    require(sim.set_move_input(0,-side*.18), "ordinary player walking input accepted");
    tick(sim, 50);
    require(sim.set_move_input(0,0), "player input released");
    const auto pushed = sim.story_actor_state();
    std::cout << "STORY_ACTOR player_push_dz=" << pushed.position.z-before_push.z << std::endl;
    require((pushed.position.z-before_push.z)*(-side) > .03,
            "real player collision pushes actual native actor");
    require(sim.debug_restart_at({100,30,50}), "stage actual lethal fall over open grade");
    const auto deaths = sim.snapshot().death_count;
    const auto episode = sim.story_actor_state().episode;
    for (int i=0; i<360 && sim.snapshot().death_count == deaths; ++i) tick(sim);
    require(sim.snapshot().death_count == deaths+1 && sim.story_actor_state().episode == episode+1 &&
            sim.entity_body_count(63200) == 1, "actual death restores actor state without allocating another body");
    std::cout << "PASS STORY_ACTOR_PUBLIC actor63200 support1993 stable_walk approach_face player_push checkpoint_momentum episode death no_leak\n";
}
void fixture_world_absence() {
    Simulation sim(InitialSpawn::ExteriorGrade, WorldContent::GroundFoundation);
    require(!sim.story_actor_state().present && sim.entity_body_count(63200) == 0,
            "ground fixture does not gain a story body");
}
}
int main() { fixture_world_absence(); normal_world(); }
