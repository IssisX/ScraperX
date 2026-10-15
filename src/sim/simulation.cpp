#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

Simulation::Simulation(const InitialSpawn initial_spawn, const WorldContent content)
    : physics_world_(std::make_unique<PhysicsWorld>(initial_spawn, content)) {
    snapshot_ = physics_world_->state();
    snapshot_.fixed_step_seconds = kFixedStepSeconds;
    capture_render_history();
}

Simulation::~Simulation() = default;

bool Simulation::set_slingshot_input(double draw, double yaw, double elevation) noexcept {
    if (!std::isfinite(draw) || !std::isfinite(yaw) || !std::isfinite(elevation)) return false;
    sling_draw_ = std::clamp(draw, 0.0, 1.0);
    sling_yaw_ = std::clamp(yaw, -0.8, 0.8);
    sling_elevation_ = std::clamp(elevation, 0.35, 1.48);
    return true;
}

bool Simulation::request_slingshot_action() noexcept {
    if (!slingshot_state().available) return false;
    sling_action_ = true;
    return true;
}

bool Simulation::request_slingshot_drop() noexcept {
    jump_cancel_requested_ = true;
    if (!slingshot_state().seated && !slingshot_state().recovering) return false;
    sling_drop_ = true;
    return true;
}

SlingshotSnapshot Simulation::slingshot_state() const noexcept {
    SlingshotSnapshot result;
    const auto *launcher = physics_world_->slingshot();
    if (!launcher) return result;
    const auto &state = launcher->state();
    result.tick_index = tick_index_;
    result.simulation_time_seconds = tick_index_ * kFixedStepSeconds;
    result.fixed_step_seconds = kFixedStepSeconds;
    result.rider_specific_acceleration = physics_world_->rider_specific_acceleration();
    result.harness_rest_local = to_vector3(state.harness_rest_local);
    const auto seat = Slingshot::leather_surface(float(state.harness_rest_local.GetX()/1.68 + .5),
        float(state.harness_rest_local.GetZ()/1.30 + .5), float(state.leather_deflection_m));
    result.seat_surface_position = to_vector3(state.pouch_position + JPH::RVec3(seat));
    result.leather_deflection_m = state.leather_deflection_m;
    result.leather_energy_j = state.leather_energy_j;
    result.available = true;
    result.max_draw_m = Slingshot::kMaximumDrawM;
    result.max_source_power_w = Slingshot::kPlayerPowerW;
    result.band_rest_m = state.band_rest_m;
    result.neutral_position = to_vector3(Slingshot::neutral_position());
    result.retrieval_control_position = to_vector3(Slingshot::retrieval_control_position());
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
    result.aim_control_work_j = state.aim_control_work_j;
    result.aim_ready = state.aim_ready;
    result.aim_locked = state.aim_locked;
    result.aim_source_power_w = state.aim_source_power_w;
    result.target_yaw_rad = state.target_yaw_rad;
    result.target_elevation_rad = state.target_elevation_rad;
    result.launch_track_start = to_vector3(state.launch_track_start);
    result.launch_track_end = to_vector3(state.launch_track_end);
    result.retrieval_work_j = state.retrieval_work_j;
    result.retrieval_source_power_w = state.retrieval_source_power_w;
    result.draw_m = state.draw_m;
    result.energy_j = state.energy_j;
    result.work_j = state.work_j;
    result.source_power_w = state.source_power_w;
    result.yaw_rad = state.yaw_rad;
    result.elevation_rad = state.elevation_rad;
    result.dissipated_work_j = state.dissipated_work_j;
    result.kinetic_j = state.kinetic_j;
    result.gravity_j = state.gravity_j;
    result.energy_residual_j = state.energy_residual_j;
    result.anchor_left = to_vector3(state.anchor_left);
    result.anchor_right = to_vector3(state.anchor_right);
    result.pouch_position = to_vector3(state.pouch_position);
    result.launch_count = state.launch_count;
    return result;
}

std::vector<Vector3> Simulation::slingshot_prediction() const {
    return physics_world_->slingshot_prediction();
}

