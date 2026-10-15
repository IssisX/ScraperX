#pragma once
#include "sim/simulation/constants.hpp"

// Public-input player leg of the forklift route. Include after the route
// fixture's require/tick/observe helpers; no privileged body-state access.
namespace {
void walk_to(Simulation &s, double x, double z, double speed = .35) {
    for (unsigned i = 0; i < 900; ++i) {
        const auto p = s.snapshot().player_position;
        const auto v = s.snapshot().player_linear_velocity;
        if (s.snapshot().player_grounded && std::hypot(x - p.x, z - p.z) < .07 &&
            std::hypot(v.x, v.z) < .15) {
            require(s.set_move_input(0, 0), "stop walking through public intent");
            tick(s, 20);
            return;
        }
        const double dx = x - p.x, dz = z - p.z;
        const double ix = std::clamp(dx - .45 * v.x, -speed, speed);
        const double iz = std::clamp(dz - .45 * v.z, -speed, speed);
        require(s.set_move_input(ix, iz), "route walking intent accepted");
        tick(s);
    }
    observe(s, "WALK_FAILED");
    require(false, "ordinary movement reaches next route approach");
}
void climb_step(Simulation &s, std::uint64_t entity, double top_y) {
    require(s.set_facing(0, -1) && s.set_move_input(0, 0), "face actual cargo lip");
    require(s.request_traversal(), "request physical pull-up at verified cargo geometry");
    const auto before = s.snapshot().foot_transfer_count;
    bool completed = false;
    for (unsigned i = 0; i < 900; ++i) {
        tick(s);
        if (s.snapshot().traversal_state == TraversalState::None &&
            s.snapshot().player_grounded && s.snapshot().support_entity_id == entity &&
            s.snapshot().player_position.y > top_y + .85) {
            completed = true;
            break;
        }
        if (i % 180 == 0) {
            observe(s, "PLAYER_PULLUP", entity);
            std::cout << "TRANSFER state=" << unsigned(s.snapshot().traversal_state)
                      << " phase=" << unsigned(s.snapshot().traversal_phase)
                      << " hand_N=" << s.snapshot().actual_hand_load_n
                      << " foot_N=" << s.snapshot().actual_foot_load_n << std::endl;
        }
    }
    require(completed && s.snapshot().foot_transfer_count > before,
            "finite hands and real cargo footing complete physical transfer");
    for (unsigned i = 0; i < 90; ++i) {
        tick(s);
        require(s.snapshot().player_grounded && s.snapshot().support_entity_id == entity,
                "actual loaded cargo supports the player after hands release");
    }
    observe(s, "PLAYER_CARGO_FOOTING", entity);
}
void ascend_stack(Simulation &s) {
    // Leave the truck on foot and walk around real solids. The single initial
    // grade stage in board() is the last debug position write in this replay.
    require(s.set_forklift_input(0, 0, 0, 0, 1), "park with finite native brakes");
    walk_to(s, -102, s.snapshot().player_position.z);
    require(!s.forklift_state().controls_reachable, "walking off platform revokes controls");
    const auto a = s.kit_body_position(s.kit_body_index(2444));
    const auto b = s.kit_body_position(s.kit_body_index(2445));
    walk_to(s, -102, a.z + 1.75);
    walk_to(s, a.x, a.z + 1.65);
    climb_step(s, 2444, a.y + 1.5);
    climb_step(s, 2445, b.y + 1.5);

    require(s.set_facing(0, -1) && s.set_move_input(0, -.9), "run across actual upper crate");
    bool jumped = false, action = false, received = false;
    for (unsigned i = 0; i < 1200; ++i) {
        const auto state = s.snapshot();
        if (!jumped && state.player_position.z < b.z - .35) {
            require(state.player_grounded && state.support_entity_id == 2445,
                    "gallery takeoff starts on actual upper cargo");
            require(s.request_jump(), "finite sprint-jump toward real gallery lip");
            jumped = true;
        }
        const auto &left = state.traversal_left_hand;
        const auto &right = state.traversal_right_hand;
        const double dx = .5 * (left.x + right.x) - state.player_position.x;
        const double dy = .5 * (left.y + right.y) - state.player_position.y -
                          scraperx::sim::detail::kClimbHandMid;
        const double dz = .5 * (left.z + right.z) - state.player_position.z;
        // Jump/reach earns the actual hang first. A separate, fresh Action
        // requests the pull-up once those real hands are within transfer
        // reach; an earlier rejected airborne press cannot stand in for it.
        if (jumped && !action && state.traversal_state == TraversalState::Hanging &&
            state.traversal_hand_constraint_count == 2 && dx * dx + dy * dy + dz * dz < 1.0) {
            observe(s, "GALLERY_PULLUP_ACTION");
            require(s.request_traversal(), "request pull-up after actual reachable gallery catch");
            require(s.set_move_input(0, 0), "hold finite pull-up without onward run-through");
            action = true;
        }
        tick(s);
        const auto after = s.snapshot();
        if (after.player_grounded && after.support_entity_id == 1440 &&
            after.player_position.y > 6.75 && after.traversal_state == TraversalState::None) {
            received = true;
            break;
        }
        if (i % 180 == 0) observe(s, "PLAYER_GALLERY_TRANSFER");
    }
    observe(s, "PLAYER_GALLERY_RESULT");
    require(jumped && action && received, "cargo-built route reaches actual gallery footing");
    for (unsigned i = 0; i < 90; ++i) {
        tick(s);
        require(s.snapshot().player_grounded && s.snapshot().support_entity_id == 1440,
                "receiving gallery supports player after actual hand release");
    }
    // Continue beyond the receiving patch, then exercise the actual grade
    // exit and return. Neither arrival nor retry is a scripted world reset.
    walk_to(s, -100, -95.8);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == 1440,
            "ordinary movement leaves landing patch across receiving gallery");
    observe(s, "GALLERY_ONWARD_WALK");
    walk_to(s, -100, -98);
    require(s.snapshot().player_grounded && s.snapshot().player_position.y < 1.0,
            "ordinary gallery departure lands on real grade");
    walk_to(s, -102, -98);
    walk_to(s, -102, a.z + 1.75);
    walk_to(s, a.x, a.z + 1.65);
    require(s.snapshot().player_grounded && s.snapshot().player_position.y < 1.0 &&
            std::abs(s.kit_body_position(s.kit_body_index(2445)).y - b.y) < .03,
            "actual walking returns to intact cargo route for retry without reset");
    observe(s, "GRADE_RETRY_APPROACH");
}
} // namespace
