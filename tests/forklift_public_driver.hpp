#pragma once
// Include inside the fixture namespace after require/tick/observe.
// Extracted byte-for-byte from the accepted warehouse cargo driver.

void board(Simulation &s) {
    // Only the initial grade approach is staged. Boarding/operation below use
    // public intent, ordinary foot contact and actual vehicle motion.
    require(s.debug_restart_at({-102, .9, -57.65}), "initial grade approach");
    tick(s, 45);
    require(s.snapshot().player_grounded && !s.forklift_state().controls_reachable,
            "approach begins outside truck on actual grade");
    require(s.set_facing(1, 0), "face actual rear platform from its open side");
    const auto pose_steps_before = s.snapshot().step_up_pose_count;
    // Walk through the real side tread and open guard; a full parkour jump
    // legitimately catches the overhead guard instead of boarding the cab.
    for (unsigned i = 0; i < 270; ++i) {
        const auto target = s.kit_body_position(s.kit_body_index(2440));
        const auto player = s.snapshot().player_position;
        const auto velocity = s.snapshot().player_linear_velocity;
        const double x = std::clamp(1.2 * (target.x - player.x) - .45 * velocity.x, -.3, .3);
        const double z = std::clamp(1.2 * (target.z - 1.65 - player.z) - .45 * velocity.z, -.15, .15);
        require(s.set_move_input(x, z), "boarding direction accepted");
        tick(s);
        if (s.forklift_state().controls_reachable) {
            require(s.set_move_input(0, 0), "stand on platform");
            tick(s, 30);
            observe(s, "BOARDED");
            require(s.forklift_state().controls_reachable, "real platform footing retained");
            require(s.snapshot().step_up_pose_count == pose_steps_before,
                    "boarding uses real finite steps without legacy pose correction");
            return;
        }
    }
    observe(s, "BOARDING_FAILED");
    require(false, "ordinary walking and physical steps reach rear operator platform");
}

// Some routes need only steering/boarding from this shared fixture header.
[[maybe_unused]] void drive_straight_to(Simulation &s, double z, double speed_limit = 1.5,
                       std::uint64_t carried_entity = 0, bool align_over_receiver = false) {
    const auto chassis = s.kit_body_index(2440);
    for (unsigned i = 0; i < 6000; ++i) {
        require(s.forklift_state().controls_reachable, "operator keeps actual platform contact during transport");
        if (carried_entity) {
            const auto load = s.kit_body_position(s.kit_body_index(carried_entity));
            const auto forks = s.kit_body_position(s.kit_body_index(2443));
            require(load.y > .4 && load.z > forks.z && load.z < forks.z + 1.8,
                    "transport retains actual load inside the fork support polygon");
        }
        const auto p = s.kit_body_position(chassis);
        const auto v = s.kit_body_velocity(chassis);
        const double error = z - p.z;
        if (std::abs(error) < .045 && std::abs(v.z) < .06) {
            require(s.set_forklift_input(0, 0, 0, 0, 1), "finite brake at placement");
            tick(s, 30);
            return;
        }
        const double direction = error >= 0 ? 1 : -1;
        const double speed = std::min(.7 * std::abs(error), speed_limit);
        const double relative_speed = v.z * direction;
        const double drive = direction * std::clamp(.08 + .7 * (speed - relative_speed), 0., .55);
        const double brake = relative_speed > speed + .08 || std::abs(error) < .025 ? 1 : 0;
        double steer = 0;
        if (align_over_receiver) {
            require(carried_entity && direction > 0, "placement alignment uses a forward carried load");
            const auto load = s.kit_body_position(s.kit_body_index(carried_entity));
            const auto receiver = s.kit_body_position(s.kit_body_index(2444));
            const auto q = s.kit_body_rotation(chassis);
            const double fx = 2 * (q.x * q.z + q.w * q.y);
            const double fz = 1 - 2 * (q.x * q.x + q.y * q.y);
            require(fz > .9 && load.z - p.z > 2, "placement approaches with the actual load ahead");
            // Aim the carried crate, not just chassis Z: a small residual
            // heading after the turn otherwise drifts the load sideways.
            // Rear-steer kinematics at this forward point give
            // dx_load/dz ~= heading - reach * tan(wheel_angle) / wheelbase.
            // This driver still sends only finite analog steering commands.
            steer = std::clamp(std::atan(1.65 * (fx / fz + .8 * (load.x - receiver.x)) /
                                         (load.z - p.z)) / .9, -.2, .2);
        }
        require(s.set_forklift_input(brake ? 0 : drive, steer, 0, 0, brake), "finite transport command");
        tick(s);
        if (i % 900 == 0) observe(s, "TRANSPORT");
    }
    observe(s, "TRANSPORT_FAILED");
    require(false, "real drive/brakes reaches placement lane");
}

