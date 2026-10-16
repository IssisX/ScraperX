// Public-input negative regressions for finite walking-step catch suppression.
// Explicit-instantiation observation is read-only; no production state is staged
// after the one initial restart, and no actuator/velocity/contact is forged.
#include "sim/simulation/world.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <type_traits>
#include <utility>

using namespace scraperx::sim;
namespace {
template <auto Member, class Tag> struct ReadMember {
    friend auto member(Tag) { return Member; }
};
struct WorldTag { friend auto member(WorldTag); };
template struct ReadMember<&Simulation::physics_world_, WorldTag>;
using World = std::remove_reference_t<decltype(*(
    std::declval<Simulation &>().*member(WorldTag{})))>;
struct PendingTag { friend auto member(PendingTag); };
template struct ReadMember<&World::physical_step_pending_, PendingTag>;
bool pending(const Simulation &s) {
    return (*(s.*member(WorldTag{}))).*member(PendingTag{});
}
void require(bool condition, const char *message) {
    if (!condition) { std::cerr << "FAIL STEP_INTENT " << message << '\n'; std::exit(1); }
}
void tick(Simulation &s, unsigned count = 1) {
    for (unsigned i = 0; i < count; ++i) {
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "ordinary tick accepted");
        require(s.snapshot().death_count == 0, "fixture remains alive");
        require(s.snapshot().step_up_pose_count == 0, "no pose correction step");
    }
}
void observe(const Simulation &s, const char *phase) {
    const auto &v = s.snapshot();
    std::cout << "STEP_INTENT " << phase << " x=" << v.player_position.x
              << " y=" << v.player_position.y << " vy=" << v.player_linear_velocity.y
              << " grounded=" << v.player_grounded << " pending=" << pending(s)
              << " hands=" << v.traversal_hand_constraint_count << '\n';
}
void setup(Simulation &s) {
    // Settle the ordinary truck before the single staged tread start. The
    // tread is a fixture location only; every later step is physical walking.
    tick(s, 45);
    require(s.debug_restart_at({-100.98, 1.074, -57.65}), "initial truck tread fixture");
    require(s.set_facing(1, 0), "face real cab guard");
}
void walk(Simulation &s) {
    const auto truck = s.kit_body_position(s.kit_body_index(2440));
    const auto &v = s.snapshot();
    require(s.set_move_input(
        std::clamp(1.2 * (truck.x - v.player_position.x) - .45 * v.player_linear_velocity.x, -.3, .3),
        std::clamp(1.2 * (truck.z - 1.65 - v.player_position.z) - .45 * v.player_linear_velocity.z, -.15, .15)),
        "ordinary cab approach input");
    tick(s);
}
void invalid_action_retains_rising_step() {
    Simulation s;
    setup(s);
    bool found = false;
    for (unsigned i = 0; i < 180; ++i) {
        walk(s);
        const auto &v = s.snapshot();
        require(v.traversal_state == TraversalState::None, "walking did not acquire hands");
        if (pending(s) && !v.player_grounded && v.player_linear_velocity.y > 1 &&
            v.player_position.x < -100.95) { found = true; break; }
    }
    observe(s, "invalid-action-before");
    require(found, "real rising finite step reached before the guard");
    // Facing away makes the actual reach invalid while retaining the same
    // walking input. A rejected request must not abandon the walking intent.
    require(s.set_facing(-1, 0), "face away from actual guard");
    require(s.request_traversal(), "invalid Action queued through public API");
    tick(s);
    observe(s, "invalid-action-after");
    require(s.snapshot().player_linear_velocity.y > 0, "still physically rising after rejected Action");
    require(s.snapshot().traversal_hand_constraint_count == 0 &&
            s.snapshot().traversal_state == TraversalState::None, "invalid Action grants no hands");
    require(pending(s), "invalid Action preserves finite step intent");
}
void explicit_action_catches_during_rising_step() {
    Simulation s;
    setup(s);
    bool caught = false;
    for (unsigned i = 0; i < 180; ++i) {
        const auto before = s.snapshot();
        const bool candidate = pending(s) && !before.player_grounded &&
            before.player_linear_velocity.y > 0 && before.player_position.x > -100.93;
        if (candidate) {
            observe(s, "valid-action-before");
            require(s.request_traversal(), "deliberate rising Action queued");
        }
        walk(s);
        if (s.snapshot().traversal_state != TraversalState::None) {
            require(candidate, "hands acquired only on deliberate rising Action");
            require(s.snapshot().traversal_hand_constraint_count > 0, "actual finite hands acquired");
            require(!pending(s), "successful actual hand catch clears walking step intent");
            require(s.snapshot().player_gravity_factor == 1, "catch retains gravity");
            caught = true;
            break;
        }
        if (s.forklift_state().controls_reachable) break;
    }
    observe(s, "valid-action-after");
    require(caught, "deliberate Action can acquire real guard during rising step");
}
}
int main() {
    invalid_action_retains_rising_step();
    explicit_action_catches_during_rising_step();
    std::cout << "PASS STEP_INTENT invalid Action retains intent; explicit rising Action catches\n";
}
