#include "sim/physics_internal.hpp"

namespace scraperx::sim {

Simulation::Simulation(const InitialSpawn initial_spawn)
    : physics_world_(std::make_unique<PhysicsWorld>(initial_spawn)) {
    snapshot_ = physics_world_->state();
    snapshot_.fixed_step_seconds = kFixedStepSeconds;
    previous_player_position_ = snapshot_.player_position;
}

Simulation::~Simulation() = default;

bool Simulation::set_move_input(double world_x, double world_z) noexcept {
    if (!std::isfinite(world_x) || !std::isfinite(world_z)) {
        return false;
    }

    const double length = std::hypot(world_x, world_z);
    if (length > 1.0) {
        world_x /= length;
        world_z /= length;
    }
    move_input_x_ = world_x;
    move_input_z_ = world_z;
    return true;
}

bool Simulation::set_facing(const double world_x, const double world_z) noexcept {
    if (!std::isfinite(world_x) || !std::isfinite(world_z)) {
        return false;
    }
    const double length = std::hypot(world_x, world_z);
    if (!(length > 1.0e-6)) {
        return false;
    }
    facing_x_ = world_x / length;
    facing_z_ = world_z / length;
    return true;
}

bool Simulation::request_jump() noexcept {
    jump_requested_ = true;
    return true;
}

bool Simulation::request_traversal() noexcept {
    if (snapshot_.traversal_state == TraversalState::Mantling ||
        snapshot_.traversal_state == TraversalState::Vaulting) {
        return false;
    }
    traversal_requested_ = true;
    return true;
}

bool Simulation::request_release() noexcept {
    const bool holding = snapshot_.traversal_state == TraversalState::Hanging ||
                         snapshot_.traversal_state == TraversalState::Climbing;
    const bool standing =
        snapshot_.traversal_state == TraversalState::None && snapshot_.player_grounded;
    if (!holding && !standing) {
        return false;
    }
    release_requested_ = true;
    return true;
}

bool Simulation::request_parachute() noexcept {
    parachute_toggle_requested_ = true;
    return true;
}

bool Simulation::set_crouch_input(const bool held) noexcept {
    crouch_input_ = held;
    return true;
}

bool Simulation::set_sprint_input(const bool held) noexcept {
    sprint_input_ = held;
    return true;
}

bool Simulation::request_pick_up() noexcept {
    pick_up_requested_ = true;
    return true;
}

bool Simulation::request_set_down() noexcept {
    set_down_requested_ = true;
    return true;
}

bool Simulation::request_rig() noexcept {
    rig_requested_ = true;
    return true;
}

namespace {

[[nodiscard]] Vector3 to_sim_vector(const JPH::Vec3 value) noexcept {
    return {value.GetX(), value.GetY(), value.GetZ()};
}

[[nodiscard]] Quaternion to_sim_quaternion(const JPH::Quat value) noexcept {
    return {value.GetX(), value.GetY(), value.GetZ(), value.GetW()};
}

} // namespace

std::uint32_t Simulation::kit_body_count() const noexcept {
    return physics_world_->kit().body_count();
}

std::uint64_t Simulation::kit_body_entity(const std::uint32_t body) const noexcept {
    return physics_world_->kit().body_entity(kit::BodyIndex{body});
}

bool Simulation::kit_body_dynamic(const std::uint32_t body) const noexcept {
    return physics_world_->kit().body_dynamic(kit::BodyIndex{body});
}

bool Simulation::kit_body_enabled(const std::uint32_t body) const noexcept {
    return physics_world_->kit().body_enabled(kit::BodyIndex{body});
}

std::uint32_t Simulation::kit_body_part_count(const std::uint32_t body) const noexcept {
    return static_cast<std::uint32_t>(
        physics_world_->kit().body_parts(kit::BodyIndex{body}).size());
}

KitPart Simulation::kit_body_part(const std::uint32_t body, const std::uint32_t part) const noexcept {
    const std::vector<kit::Part> &parts = physics_world_->kit().body_parts(kit::BodyIndex{body});
    if (part >= parts.size()) {
        return {};
    }
    const kit::Part &source = parts[part];
    return {to_sim_vector(source.half), to_sim_vector(source.offset),
            to_sim_quaternion(source.rotation), static_cast<std::uint8_t>(source.material),
            static_cast<std::uint8_t>(source.shape), source.inner_radius};
}

Vector3 Simulation::kit_body_position(const std::uint32_t body) const noexcept {
    return to_sim_vector(physics_world_->kit().body_position(kit::BodyIndex{body}));
}

Vector3 Simulation::kit_body_center_of_mass(const std::uint32_t body) const noexcept {
    return to_sim_vector(physics_world_->kit().body_center_of_mass(kit::BodyIndex{body}));
}

Quaternion Simulation::kit_body_rotation(const std::uint32_t body) const noexcept {
    return to_sim_quaternion(physics_world_->kit().body_rotation(kit::BodyIndex{body}));
}

Vector3 Simulation::kit_body_velocity(const std::uint32_t body) const noexcept {
    return to_sim_vector(physics_world_->kit().body_velocity(kit::BodyIndex{body}));
}

double Simulation::kit_body_mass(const std::uint32_t body) const noexcept {
    return physics_world_->kit().body_mass(kit::BodyIndex{body});
}

std::uint32_t Simulation::kit_body_index(const std::uint64_t entity) const noexcept {
    return physics_world_->kit().body_for_entity(entity).value;
}

std::uint32_t Simulation::kit_cable_count() const noexcept {
    return physics_world_->kit().cable_count();
}

std::uint32_t Simulation::kit_cable_points(const std::uint32_t cable, Vector3 *out,
                                           const std::uint32_t capacity) const noexcept {
    if (out == nullptr) {
        return 0;
    }
    std::vector<JPH::RVec3> points;
    physics_world_->kit().cable_polyline(cable, points);
    const auto count = static_cast<std::uint32_t>(std::min<std::size_t>(points.size(), capacity));
    for (std::uint32_t index = 0; index < count; ++index) {
        out[index] = to_sim_vector(points[index]);
    }
    return count;
}

std::uint32_t Simulation::kit_bin_count() const noexcept {
    return physics_world_->kit().bin_count();
}

KitBin Simulation::kit_bin(const std::uint32_t bin) const noexcept {
    const kit::Kit &kit = physics_world_->kit();
    KitBin out;
    if (bin >= kit.bin_count()) {
        return out;
    }
    const kit::BinIndex index{bin};
    out.body = kit.bin_body(index).value;
    out.contents_kg = kit.bin_contents(index);
    out.capacity_kg = kit.bin_capacity(index);
    out.water = kit.bin_water(index);
    JPH::RVec3 from;
    JPH::RVec3 to;
    out.flowing = kit.bin_stream(index, from, to);
    if (out.flowing) {
        out.stream_from = to_sim_vector(from);
        out.stream_to = to_sim_vector(to);
    }
    return out;
}

std::uint32_t Simulation::kit_pile_count() const noexcept {
    return static_cast<std::uint32_t>(physics_world_->kit().piles().size());
}

KitPile Simulation::kit_pile(const std::uint32_t pile) const noexcept {
    const auto &piles = physics_world_->kit().piles();
    if (pile >= piles.size()) {
        return {};
    }
    return {to_sim_vector(piles[pile].at), piles[pile].kg};
}

std::uint32_t Simulation::kit_pool_count() const noexcept {
    return physics_world_->kit().pool_count();
}

KitPool Simulation::kit_pool(const std::uint32_t pool) const noexcept {
    const kit::Kit &kit = physics_world_->kit();
    KitPool out;
    if (pool >= kit.pool_count()) {
        return out;
    }
    const kit::PoolIndex index{pool};
    JPH::Vec3 low;
    JPH::Vec3 high;
    kit.pool_box(index, low, high);
    out.min_corner = to_sim_vector(JPH::RVec3(low));
    out.max_corner = to_sim_vector(JPH::RVec3(high));
    out.level_m = kit.pool_level(index);
    out.water_kg = kit.pool_water(index);
    return out;
}

std::uint32_t Simulation::kit_spout_count() const noexcept {
    return physics_world_->kit().pipe_count();
}

KitSpout Simulation::kit_spout(const std::uint32_t spout) const noexcept {
    const kit::Kit &kit = physics_world_->kit();
    KitSpout out;
    JPH::RVec3 from;
    JPH::RVec3 to;
    if (spout < kit.pipe_count() && kit.pipe_stream(kit::PipeIndex{spout}, from, to)) {
        out.pouring = true;
        out.from = to_sim_vector(from);
        out.to = to_sim_vector(to);
    }
    return out;
}

WetState Simulation::wet_state() const noexcept {
    const kit::Kit &kit = physics_world_->kit();
    const scraperx::sim::bands::WetIsolation &wet = physics_world_->wet();
    WetState out;
    out.d_pipe_whole = kit.pipe_whole(wet.d_fill);
    out.d_fill_kg_s = kit.pipe_flow(wet.d_fill);
    out.d_tank_kg = kit.pool_water(wet.d_tank);
    out.d_tube_kg = kit.pool_water(wet.d_tube);
    out.d_tank_level = kit.pool_level(wet.d_tank);
    out.d_tube_level = kit.pool_level(wet.d_tube);
    out.d_platform_travel = kit.guide_travel(wet.d_guide);
    out.d_valve_angle = kit.lever_angle(wet.d_valve);
    out.d_drain_angle = kit.lever_angle(wet.d_drain);
    out.dump_angle = kit.lever_angle(wet.header_dump);
    out.e_door_latched = kit.catch_latched(wet.e_door_catch);
    out.e_door_angle = kit.lever_angle(wet.e_door_hinge);
    out.e_catch_latched = kit.catch_latched(wet.e_catch);
    out.e_duct_pa = kit.cell_pressure(wet.e_duct_cell);
    out.e_cab_pa = kit.cell_pressure(wet.e_cab_cell);
    out.e_cab_travel = kit.guide_travel(wet.e_cab_guide);
    out.e_chiller_travel = kit.guide_travel(wet.e_chiller_guide);
    out.e_bucket_kg = kit.bin_contents(wet.e_bucket_bin);
    out.f_catch_latched = kit.catch_latched(wet.f_catch);
    out.f_hose_coupled = kit.rope_end_entity(wet.f_line) != Simulation::kWetFHoseEntityId;
    out.f_platform_travel = kit.guide_travel(wet.f_platform_guide);
    out.f_accumulator_travel = kit.guide_travel(wet.f_accumulator_guide);
    out.header_kg = kit.pool_water(wet.header);
    out.drained_kg = kit.drained();
    return out;
}

ShopState Simulation::shop_state() const noexcept {
    const kit::Kit &kit = physics_world_->kit();
    const scraperx::sim::bands::PlateShop &shop = physics_world_->shop();
    ShopState out;
    out.g_rope_on_eye = kit.rope_end_entity(shop.g_rope) == Simulation::kShopGPlatformEntityId;
    out.g_tower_latched = kit.catch_latched(shop.g_catch);
    out.g_platform_travel = kit.guide_travel(shop.g_platform_guide);
    out.g_tower_travel = kit.guide_travel(shop.g_tower_guide);
    out.h_girder_latched = kit.catch_latched(shop.h_girder_catch);
    out.h_trolley_latched = kit.catch_latched(shop.h_trolley_catch);
    out.h_girder_angle = kit.lever_angle(shop.h_girder_hinge);
    out.h_platform_travel = kit.guide_travel(shop.h_platform_guide);
    out.i_rope_on_eye = kit.rope_end_entity(shop.i_rope) == Simulation::kShopICageEntityId;
    out.i_domino_latched = kit.catch_latched(shop.i_domino_catch);
    out.i_monolith_latched = kit.catch_latched(shop.i_monolith_catch);
    out.i_domino_angle = kit.lever_angle(shop.i_domino_hinge);
    out.i_trip_angle = kit.lever_angle(shop.i_trip);
    out.i_monolith_angle = kit.lever_angle(shop.i_monolith_hinge);
    out.i_cage_travel = kit.guide_travel(shop.i_cage_guide);
    return out;
}

CraneState Simulation::crane_state() const noexcept {
    const kit::Kit &kit = physics_world_->kit();
    const scraperx::sim::bands::FacadeCrane &crane = physics_world_->crane();
    CraneState out;
    out.j_rail_whole = kit.rail_whole(crane.j_traveler_guide);
    out.j_wagon_latched = kit.catch_latched(crane.j_wagon_catch);
    out.j_traveler_travel = kit.guide_travel(crane.j_traveler_guide);
    out.j_wagon_travel = kit.guide_travel(crane.j_wagon_guide);
    out.k_rope_on_eye = kit.rope_end_entity(crane.k_rope) == Simulation::kCraneKCageEntityId;
    out.k_jib_latched = kit.catch_latched(crane.k_jib_catch);
    out.k_jib_angle = kit.lever_angle(crane.k_jib_hinge);
    out.k_cage_travel = kit.guide_travel(crane.k_cage_guide);
    out.l_clutch_in = kit.clutch_in(crane.l_rope);
    out.l_weight_latched = kit.catch_latched(crane.l_weight_catch);
    out.l_cart_latched = kit.catch_latched(crane.l_cart_catch);
    out.l_cart_travel = kit.guide_travel(crane.l_cart_guide);
    out.l_cab_travel = kit.guide_travel(crane.l_cab_guide);
    return out;
}

StackState Simulation::stack_state() const noexcept {
    const kit::Kit &kit = physics_world_->kit();
    const scraperx::sim::bands::Stack &stack = physics_world_->stack();
    StackState out;
    out.s1_cage_travel = kit.guide_travel(stack.s1_cage_guide);
    out.s1_cage_peak_speed = kit.guide_peak_speed(stack.s1_cage_guide);
    out.s1_bucket_travel = kit.guide_travel(stack.s1_bucket_guide);
    out.s1_bucket_water_kg = kit.bin_contents(stack.s1_bin);
    out.s1_tank_water_kg = kit.pool_water(stack.s1_tank);
    out.s1_valve_angle = kit.lever_angle(stack.s1_lever);
    out.s1_catch_latched = kit.catch_latched(stack.s1_catch);
    out.s1_rope_tension = kit.rope_tension(stack.s1_rope);
    out.s2_cage_travel = kit.guide_travel(stack.s2_cage_guide);
    out.s2_cage_peak_speed = kit.guide_peak_speed(stack.s2_cage_guide);
    out.s2_cart_travel = kit.guide_travel(stack.s2_cart_guide);
    out.s2_beam_angle = kit.lever_angle(stack.s2_beam_hinge);
    out.s2_chock_angle = kit.lever_angle(stack.s2_chock_lever);
    out.s2_chock_latched = kit.catch_latched(stack.s2_catch);
    out.s2_cage_latched = kit.catch_latched(stack.s2_cage_catch);
    out.s2_rope_tension = kit.rope_tension(stack.s2_rope);
    out.s3_cage_travel = kit.guide_travel(stack.s3_cage_guide);
    out.s3_cage_peak_speed = kit.guide_peak_speed(stack.s3_cage_guide);
    out.s3_car_travel = kit.guide_travel(stack.s3_car_guide);
    out.s3_brake_angle = kit.lever_angle(stack.s3_brake_lever);
    out.s3_brake_latched = kit.catch_latched(stack.s3_catch);
    out.s3_cage_latched = kit.catch_latched(stack.s3_cage_catch);
    out.s3_rope_tension = kit.rope_tension(stack.s3_rope);
    return out;
}

bool Simulation::set_slingshot_input(const double draw, const double yaw,
                                     const double elevation) noexcept {
    if (!std::isfinite(draw) || !std::isfinite(yaw) || !std::isfinite(elevation)) {
        return false;
    }
    sling_draw_ = std::clamp(draw, 0.0, 1.0);
    sling_yaw_ = yaw;
    sling_elevation_ = elevation;
    return true;
}

bool Simulation::request_slingshot_action() noexcept {
    sling_action_requested_ = true;
    return true;
}

bool Simulation::request_slingshot_drop() noexcept {
    sling_drop_requested_ = true;
    return true;
}

SlingshotSnapshot Simulation::slingshot_state() const noexcept {
    SlingshotSnapshot result;
    const Slingshot *launcher = physics_world_->slingshot();
    if (launcher == nullptr) {
        return result;
    }
    const Slingshot::State &state = launcher->state();
    const auto as_vec = [](JPH::RVec3Arg value) {
        return Vector3{value.GetX(), value.GetY(), value.GetZ()};
    };
    result.available = true;
    result.leather_deflection_m = state.leather_deflection_m;
    result.leather_energy_j = state.leather_energy_j;
    result.max_draw_m = Slingshot::kMaximumDrawM;
    result.max_source_power_w = Slingshot::kPlayerPowerW;
    result.band_rest_m = state.band_rest_m;
    result.neutral_position = as_vec(Slingshot::neutral_position());
    result.retrieval_control_position = as_vec(Slingshot::retrieval_control_position());
    result.station_available = state.station_available;
    result.seated = state.seated;
    result.drawing = state.drawing;
    result.released = state.released;
    result.can_retrieve = state.can_retrieve;
    result.recovering = state.recovering;
    result.guided_launch = state.guided_launch;
    result.track_exit = state.track_exit;
    result.release_ready = state.release_ready;
    result.ledger_valid = state.ledger_valid;
    result.pouch_pair_excluded = state.pouch_pair_excluded;
    result.aim_ready = state.aim_ready;
    result.aim_locked = state.aim_locked;
    result.aim_control_work_j = state.aim_control_work_j;
    result.aim_source_power_w = state.aim_source_power_w;
    result.target_yaw_rad = state.target_yaw_rad;
    result.target_elevation_rad = state.target_elevation_rad;
    result.launch_track_start = as_vec(state.launch_track_start);
    result.launch_track_end = as_vec(state.launch_track_end);
    result.retrieval_work_j = state.retrieval_work_j;
    result.retrieval_source_power_w = state.retrieval_source_power_w;
    result.draw_m = state.draw_m;
    result.energy_j = state.energy_j;
    result.work_j = state.work_j;
    result.source_power_w = state.source_power_w;
    result.energy_residual_j = state.energy_residual_j;
    result.yaw_rad = state.yaw_rad;
    result.elevation_rad = state.elevation_rad;
    result.launch_count = state.launch_count;
    result.anchor_left = as_vec(state.anchor_left);
    result.anchor_right = as_vec(state.anchor_right);
    result.pouch_position = as_vec(state.pouch_position);
    result.harness_rest_local = as_vec(state.harness_rest_local);
    const JPH::Vec3 seat = Slingshot::leather_surface(
        static_cast<float>(state.harness_rest_local.GetX() / 1.68 + 0.5),
        static_cast<float>(state.harness_rest_local.GetZ() / 1.30 + 0.5),
        static_cast<float>(state.leather_deflection_m));
    result.seat_surface_position = as_vec(state.pouch_position + JPH::RVec3(seat));
    result.rider_specific_acceleration = physics_world_->rider_specific_acceleration();
    return result;
}

std::vector<Vector3> Simulation::slingshot_prediction() const {
    std::vector<Vector3> points;
    const Slingshot *launcher = physics_world_->slingshot();
    if (launcher == nullptr) {
        return points;
    }
    for (const JPH::RVec3 point : launcher->prediction()) {
        points.push_back(Vector3{point.GetX(), point.GetY(), point.GetZ()});
    }
    return points;
}

void Simulation::step_fixed() noexcept {
    const std::uint64_t previous_death_count = snapshot_.death_count;
    const bool previous_crouched = snapshot_.player_crouched;
    previous_player_position_ = snapshot_.player_position;
    const double next_time_seconds =
        static_cast<double>(tick_index_ + 1) * kFixedStepSeconds;

    PhysicsWorld::StepCommands commands;
    commands.move_input_x = move_input_x_;
    commands.move_input_z = move_input_z_;
    commands.facing_x = facing_x_;
    commands.facing_z = facing_z_;
    commands.jump_requested = jump_requested_;
    commands.traversal_requested = traversal_requested_;
    commands.release_requested = release_requested_;
    commands.crouch_held = crouch_input_;
    commands.sprint_held = sprint_input_;
    commands.pick_up_requested = pick_up_requested_;
    commands.set_down_requested = set_down_requested_;
    commands.rig_requested = rig_requested_;
    commands.parachute_toggle_requested = parachute_toggle_requested_;
    commands.sling_draw = sling_draw_;
    commands.sling_yaw = sling_yaw_;
    commands.sling_elevation = sling_elevation_;
    commands.sling_action = sling_action_requested_;
    commands.sling_drop = sling_drop_requested_;

    physics_world_->step(commands, static_cast<float>(kFixedStepSeconds), next_time_seconds);
    jump_requested_ = false;
    traversal_requested_ = false;
    release_requested_ = false;
    parachute_toggle_requested_ = false;
    pick_up_requested_ = false;
    set_down_requested_ = false;
    rig_requested_ = false;
    sling_action_requested_ = false;
    sling_drop_requested_ = false;
    ++tick_index_;

    snapshot_ = physics_world_->state();
    const double render_dx = snapshot_.player_position.x - previous_player_position_.x;
    const double render_dy = snapshot_.player_position.y - previous_player_position_.y;
    const double render_dz = snapshot_.player_position.z - previous_player_position_.z;
    if (snapshot_.death_count != previous_death_count ||
        snapshot_.player_crouched != previous_crouched ||
        render_dx * render_dx + render_dy * render_dy + render_dz * render_dz > 1.0) {
        previous_player_position_ = snapshot_.player_position;
    }
    snapshot_.tick_index = tick_index_;
    snapshot_.simulation_time_seconds =
        static_cast<double>(tick_index_) * kFixedStepSeconds;
    snapshot_.fixed_step_seconds = kFixedStepSeconds;
}

AdvanceResult Simulation::advance_frame(const double frame_delta_seconds) noexcept {
    if (!std::isfinite(frame_delta_seconds) || frame_delta_seconds < 0.0 ||
        frame_delta_seconds > kMaximumAcceptedFrameDeltaSeconds) {
        return {};
    }

    const double accumulated = remainder_seconds_ + frame_delta_seconds;
    const double step_epsilon = kFixedStepSeconds * 1.0e-9;
    const double due_as_double = std::floor((accumulated + step_epsilon) / kFixedStepSeconds);

    if (due_as_double < 0.0 ||
        due_as_double > static_cast<double>(std::numeric_limits<std::uint32_t>::max()) ||
        due_as_double > static_cast<double>(std::numeric_limits<std::uint64_t>::max() - tick_index_)) {
        return {};
    }

    auto due = static_cast<std::uint32_t>(due_as_double);
    remainder_seconds_ = accumulated - static_cast<double>(due) * kFixedStepSeconds;

    if (remainder_seconds_ < 0.0 && remainder_seconds_ > -step_epsilon) {
        remainder_seconds_ = 0.0;
    }
    if (remainder_seconds_ >= kFixedStepSeconds &&
        remainder_seconds_ - kFixedStepSeconds < step_epsilon) {
        remainder_seconds_ = 0.0;
        ++due;
    }

    for (std::uint32_t step = 0; step < due; ++step) {
        step_fixed();
    }
    return {true, due};
}

Snapshot Simulation::snapshot() const noexcept {
    Snapshot result = snapshot_;
    result.interpolation_alpha = remainder_seconds_ / kFixedStepSeconds;
    return result;
}

Vector3 Simulation::render_player_position() const noexcept {
    const double alpha = std::clamp(remainder_seconds_ / kFixedStepSeconds, 0.0, 1.0);
    return {
        previous_player_position_.x +
            (snapshot_.player_position.x - previous_player_position_.x) * alpha,
        previous_player_position_.y +
            (snapshot_.player_position.y - previous_player_position_.y) * alpha,
        previous_player_position_.z +
            (snapshot_.player_position.z - previous_player_position_.z) * alpha,
    };
}

} // namespace scraperx::sim
