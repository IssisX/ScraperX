// Bounded read-only observer for the first forklift boarding divergence.
// No production state is written except through the ordinary public driver.
#include "sim/simulation/world.hpp"
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <type_traits>
#include <utility>

using namespace scraperx::sim;
namespace boarding_diagnostic {
// Explicit-instantiation access is confined to this diagnostic translation unit.
// It avoids changing production access, headers, layouts or snapshot authority.
template <auto Member, class Tag> struct ReadMember {
    friend auto member(Tag) { return Member; }
};
struct WorldTag { friend auto member(WorldTag); };
template struct ReadMember<&Simulation::physics_world_, WorldTag>;
using World = std::remove_reference_t<decltype(*(
    std::declval<Simulation &>().*member(WorldTag{})))>;
#define READ_TAG(Name, Field) \
    struct Name { friend auto member(Name); }; \
    template struct ReadMember<&World::Field, Name>
READ_TAG(PhysicsTag, physics_system_);
READ_TAG(PlayerTag, player_id_);
READ_TAG(PendingTag, physical_step_pending_);
READ_TAG(StartFeetTag, physical_step_start_feet_y_);
READ_TAG(ContactTag, support_sample_);
READ_TAG(SerialTag, contact_solve_serial_);
READ_TAG(CastTag, cast_capsule);
READ_TAG(ClearTag, capsule_pose_is_clear);
#undef READ_TAG

void require(bool result, const char *message) {
    if (!result) { std::cerr << "FAIL BOARDING_DIAGNOSTIC " << message << '\n'; std::exit(1); }
}
void vec(JPH::Vec3Arg value) {
    std::cout << value.GetX() << ',' << value.GetY() << ',' << value.GetZ();
}
void observe(Simulation &simulation, unsigned driver_tick, double move_x, double move_z) {
    auto &world = *(simulation.*member(WorldTag{}));
    auto &bodies = (world.*member(PhysicsTag{})).GetBodyInterface();
    const auto player = world.*member(PlayerTag{});
    const auto &state = simulation.snapshot();
    const auto &contact = world.*member(ContactTag{});
    const auto at = bodies.GetPosition(player);
    std::cout << "[BOARDING_CAUSE] tick=" << driver_tick
              << " solve=" << world.*member(SerialTag{})
              << " p=" << state.player_position.x << ',' << state.player_position.y << ',' << state.player_position.z
              << " v=" << state.player_linear_velocity.x << ',' << state.player_linear_velocity.y << ',' << state.player_linear_velocity.z
              << " input=" << move_x << ',' << move_z
              << " grounded=" << state.player_grounded << " support=" << state.support_entity_id
              << " contact_body=" << contact.body_id.GetIndexAndSequenceNumber()
              << " contact_leaf=" << contact.subshape.GetValue() << " contact_normal=" << contact.normal_y
              << " contact_point=" << contact.contact_point.x << ',' << contact.contact_point.y << ',' << contact.contact_point.z
              << " pending=" << world.*member(PendingTag{})
              << " start_feet=" << world.*member(StartFeetTag{})
              << " step_count=" << state.step_up_count << " pose_count=" << state.step_up_pose_count
              << " foot=" << state.foot_push_active << " foot_load=" << state.foot_push_last_load_n
              << " traversal=" << unsigned(state.traversal_state)
              << " hand=" << state.traversal_left_hand.x << ',' << state.traversal_left_hand.y << ',' << state.traversal_left_hand.z
              << " ledge=" << state.traversal_ledge_point.x << ',' << state.traversal_ledge_point.y << ',' << state.traversal_ledge_point.z
              << " target=" << state.traversal_target_point.x << ',' << state.traversal_target_point.y << ',' << state.traversal_target_point.z;
    const JPH::Vec3 horizontal(float(move_x * kPlayerMaximumRelativeSpeed), 0,
                              float(move_z * kPlayerMaximumRelativeSpeed));
    const float speed = horizontal.Length();
    if (speed >= kStepMinimumSpeed) {
        const auto ahead = horizontal / speed *
            (speed * float(Simulation::kFixedStepSeconds) + kStepLookahead);
        float fraction = 1;
        JPH::Vec3 normal = JPH::Vec3::sZero();
        JPH::BodyID obstacle;
        JPH::SubShapeID leaf;
        const bool low = (world.*member(CastTag{}))(at, ahead, fraction, normal, &obstacle, &leaf, nullptr);
        std::cout << " low=" << low;
        if (low) {
            std::cout << " obstacle=" << bodies.GetUserData(obstacle)
                      << " obstacle_leaf=" << leaf.GetValue() << " low_fraction=" << fraction
                      << " low_normal=";
            vec(normal);
            const auto raised = at + JPH::Vec3(0, kStepMaximumHeight, 0);
            const bool clear = (world.*member(ClearTag{}))(raised);
            const bool blocked = (world.*member(CastTag{}))(raised, ahead, fraction, normal, nullptr, nullptr, nullptr);
            const bool down = (world.*member(CastTag{}))(raised + ahead,
                JPH::Vec3(0, -(kStepMaximumHeight + kStepMinimumHeight), 0),
                fraction, normal, &obstacle, &leaf, nullptr);
            std::cout << " raised_clear=" << clear << " raised_blocked=" << blocked << " down=" << down;
            if (down) {
                std::cout << " down_entity=" << bodies.GetUserData(obstacle)
                          << " down_leaf=" << leaf.GetValue() << " down_normal=";
                vec(normal);
                std::cout << " rise=" << kStepMaximumHeight -
                    (kStepMaximumHeight + kStepMinimumHeight) * fraction;
            }
        }
    }
    std::cout << std::endl;
}
} // namespace boarding_diagnostic