struct Point { double x, z; };

Point forward(Simulation &s) {
    const auto q = s.kit_body_rotation(s.kit_body_index(2440));
    const double x = 2 * (q.x * q.z + q.w * q.y);
    const double z = 1 - 2 * (q.x * q.x + q.y * q.y);
    const double length = std::hypot(x, z);
    require(length > .8, "truck stays physically upright during route operation");
    return {x / length, z / length};
}

void follow(Simulation &s, const std::vector<Point> &path, double direction,
            std::uint64_t carried_entity = 0) {
    // A test driver chooses only the same analog controls as the player.
    // Pure-pursuit geometry never writes a transform, velocity or cargo state.
    require(path.size() >= 2, "maneuver has real path extent");
    const auto chassis = s.kit_body_index(2440);
    unsigned segment = 0;
    double closest_progress = 0;
    for (unsigned i = 0; i < 9000; ++i) {
        require(s.forklift_state().controls_reachable, "operator retains actual contact in steering maneuver");
        const auto p = s.kit_body_position(chassis);
        const auto v = s.kit_body_velocity(chassis);
        const auto f = forward(s);
        const double end_distance = std::hypot(path.back().x - p.x, path.back().z - p.z);
        if (segment + 2 == path.size() && end_distance < .4) {
            require(s.set_forklift_input(0, 0, 0, 0, 1), "stop after actual maneuver");
            tick(s, 90);
            observe(s, "MANEUVER_FINISHED", carried_entity ? carried_entity : 2444);
            return;
        }
        const auto a = path[segment], b = path[segment + 1];
        const double dx = b.x - a.x, dz = b.z - a.z;
        const double length = std::hypot(dx, dz);
        const double projection = std::clamp(((p.x - a.x) * dx + (p.z - a.z) * dz) /
                                             (length * length), 0., 1.);
        closest_progress = std::max(closest_progress, projection);
        if ((projection > .96 || std::hypot(b.x - p.x, b.z - p.z) < .5) && segment + 2 < path.size()) {
            ++segment;
            closest_progress = 0;
            continue;
        }
        Point target{a.x + closest_progress * dx, a.z + closest_progress * dz};
        double lookahead = 1.8;
        unsigned look_segment = segment;
        while (lookahead > 0 && look_segment + 1 < path.size()) {
            const auto end = path[look_segment + 1];
            const double distance = std::hypot(end.x - target.x, end.z - target.z);
            if (distance >= lookahead) {
                target = {target.x + (end.x - target.x) * lookahead / distance,
                          target.z + (end.z - target.z) * lookahead / distance};
                break;
            }
            lookahead -= distance;
            target = end;
            ++look_segment;
        }
        const double tx = target.x - p.x, tz = target.z - p.z;
        const double lateral = f.z * tx - f.x * tz;
        // Rear steer: positive demand rotates the forward axis toward -X.
        const double steer = std::clamp(-std::atan2(2 * 1.65 * lateral, tx * tx + tz * tz) / .9, -1., 1.);
        // Crawl through turns; use the already accepted1.5m/s transport speed
        // on long aligned straights instead of spending most of the replay
        // slowly traversing empty aisle. These remain finite driver inputs.
        const double speed_limit = length > 10 && std::abs(steer) < .08 && end_distance > 5 ? 1.5 : .8;
        const double speed = std::min(speed_limit, .7 * end_distance);
        const double current_speed = direction * (v.x * f.x + v.z * f.z);
        const double brake = current_speed > speed + .08 ? 1 : 0;
        const double drive = direction * std::clamp(.08 + .7 * (speed - current_speed), 0., .4);
        require(s.set_forklift_input(brake ? 0 : drive, steer, 0, 0, brake), "finite rear-steer maneuver input");
        tick(s);
        if (carried_entity) {
            const auto load = s.kit_body_position(s.kit_body_index(carried_entity));
            const auto forks = s.kit_body_position(s.kit_body_index(2443));
            const double lx = load.x - forks.x, lz = load.z - forks.z;
            const double along = lx * f.x + lz * f.z;
            const double across = lx * f.z - lz * f.x;
            require(load.y > .4 && along > .2 && along < 1.8 && std::abs(across) < .25,
                    "steering carries independent load inside real fork polygon");
        }
        if (i % 900 == 0) {
            observe(s, "MANEUVER", carried_entity ? carried_entity : 2444);
            std::cout << "PATH segment=" << segment << " forward=" << f.x << ',' << f.z
                      << " steer=" << steer << " target=" << target.x << ',' << target.z << std::endl;
        }
    }
    observe(s, "MANEUVER_FAILED", carried_entity ? carried_entity : 2444);
    require(false, "real steering reaches cargo aisle within finite drive path");
}