bool Simulation::debug_restart_at(const Vector3 centre) noexcept {
    if (!physics_world_->debug_restart_at(centre)) return false;
    move_input_x_ = move_input_z_ = 0.0;
    jump_requested_ = jump_cancel_requested_ = traversal_requested_ = release_requested_ = false;
    parachute_toggle_requested_ = pick_up_requested_ = set_down_requested_ = rig_requested_ = false;
    crouch_input_ = sprint_input_ = false;
    jib_slew_input_ = jib_hoist_input_ = needle_hoist_input_ = service_lift_input_ = supplied_machine_input_ = 0.0;
    intake_slew_input_ = intake_hoist_input_ = 0.0;
    valve_toggle_requested_ = water_screw_toggle_requested_ = water_lift_valve_toggle_requested_ = false;
    water_lift_release_requested_ = water_lift_reset_requested_ = false;
    intake_sling_release_requested_ = intake_sling_attach_requested_ = false;
    sling_draw_ = 0.0;
    sling_action_ = sling_drop_ = false;
    snapshot_ = physics_world_->state();
    snapshot_.tick_index = tick_index_;
    snapshot_.simulation_time_seconds = tick_index_ * kFixedStepSeconds;
    snapshot_.fixed_step_seconds = kFixedStepSeconds;
    capture_render_history();
    remainder_seconds_ = 0.0;
    return true;
}

bool Simulation::restart_service_lift_attempt() noexcept {
    if (!physics_world_->select_service_lift_retry()) return false;
    return restart_checkpoint();
}

bool Simulation::can_restart_gravity_reclaim_attempt() const noexcept {
    return physics_world_->can_restart_gravity_reclaim_attempt();
}

bool Simulation::restart_gravity_reclaim_attempt() noexcept {
    if (!physics_world_->select_gravity_reclaim_retry()) return false;
    return restart_checkpoint();
}

bool Simulation::restart_checkpoint() noexcept {
    if (!physics_world_->restart_checkpoint()) return false;
    move_input_x_ = move_input_z_ = 0.0;
    jump_requested_ = jump_cancel_requested_ = traversal_requested_ = release_requested_ = false;
    parachute_toggle_requested_ = pick_up_requested_ = set_down_requested_ = rig_requested_ = false;
    crouch_input_ = sprint_input_ = false;
    jib_slew_input_ = jib_hoist_input_ = needle_hoist_input_ = service_lift_input_ = supplied_machine_input_ = 0.0;
    intake_slew_input_ = intake_hoist_input_ = 0.0;
    valve_toggle_requested_ = water_screw_toggle_requested_ = water_lift_valve_toggle_requested_ = false;
    water_lift_release_requested_ = water_lift_reset_requested_ = false;
    intake_sling_release_requested_ = intake_sling_attach_requested_ = false;
    sling_draw_ = 0.0;
    sling_action_ = sling_drop_ = false;
    snapshot_ = physics_world_->state();
    snapshot_.tick_index = tick_index_;
    snapshot_.simulation_time_seconds = tick_index_ * kFixedStepSeconds;
    snapshot_.fixed_step_seconds = kFixedStepSeconds;
    capture_render_history();
    remainder_seconds_ = 0.0;
    return true;
}

std::uint32_t Simulation::entity_body_count(const std::uint64_t entity) const noexcept {
    return physics_world_->entity_body_count(entity);
}
std::uint32_t Simulation::moving_body_count() const noexcept {
    return physics_world_->moving_body_count();
}

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

void Simulation::set_boiler_feed_enabled(const bool enabled) noexcept {
    physics_world_->set_feed_enabled(enabled);
}