int main() {
    using namespace boarding_diagnostic;
    Simulation simulation;
    require(simulation.debug_restart_at({-102, .9, -57.65}), "initial grade");
    for (unsigned tick = 0; tick < 45; ++tick)
        require(simulation.advance_frame(Simulation::kFixedStepSeconds).accepted, "settle");
    require(simulation.set_facing(1, 0), "face doorway");
    const auto initial_poses = simulation.snapshot().step_up_pose_count;
    std::cout << std::setprecision(9);
    for (unsigned tick = 0; tick < 540; ++tick) {
        const auto truck = simulation.kit_body_position(simulation.kit_body_index(2440));
        const auto position = simulation.snapshot().player_position;
        const auto velocity = simulation.snapshot().player_linear_velocity;
        const double x = std::clamp(1.2 * (truck.x - position.x) - .45 * velocity.x, -.3, .3);
        const double z = std::clamp(1.2 * (truck.z - 1.65 - position.z) - .45 * velocity.z, -.15, .15);
        require(simulation.set_move_input(x, z), "ordinary walking input");
        // This samples before the actual owner runs; it never calls locomotion
        // or starts an actuator. The next sample captures the solved result.
        if (tick == 0 || position.x > -101.4) observe(simulation, tick, x, z);
        require(simulation.advance_frame(Simulation::kFixedStepSeconds).accepted, "walk tick");
        require(simulation.snapshot().death_count == 0, "no death");
        require(simulation.snapshot().step_up_pose_count == initial_poses, "no pose step");
        if (simulation.snapshot().traversal_state != TraversalState::None) {
            observe(simulation, tick + 1, x, z);
            require(false, "first traversal stole ordinary boarding; transition captured");
        }
        if (simulation.forklift_state().controls_reachable) {
            observe(simulation, tick + 1, x, z);
            std::cout << "PASS BOARDING_DIAGNOSTIC ordinary walking reaches actual operator floor\n";
            return 0;
        }
    }
    require(false, "ordinary boarding timed out");
}