bool Simulation::request_release() noexcept {
    jump_cancel_requested_ = true;
    const bool holding = snapshot_.traversal_state == TraversalState::Hanging ||
                         snapshot_.traversal_state == TraversalState::Climbing ||
                         snapshot_.traversal_state == TraversalState::Lowering;
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
    jump_cancel_requested_ = true;
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

std::vector<Vector3> Simulation::cargo_net_vertices(const double alpha) const {
    std::vector<Vector3> out;
    const auto *net=physics_world_->cargo_net();
    if(net) for(const auto &v:net->render_vertices(static_cast<float>(alpha))) out.push_back(to_vector3(v));
    return out;
}
std::vector<std::uint32_t> Simulation::cargo_net_indices() const {
    const auto *net=physics_world_->cargo_net();
    return net ? net->indices() : std::vector<std::uint32_t>{};
}

std::vector<Vector3> Simulation::catch_net_vertices(const double alpha) const {
    std::vector<Vector3> out;
    const auto *net = physics_world_->catch_net();
    if (net) for (const auto &v : net->render_vertices(static_cast<float>(alpha))) out.push_back(to_vector3(v));
    return out;
}
std::vector<std::uint32_t> Simulation::catch_net_indices() const {
    const auto *net = physics_world_->catch_net();
    return net ? net->indices() : std::vector<std::uint32_t>{};
}
std::vector<Vector3> Simulation::render_catch_net_tether_vertices() const {
    const auto *net=physics_world_->catch_net();
    if(!net || !net->config().tether_enabled) return {};
    const auto &config=net->config();
    const auto material=net->render_world_point(config.tether_material,float(snapshot().interpolation_alpha));
    const auto frame=kit_body_index(kCatchNetFrameEntity);
    const auto position=render_kit_body_position(frame);
    const auto rotation=render_kit_body_rotation(frame);
    const JPH::Quat q(float(rotation.x),float(rotation.y),float(rotation.z),float(rotation.w));
    const auto anchor=q*config.tether_anchor_body_local;
    return {{material.GetX(),material.GetY(),material.GetZ()},
            {position.x+anchor.GetX(),position.y+anchor.GetY(),position.z+anchor.GetZ()}};
}
std::vector<Vector3> Simulation::render_catch_net_patch_vertices() const {
    const auto *net=physics_world_->catch_net();
    if(!net || net->reinforcement_indices().empty()) return {};
    const auto vertices=net->render_vertices(float(snapshot().interpolation_alpha));
    std::vector<Vector3> out;out.reserve(3);
    for(const auto index:net->reinforcement_indices()) {
        if(index>=vertices.size()) return {};
        const auto &v=vertices[index];out.push_back({v.GetX(),v.GetY(),v.GetZ()});
    }
    return out;
}
CatchNetSnapshot Simulation::catch_net_state() const noexcept { return physics_world_->catch_net_state(); }

std::uint32_t Simulation::kit_body_count() const noexcept {
    return physics_world_->kit().body_count();
}

std::uint64_t Simulation::kit_body_entity(const std::uint32_t body) const noexcept {
    return physics_world_->kit().body_entity(kit::BodyIndex{body});
}

std::uint64_t Simulation::kit_body_material_key(const std::uint32_t body) const noexcept {
    return physics_world_->kit().body_weld_material_key(kit::BodyIndex{body});
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

std::vector<Vector3> Simulation::kit_body_part_mesh(const std::uint32_t body,
                                                  const std::uint32_t part) const {
    const auto &vertices = physics_world_->kit().body_part_mesh(kit::BodyIndex{body}, part);
    std::vector<Vector3> out;
    out.reserve(vertices.size());
    for (const auto &vertex : vertices) out.push_back(to_sim_vector(vertex));
    return out;
}

double Simulation::pipe_bridge_tip_height() const noexcept {
    const auto *bridge = physics_world_->pipe_bridge();
    return bridge ? bridge->tip_height() : 0;
}

std::uint32_t Simulation::pipe_bridge_retained_pipes() const noexcept {
    const auto *bridge = physics_world_->pipe_bridge();
    return bridge ? bridge->retained_pipes() : 0;
}

double Simulation::pipe_bridge_crush_front() const noexcept {
    const auto *bridge = physics_world_->pipe_bridge();
    return bridge ? bridge->state().plastic_front_m : 0;
}

Vector3 Simulation::kit_body_position(const std::uint32_t body) const noexcept {
    return to_sim_vector(physics_world_->kit().body_position(kit::BodyIndex{body}));
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

double Simulation::kit_body_kinetic_energy(const std::uint32_t body) const noexcept {
    return physics_world_->kit().body_kinetic_energy(kit::BodyIndex{body});
}

bool Simulation::inspection_span_seat_contact() const noexcept {
    return physics_world_->inspection_span_seat_contact();
}

Vector3 Simulation::kit_carry_grip_position(const std::uint32_t body) const noexcept {
    const auto &kit = physics_world_->kit();
    const kit::BodyIndex index{body};
    const auto local = kit.carry_handle(kit.body_entity(index));
    return to_sim_vector(kit.body_center_of_mass_position(index) + kit.body_rotation(index) * local);
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

bool Simulation::set_jib_slew_input(const double value) noexcept {
    if (!std::isfinite(value)) {
        return false;
    }
    jib_slew_input_ = std::clamp(value, -1.0, 1.0);
    return true;
}

bool Simulation::set_jib_hoist_input(const double value) noexcept {
    if (!std::isfinite(value)) {
        return false;
    }
    jib_hoist_input_ = std::clamp(value, -1.0, 1.0);
    return true;
}

bool Simulation::set_needle_hoist_input(const double value) noexcept {
    if (!std::isfinite(value)) {
        return false;
    }
    needle_hoist_input_ = std::clamp(value, -1.0, 1.0);
    return true;
}

bool Simulation::set_supplied_machine_input(const double value) noexcept {
    if (!std::isfinite(value)) return false;
    supplied_machine_input_ = std::clamp(value, -1.0, 1.0);
    return true;
}

bool Simulation::set_service_lift_input(const double value) noexcept {
    if (!std::isfinite(value)) return false;
    service_lift_input_ = std::clamp(value, -1.0, 1.0);
    return true;
}

bool Simulation::request_valve_toggle() noexcept {
    valve_toggle_requested_ = true;
    return true;
}

bool Simulation::request_water_screw_toggle() noexcept {
    water_screw_toggle_requested_ = true;
    return true;
}
bool Simulation::request_water_lift_valve_toggle() noexcept {
    water_lift_valve_toggle_requested_ = true;
    return true;
}
bool Simulation::request_water_lift_release() noexcept {
    water_lift_release_requested_ = true;
    return true;
}
bool Simulation::request_water_lift_reset() noexcept {
    water_lift_reset_requested_ = true;
    return true;
}
void Simulation::set_water_lift_rope_connected(const bool connected) noexcept {
    physics_world_->set_water_lift_rope_connected(connected);
}
void Simulation::set_water_lift_cage_mass_kg(const double mass_kg) noexcept {
    physics_world_->set_water_lift_cage_mass_kg(mass_kg);
}
void Simulation::set_water_screw_motor_torque_limit_nm(const double torque_nm) noexcept {
    physics_world_->set_water_screw_motor_torque_limit_nm(torque_nm);
}
void Simulation::set_water_screw_outlet_blocked(const bool blocked) noexcept {
    physics_world_->set_water_screw_outlet_blocked(blocked);
}
void Simulation::set_water_screw_drive_direction(const int direction) noexcept {
    physics_world_->set_water_screw_drive_direction(direction);
}
void Simulation::set_water_screw_basin_volume_m3(const double volume_m3) noexcept {
    physics_world_->set_water_screw_basin_volume_m3(volume_m3);
}
void Simulation::set_water_screw_tank_volume_m3_for_proof(const double volume_m3) noexcept {
    physics_world_->set_water_screw_tank_volume_m3_for_proof(volume_m3);
}

bool Simulation::set_intake_slew_input(const double value) noexcept {
    if (!std::isfinite(value)) {
        return false;
    }
    intake_slew_input_ = std::clamp(value, -1.0, 1.0);
    return true;
}

bool Simulation::set_intake_hoist_input(const double value) noexcept {
    if (!std::isfinite(value)) {
        return false;
    }
    intake_hoist_input_ = std::clamp(value, -1.0, 1.0);
    return true;
}

bool Simulation::request_intake_sling_release() noexcept {
    intake_sling_release_requested_ = true;
    return true;
}

bool Simulation::request_intake_sling_attach() noexcept {
    intake_sling_attach_requested_ = true;
    return true;
}

bool Simulation::set_grapple_aim(double x, double y, double z) noexcept {
    const double norm = std::hypot(x, y, z);
    if (!std::isfinite(norm) || norm < 1.0e-6) return false;
    grapple_aim_ = {x / norm, y / norm, z / norm};
    return true;
}
bool Simulation::set_grapple_equipped(bool equipped) noexcept {
    grapple_equipped_input_ = equipped;
    grapple_equip_requested_ = true;
    return true;
}
bool Simulation::set_grapple_reel_input(double input) noexcept {
    if (!std::isfinite(input) || input < -1 || input > 1) return false;
    grapple_reel_input_ = input;
    return true;
}
bool Simulation::set_grapple_auto_reel(bool enabled) noexcept {
    grapple_auto_reel_input_=enabled;
    grapple_auto_reel_requested_=true;
    return true;
}
bool Simulation::request_grapple_action() noexcept {
    grapple_action_requested_ = true;
    return true;
}
bool Simulation::request_grapple_release() noexcept {
    grapple_release_requested_ = true;
    return true;
}
GrappleGunSnapshot Simulation::grapple_gun_state() const noexcept {
    auto out = physics_world_->grapple_gun_state();
    const JPH::Vec3 aim = JPH::Vec3(float(grapple_aim_.x),float(grapple_aim_.y),float(grapple_aim_.z)).Normalized();
    out.launch_direction = to_vector3(JPH::RVec3(aim));
    out.launch_origin = to_vector3(GrapplingGun::launch_origin(
        JPH::RVec3(float(out.player_endpoint.x),float(out.player_endpoint.y),float(out.player_endpoint.z)),aim));
    return out;
}

void Simulation::step_fixed() noexcept {
    const std::uint64_t previous_death_count = snapshot_.death_count;
    const bool previous_crouched = snapshot_.player_crouched;
    capture_render_history();
    const double next_time_seconds =
        static_cast<double>(tick_index_ + 1) * kFixedStepSeconds;

    PhysicsWorld::StepCommands commands;
    commands.move_input_x = move_input_x_;
    commands.move_input_z = move_input_z_;
    commands.facing_x = facing_x_;
    commands.facing_z = facing_z_;
    commands.jump_requested = jump_requested_;
    commands.cancel_jump_requested = jump_cancel_requested_;
    commands.traversal_requested = traversal_requested_;
    commands.release_requested = release_requested_;
    commands.crouch_held = crouch_input_;
    commands.sprint_held = sprint_input_;
    commands.pick_up_requested = pick_up_requested_;
    commands.set_down_requested = set_down_requested_;
    commands.rig_requested = rig_requested_;
    commands.parachute_toggle_requested = parachute_toggle_requested_;
    commands.jib_slew_input = jib_slew_input_;
    commands.jib_hoist_input = jib_hoist_input_;
    commands.needle_hoist_input = needle_hoist_input_;
    commands.service_lift_input = service_lift_input_;
    commands.supplied_machine_input = supplied_machine_input_;
    commands.valve_toggle_requested = valve_toggle_requested_;
    commands.water_screw_toggle_requested = water_screw_toggle_requested_;
    commands.water_lift_valve_toggle_requested = water_lift_valve_toggle_requested_;
    commands.water_lift_release_requested = water_lift_release_requested_;
    commands.water_lift_reset_requested = water_lift_reset_requested_;
    commands.intake_slew_input = intake_slew_input_;
    commands.intake_hoist_input = intake_hoist_input_;
    commands.intake_sling_release_requested = intake_sling_release_requested_;
    commands.intake_sling_attach_requested = intake_sling_attach_requested_;

    commands.sling_draw = sling_draw_;
    commands.sling_yaw = sling_yaw_;
    commands.sling_elevation = sling_elevation_;
    commands.sling_action = sling_action_;
    commands.sling_drop = sling_drop_;
    commands.grapple_aim = grapple_aim_;
    commands.grapple_reel = grapple_reel_input_;
    commands.grapple_equip_requested = grapple_equip_requested_;
    commands.grapple_equipped = grapple_equipped_input_;
    commands.grapple_action = grapple_action_requested_;
    commands.grapple_release = grapple_release_requested_;
    commands.grapple_auto_reel = grapple_auto_reel_input_;
    commands.grapple_auto_reel_requested = grapple_auto_reel_requested_;
    physics_world_->step(commands, static_cast<float>(kFixedStepSeconds), next_time_seconds);
    sling_action_ = sling_drop_ = false;
    grapple_equip_requested_ = grapple_action_requested_ = grapple_release_requested_ = false;
    grapple_auto_reel_requested_ = false;
    jump_requested_ = jump_cancel_requested_ = false;
    traversal_requested_ = false;
    release_requested_ = false;
    parachute_toggle_requested_ = false;
    pick_up_requested_ = false;
    set_down_requested_ = false;
    rig_requested_ = false;
    valve_toggle_requested_ = false;
    water_screw_toggle_requested_ = false;
    water_lift_valve_toggle_requested_ = false;
    water_lift_release_requested_ = false;
    water_lift_reset_requested_ = false;
    intake_sling_release_requested_ = false;
    intake_sling_attach_requested_ = false;
    ++tick_index_;

    snapshot_ = physics_world_->state();
    if (snapshot_.death_count != previous_death_count ||
        snapshot_.player_crouched != previous_crouched) {
        capture_render_history();
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

Vector3 Simulation::render_traversal_hand(const bool left) const noexcept {
    const auto current = left ? snapshot_.traversal_left_hand : snapshot_.traversal_right_hand;
    const double alpha = std::clamp(remainder_seconds_ / kFixedStepSeconds, 0.0, 1.0);
    const auto &previous_anchor = previous_hand_anchors_[left ? 0 : 1];
    const auto anchor = physics_world_->render_hand_anchor(left ? 0 : 1);
    if (anchor.valid) {
        const bool retained = previous_anchor.valid && anchor.support == previous_anchor.support &&
            anchor.generation == previous_anchor.generation && anchor.deforming == previous_anchor.deforming &&
            snapshot_.traversal_state == previous_traversal_state_;
        // Grip transfers snap the local coordinate, not the support's clock.
        // A planted hand owns its support independently of the body's current
        // principal traversal support.
        auto local = anchor.local;
        if (retained) {
            local = {previous_anchor.local.x + (local.x - previous_anchor.local.x) * alpha,
                     previous_anchor.local.y + (local.y - previous_anchor.local.y) * alpha,
                     previous_anchor.local.z + (local.z - previous_anchor.local.z) * alpha};
        }
        if (anchor.deforming) {
            const auto *net = physics_world_->cargo_net();
            return to_sim_vector(net->render_world_point(
                JPH::Vec3(float(local.x), float(local.y), float(local.z)), float(alpha)));
        }
        if (anchor.kit_body < previous_kit_positions_.size()) {
            const auto position = render_kit_body_position(anchor.kit_body);
            const auto rotation = render_kit_body_rotation(anchor.kit_body);
            const auto offset = JPH::Quat(float(rotation.x), float(rotation.y), float(rotation.z), float(rotation.w)) *
                JPH::Vec3(float(local.x), float(local.y), float(local.z));
            return {position.x + offset.GetX(), position.y + offset.GetY(), position.z + offset.GetZ()};
        }
        // Non-Kit fixture supports use the captured hand owner's pose when
        // available; new owner acquisition has no earlier pose in that history.
        if (!previous_anchor.valid || anchor.support != previous_anchor.support) return current;
        const JPH::Quat a(float(previous_anchor.rotation.x), float(previous_anchor.rotation.y),
                          float(previous_anchor.rotation.z), float(previous_anchor.rotation.w));
        const JPH::Quat b(float(anchor.rotation.x), float(anchor.rotation.y),
                          float(anchor.rotation.z), float(anchor.rotation.w));
        const auto offset = a.SLERP(b, float(alpha)) * JPH::Vec3(float(local.x), float(local.y), float(local.z));
        return {previous_anchor.origin.x + (anchor.origin.x - previous_anchor.origin.x) * alpha + offset.GetX(),
                previous_anchor.origin.y + (anchor.origin.y - previous_anchor.origin.y) * alpha + offset.GetY(),
                previous_anchor.origin.z + (anchor.origin.z - previous_anchor.origin.z) * alpha + offset.GetZ()};
    }
    // Release and restore must not sweep a hand in from an old hold or from
    // the world origin. The arm presentation owns action-pose blending.
    if (snapshot_.traversal_state != previous_traversal_state_ ||
        snapshot_.traversal_support_entity_id != previous_traversal_support_)
        return current;
    const auto previous = left ? previous_left_hand_ : previous_right_hand_;
    return {previous.x + (current.x - previous.x) * alpha,
            previous.y + (current.y - previous.y) * alpha,
            previous.z + (current.z - previous.z) * alpha};
}

void Simulation::capture_render_history() noexcept {
    previous_player_position_ = snapshot_.player_position;
    previous_left_hand_ = snapshot_.traversal_left_hand;
    previous_right_hand_ = snapshot_.traversal_right_hand;
    previous_hand_anchors_[0] = physics_world_->render_hand_anchor(0);
    previous_hand_anchors_[1] = physics_world_->render_hand_anchor(1);
    previous_traversal_state_ = snapshot_.traversal_state;
    previous_traversal_support_ = snapshot_.traversal_support_entity_id;
    const auto count = kit_body_count();
    previous_kit_positions_.resize(count);
    previous_kit_rotations_.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        previous_kit_positions_[i] = kit_body_position(i);
        previous_kit_rotations_[i] = kit_body_rotation(i);
    }
    previous_slingshot_state_ = slingshot_state();
    previous_grapple_gun_state_ = grapple_gun_state();
    previous_cable_points_.resize(kit_cable_count());
    for (std::uint32_t cable = 0; cable < previous_cable_points_.size(); ++cable) {
        auto &points = previous_cable_points_[cable];
        points.resize(64);
        points.resize(kit_cable_points(cable, points.data(), 64));
    }
}

std::uint32_t Simulation::render_kit_cable_points(const std::uint32_t cable, Vector3 *out,
                                                   const std::uint32_t capacity) const noexcept {
    const auto count = kit_cable_points(cable, out, capacity);
    // Rehooking or parting changes topology. Interpolating across that event
    // could draw a rope that does not exist, so display the actual new shape.
    if (cable >= previous_cable_points_.size() || previous_cable_points_[cable].size() != count)
        return count;
    const double alpha = std::clamp(remainder_seconds_ / kFixedStepSeconds, 0.0, 1.0);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto previous = previous_cable_points_[cable][i];
        out[i] = {previous.x + (out[i].x - previous.x) * alpha,
                  previous.y + (out[i].y - previous.y) * alpha,
                  previous.z + (out[i].z - previous.z) * alpha};
    }
    return count;
}

Vector3 Simulation::render_kit_body_position(const std::uint32_t body) const noexcept {
    const auto current = kit_body_position(body);
    if (body >= previous_kit_positions_.size()) return current;
    const auto previous = previous_kit_positions_[body];
    const double alpha = std::clamp(remainder_seconds_ / kFixedStepSeconds, 0.0, 1.0);
    return {previous.x + (current.x - previous.x) * alpha,
            previous.y + (current.y - previous.y) * alpha,
            previous.z + (current.z - previous.z) * alpha};
}

Quaternion Simulation::render_kit_body_rotation(const std::uint32_t body) const noexcept {
    const auto current = kit_body_rotation(body);
    if (body >= previous_kit_rotations_.size()) return current;
    const auto previous = previous_kit_rotations_[body];
    const JPH::Quat a(float(previous.x), float(previous.y), float(previous.z), float(previous.w));
    const JPH::Quat b(float(current.x), float(current.y), float(current.z), float(current.w));
    return to_quaternion(a.SLERP(b, float(std::clamp(remainder_seconds_ / kFixedStepSeconds, 0.0, 1.0))));
}

GrappleGunSnapshot Simulation::render_grapple_gun_state() const noexcept {
    auto out = grapple_gun_state();
    const float alpha = float(snapshot().interpolation_alpha);
    out.player_endpoint = render_player_position();
    out.launch_origin = to_vector3(GrapplingGun::launch_origin(
        JPH::RVec3(float(out.player_endpoint.x),float(out.player_endpoint.y),float(out.player_endpoint.z)),
        JPH::Vec3(float(out.launch_direction.x),float(out.launch_direction.y),float(out.launch_direction.z))));
    for (unsigned i = 0; i < out.hooks.size(); ++i) {
        auto &hook = out.hooks[i];
        const auto &previous = previous_grapple_gun_state_.hooks[i];
        if (!hook.present) continue;
        if (previous.present && previous.body == hook.body) {
            hook.position = {previous.position.x + (hook.position.x - previous.position.x) * alpha,
                previous.position.y + (hook.position.y - previous.position.y) * alpha,
                previous.position.z + (hook.position.z - previous.position.z) * alpha};
            const JPH::Quat a(float(previous.rotation.x), float(previous.rotation.y),
                float(previous.rotation.z), float(previous.rotation.w));
            const JPH::Quat b(float(hook.rotation.x), float(hook.rotation.y),
                float(hook.rotation.z), float(hook.rotation.w));
            const auto rotation = a.SLERP(b, alpha);
            hook.rotation = {rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW()};
        }
        if (out.line_attached && hook.body == out.active_hook) {
            const auto eye = JPH::Quat(float(hook.rotation.x), float(hook.rotation.y),
                float(hook.rotation.z), float(hook.rotation.w)) * GrapplingGun::hook_eye_local();
            out.hook_endpoint = {hook.position.x + eye.GetX(), hook.position.y + eye.GetY(),
                hook.position.z + eye.GetZ()};
        }
    }
    return out;
}

SlingshotSnapshot Simulation::render_slingshot_state() const noexcept {
    auto result = slingshot_state();
    if (!result.available || !previous_slingshot_state_.available) return result;
    const auto blend = [&](Vector3 previous, Vector3 current) {
        const double alpha = std::clamp(remainder_seconds_ / kFixedStepSeconds, 0.0, 1.0);
        return Vector3{previous.x + (current.x - previous.x) * alpha,
                       previous.y + (current.y - previous.y) * alpha,
                       previous.z + (current.z - previous.z) * alpha};
    };
    result.pouch_position = blend(previous_slingshot_state_.pouch_position, result.pouch_position);
    const double alpha = std::clamp(remainder_seconds_ / kFixedStepSeconds, 0.0, 1.0);
    result.leather_deflection_m = previous_slingshot_state_.leather_deflection_m +
        (result.leather_deflection_m - previous_slingshot_state_.leather_deflection_m) * alpha;
    result.simulation_time_seconds = previous_slingshot_state_.simulation_time_seconds +
        (result.simulation_time_seconds - previous_slingshot_state_.simulation_time_seconds) * alpha;
    result.rider_specific_acceleration = blend(previous_slingshot_state_.rider_specific_acceleration,
                                               result.rider_specific_acceleration);
    // Evaluate the same deflected surface on the rendered pouch. Independent
    // target interpolation could separate the pelvis from its leather fold.
    const auto seat = Slingshot::leather_surface(float(result.harness_rest_local.x/1.68 + .5),
        float(result.harness_rest_local.z/1.30 + .5), float(result.leather_deflection_m));
    result.seat_surface_position = {result.pouch_position.x + seat.GetX(),
                                    result.pouch_position.y + seat.GetY(),
                                    result.pouch_position.z + seat.GetZ()};
    // The aiming lens must use the same interpolated rail as its mesh.
    // Raw 90 Hz angles otherwise step even when body translation is smooth.
    const auto rail_rotation = render_kit_body_rotation(kit_body_index(Slingshot::kLaunchRailEntity));
    const JPH::Quat rail_q(float(rail_rotation.x), float(rail_rotation.y),
                          float(rail_rotation.z), float(rail_rotation.w));
    const auto rail_axis = rail_q * JPH::Vec3(0, 0, -1);
    result.yaw_rad = std::atan2(rail_axis.GetX(), -rail_axis.GetZ());
    result.elevation_rad = std::asin(std::clamp(double(rail_axis.GetY()), -1.0, 1.0));
    // Read the interpolated fork transform, so bands stay attached to its
    // rendered timber tips while authority continues to use raw body poses.
    const auto frame = kit_body_index(Slingshot::kFrameEntity);
    const auto position = render_kit_body_position(frame);
    const auto rotation = render_kit_body_rotation(frame);
    const JPH::Quat q(float(rotation.x), float(rotation.y), float(rotation.z), float(rotation.w));
    const auto left = q * JPH::Vec3(float(-Slingshot::kHalfSpanM), 0.0F, float(-Slingshot::kForkRadiusM));
    const auto right = q * JPH::Vec3(float(Slingshot::kHalfSpanM), 0.0F, float(-Slingshot::kForkRadiusM));
    result.anchor_left = {position.x + left.GetX(), position.y + left.GetY(), position.z + left.GetZ()};
    result.anchor_right = {position.x + right.GetX(), position.y + right.GetY(), position.z + right.GetZ()};
    return result;
}

} // namespace scraperx::sim
