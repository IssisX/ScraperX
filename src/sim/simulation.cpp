// Simulation, the native's public face (simulation.hpp): each call forwards
// to the physics world (physics_world.hpp) and reads its snapshot back.

#include "sim/simulation.hpp"

#include "sim/loose_stock.hpp"
#include "sim/physics_world.hpp"

namespace scraperx::sim {

namespace {

// "SXCLIMB3" and the sizes of every saved type: a build whose types differ
// writes a different key (3: a kit body's state says whether it sleeps).
std::uint64_t save_format_key() noexcept {
    std::uint64_t key = 0x5358434C494D4233ULL;
    const std::size_t sizes[] = {sizeof(kit::Kit::BodyState), sizeof(kit::AnchorIndex), sizeof(kit::Kit::Pile),
                                 sizeof(Slingshot::State), sizeof(Swing::State),
                                 sizeof(vertical::Command), sizeof(JPH::RVec3)};
    for (const std::size_t size : sizes) {
        key = key * 1099511628211ULL ^ static_cast<std::uint64_t>(size);
    }
    return key;
}

} // namespace

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

bool Simulation::set_look_pitch(const double radians) noexcept {
    if (!std::isfinite(radians)) {
        return false;
    }
    look_pitch_input_ = std::clamp(radians, -1.5, 1.5);
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
                         snapshot_.traversal_state == TraversalState::Climbing ||
                         snapshot_.traversal_state == TraversalState::Swinging;
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
    return {to_sim_vector(source.half), to_sim_vector(source.offset), to_sim_quaternion(source.rotation),
            static_cast<std::uint8_t>(source.material), static_cast<std::uint8_t>(source.shape)};
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

static_assert(Simulation::kLooseStockFirstEntity == LooseStock::kFirstEntity);

std::uint32_t Simulation::loose_stock_count() const noexcept {
    return static_cast<std::uint32_t>(physics_world_->loose_stock().placed().size());
}

Simulation::LooseStockPiece Simulation::loose_stock_piece(const std::uint32_t index) const noexcept {
    const std::vector<LooseStock::Placement> &placed = physics_world_->loose_stock().placed();
    if (index >= placed.size()) {
        return {};
    }
    return {static_cast<std::uint8_t>(placed[index].kind), placed[index].variant};
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

ServiceState Simulation::service_state() const noexcept {
    const kit::Kit &kit = physics_world_->kit();
    const scraperx::sim::bands::MidstackService &service = physics_world_->service();
    ServiceState out;
    out.m_rope_on_eye = kit.rope_end_entity(service.m_rope) == Simulation::kServiceMCageEntityId;
    out.m_reel_latched = kit.catch_latched(service.m_reel_catch);
    out.m_cage_travel = kit.guide_travel(service.m_cage_guide);
    out.m_reel_travel = kit.guide_travel(service.m_reel_guide);
    out.m_reel_kg = kit.body_mass(service.m_reel);
    out.m_cable_paid = kit.reel_paid(service.m_reel_cable);
    out.m_rope_tension = kit.rope_tension(service.m_rope);
    out.n_cage_travel = kit.guide_travel(service.n_cage_guide);
    out.n_cage_peak_speed = kit.guide_peak_speed(service.n_cage_guide);
    out.n_hopper_travel = kit.guide_travel(service.n_hopper_guide);
    out.n_gate_angle = kit.lever_angle(service.n_gate_lever);
    out.n_silo_kg = kit.bin_contents(service.n_silo_bin);
    out.n_hopper_kg = kit.bin_contents(service.n_hopper_bin);
    out.n_chain_kg = kit.chain_hanging_kg(service.n_chain);
    out.n_hopper_mass = kit.body_mass(service.n_hopper);
    out.n_rope_tension = kit.rope_tension(service.n_rope);
    out.o_ram_latched = kit.catch_latched(service.o_ram_catch);
    out.o_gate_latched = kit.catch_latched(service.o_gate_catch);
    out.o_cab_travel = kit.guide_travel(service.o_cab_guide);
    out.o_cab_peak_speed = kit.guide_peak_speed(service.o_cab_guide);
    out.o_bucket_travel = kit.guide_travel(service.o_bucket_guide);
    out.o_wheel_angle = kit.wheel_angle(service.o_wheel_turn);
    out.o_ram_angle = kit.lever_angle(service.o_ram_lever);
    out.o_latch_angle = kit.lever_angle(service.o_latch_lever);
    out.o_gate_angle = kit.lever_angle(service.o_gate_lever);
    out.o_bin_kg = kit.bin_contents(service.o_bin_bin);
    out.o_bucket_kg = kit.bin_contents(service.o_bucket_bin);
    out.o_chain_kg = kit.chain_hanging_kg(service.o_chain);
    out.o_bucket_mass = kit.body_mass(service.o_bucket);
    out.o_rope_tension = kit.rope_tension(service.o_rope);
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
    out.s1_valve_pawled = kit.latch_set(stack.s1_pawl);
    out.s1_plate_travel = kit.guide_travel(stack.s1_plate_guide);
    out.s1_rope_tension = kit.rope_tension(stack.s1_rope);
    out.s2_cage_travel = kit.guide_travel(stack.s2_cage_guide);
    out.s2_cage_peak_speed = kit.guide_peak_speed(stack.s2_cage_guide);
    out.s2_cart_travel = kit.guide_travel(stack.s2_cart_guide);
    out.s2_beam_angle = kit.lever_angle(stack.s2_beam_hinge);
    out.s2_bolt_travel = kit.guide_travel(stack.s2_bolt_guide);
    out.s2_bolt_shot = kit.catch_latched(stack.s2_catch);
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

void Simulation::step_fixed() noexcept {
    const std::uint64_t previous_death_count = snapshot_.death_count;
    const bool previous_crouched = snapshot_.player_crouched;
    const std::uint64_t previous_step_up_count = snapshot_.step_up_count;
    previous_player_position_ = snapshot_.player_position;
    const double next_time_seconds =
        static_cast<double>(tick_index_ + 1) * kFixedStepSeconds;

    PhysicsWorld::StepCommands commands;
    commands.time_seconds = next_time_seconds;
    commands.move_input_x = move_input_x_;
    commands.move_input_z = move_input_z_;
    commands.facing_x = facing_x_;
    commands.look_pitch = look_pitch_input_;
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
    commands.sling_action = sling_action_;
    commands.sling_drop = sling_drop_;
    commands.swing_action = swing_action_;
    commands.swing_drop = swing_drop_;
    commands.trapeze_action = trapeze_action_;
    commands.trapeze_drop = trapeze_drop_;
    commands.lift_action = lift_action_;
    commands.lift_haul = lift_haul_;
    commands.forklift_action = forklift_action_;
    commands.forklift_lift = forklift_lift_;
    commands.hook_aim_x = hook_aim_x_;
    commands.hook_aim_y = hook_aim_y_;
    commands.hook_aim_z = hook_aim_z_;
    commands.hook_fire = hook_fire_;
    commands.hook_reel = hook_reel_;
    commands.hook_release = hook_release_;
    commands.hook_take = hook_take_;

    physics_world_->step(commands, static_cast<float>(kFixedStepSeconds), next_time_seconds);
    sling_action_ = false;
    sling_drop_ = false;
    swing_action_ = false;
    swing_drop_ = false;
    trapeze_action_ = false;
    trapeze_drop_ = false;
    lift_action_ = false;
    forklift_action_ = false;
    hook_fire_ = false;
    hook_release_ = false;
    hook_take_ = false;
    jump_requested_ = false;
    traversal_requested_ = false;
    release_requested_ = false;
    parachute_toggle_requested_ = false;
    pick_up_requested_ = false;
    set_down_requested_ = false;
    rig_requested_ = false;
    ++tick_index_;

    snapshot_ = physics_world_->state();
    const double render_dx = snapshot_.player_position.x - previous_player_position_.x;
    const double render_dy = snapshot_.player_position.y - previous_player_position_.y;
    const double render_dz = snapshot_.player_position.z - previous_player_position_.z;
    // A crouch or a step lifts the body in one tick: the render pose takes it
    // whole, and the presentation eases the eye over it from the snapshot.
    if (snapshot_.death_count != previous_death_count ||
        snapshot_.player_crouched != previous_crouched ||
        snapshot_.step_up_count != previous_step_up_count ||
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

bool Simulation::set_slingshot_input(const double draw, const double yaw, const double elevation) noexcept {
    if (!std::isfinite(draw) || !std::isfinite(yaw) || !std::isfinite(elevation)) {
        return false;
    }
    sling_draw_ = std::clamp(draw, 0.0, 1.0);
    sling_yaw_ = std::clamp(yaw, -0.8, 0.8);
    sling_elevation_ = std::clamp(elevation, 0.35, 1.48);
    return true;
}

bool Simulation::set_hook_aim(const double x, const double y, const double z) noexcept {
    const double length = std::sqrt(x * x + y * y + z * z);
    if (!std::isfinite(length) || length < 1.0e-6) {
        return false;
    }
    hook_aim_x_ = x / length;
    hook_aim_y_ = y / length;
    hook_aim_z_ = z / length;
    return true;
}

bool Simulation::request_hook_fire() noexcept {
    if (!snapshot_.has_hook || snapshot_.hook_state != 0) {
        return false;
    }
    hook_fire_ = true;
    return true;
}

bool Simulation::set_hook_reel(const bool held) noexcept {
    hook_reel_ = held;
    return true;
}

bool Simulation::request_hook_release() noexcept {
    if (snapshot_.hook_state == 0) {
        return false;
    }
    hook_release_ = true;
    return true;
}

bool Simulation::surface_below(const double x, const double y, const double z, const double reach,
                               double &hit_y) const noexcept {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(reach) ||
        reach > 2000.0) {
        return false;
    }
    JPH::RVec3 point;
    if (!physics_world_->surface_below(JPH::RVec3(x, y, z), static_cast<float>(reach), point)) {
        return false;
    }
    hit_y = static_cast<double>(point.GetY());
    return true;
}

bool Simulation::request_hook_take() noexcept {
    if (snapshot_.has_hook) {
        return false;
    }
    hook_take_ = true;
    return true;
}

bool Simulation::request_slingshot_action() noexcept {
    sling_action_ = true;
    return true;
}

bool Simulation::request_slingshot_drop() noexcept {
    const auto &state = physics_world_->slingshot().state();
    if (!state.seated && !state.recovering) {
        return false;
    }
    sling_drop_ = true;
    return true;
}

SlingshotSnapshot Simulation::slingshot_state() const noexcept {
    const auto to_vector = [](const JPH::RVec3 p) {
        return Vector3{static_cast<double>(p.GetX()), static_cast<double>(p.GetY()),
                       static_cast<double>(p.GetZ())};
    };
    const auto &state = physics_world_->slingshot().state();
    SlingshotSnapshot result;
    result.station_available = state.station_available;
    result.seated = state.seated;
    result.drawing = state.drawing;
    result.released = state.released;
    result.release_ready = state.release_ready;
    result.can_retrieve = state.can_retrieve;
    result.recovering = state.recovering;
    result.guided_launch = state.guided_launch;
    result.aim_ready = state.aim_ready;
    result.aim_locked = state.aim_locked;
    result.flight = physics_world_->slingshot_flight();
    result.draw_m = state.draw_m;
    result.max_draw_m = Slingshot::kMaximumDrawM;
    result.energy_j = state.energy_j;
    result.work_j = state.work_j;
    result.yaw_rad = state.yaw_rad;
    result.elevation_rad = state.elevation_rad;
    result.target_yaw_rad = state.target_yaw_rad;
    result.target_elevation_rad = state.target_elevation_rad;
    result.band_rest_m = state.band_rest_m;
    result.leather_deflection_m = state.leather_deflection_m;
    result.anchor_left = to_vector(state.anchor_left);
    result.anchor_right = to_vector(state.anchor_right);
    result.pouch_position = to_vector(state.pouch_position);
    result.neutral_position = to_vector(Slingshot::neutral_position());
    result.retrieval_control_position = to_vector(Slingshot::retrieval_control_position());
    result.launch_track_start = to_vector(state.launch_track_start);
    result.launch_track_end = to_vector(state.launch_track_end);
    result.launch_count = state.launch_count;
    return result;
}

std::vector<Vector3> Simulation::slingshot_prediction() const {
    return physics_world_->slingshot_prediction();
}

bool Simulation::request_swing_action() noexcept {
    swing_action_ = true;
    return true;
}

bool Simulation::request_swing_drop() noexcept {
    const auto &state = physics_world_->swing().state();
    if (!state.seated || !state.may_leave) {
        return false;
    }
    swing_drop_ = true;
    return true;
}

bool Simulation::request_trapeze_action() noexcept {
    const auto &state = physics_world_->trapeze().state();
    if (!state.harnessed && !state.catch_available && !state.catch_near) {
        return false;
    }
    trapeze_action_ = true;
    return true;
}

bool Simulation::request_trapeze_drop() noexcept {
    if (!physics_world_->trapeze().state().harnessed) {
        return false;
    }
    trapeze_drop_ = true;
    return true;
}

bool Simulation::request_forklift_action() noexcept {
    if (!physics_world_->forklift().driving() && !physics_world_->at_forklift_controls()) {
        return false;
    }
    forklift_action_ = true;
    return true;
}

bool Simulation::set_forklift_lift(const double lift) noexcept {
    if (!std::isfinite(lift)) {
        return false;
    }
    forklift_lift_ = std::clamp(lift, -1.0, 1.0);
    return true;
}

ForkliftSnapshot Simulation::forklift_state() const noexcept {
    const Forklift &forklift = physics_world_->forklift();
    const Forklift::State &state = forklift.state();
    ForkliftSnapshot result;
    result.driving = state.driving;
    result.at_controls = !state.driving && physics_world_->at_forklift_controls();
    result.lift_m = state.lift_m;
    result.speed_mps = state.speed_mps;
    result.steer = state.steer;
    result.drive = state.drive;
    result.lift = state.lift;
    result.lift_n = state.lift_n;
    result.lift_max_n = Forklift::kLiftForceN;
    result.pitch_rad = state.pitch_rad;
    for (int wheel = 0; wheel < Forklift::kWheels; ++wheel) {
        result.wheel_load_n[static_cast<std::size_t>(wheel)] = state.wheel_load_n[static_cast<std::size_t>(wheel)];
        result.wheel_contact[static_cast<std::size_t>(wheel)] = state.wheel_contact[static_cast<std::size_t>(wheel)];
    }
    for (int wheel = 0; wheel < Forklift::kWheels; ++wheel) {
        const JPH::RMat44 at = forklift.wheel_transform(wheel);
        const auto index = static_cast<std::size_t>(wheel);
        result.wheel_position[index] = to_sim_vector(at.GetTranslation());
        result.wheel_rotation[index] = to_sim_quaternion(at.GetQuaternion());
        result.wheel_radius[index] = forklift.wheel_radius(wheel);
        result.wheel_width[index] = forklift.wheel_width(wheel);
    }
    return result;
}

std::uint32_t Simulation::zipline_count() const noexcept {
    return static_cast<std::uint32_t>(physics_world_->zipline().lines().size());
}

Simulation::ZiplineLine Simulation::zipline_line(const std::uint32_t index) const noexcept {
    const auto &lines = physics_world_->zipline().lines();
    if (index >= lines.size()) {
        return {};
    }
    const auto &line = lines[index];
    return {to_sim_vector(line.top), to_sim_vector(line.bottom), line.length, line.brake_m, line.trolley_m,
            line.block_m, line.parked};
}

Vector3 Simulation::zipline_bar() const noexcept {
    return to_sim_vector(physics_world_->zipline().bar());
}

TrapezeSnapshot Simulation::trapeze_state() const noexcept {
    const auto &state = physics_world_->trapeze().state();
    TrapezeSnapshot result;
    result.catch_available = state.catch_available;
    result.catch_near = state.catch_near;
    result.harnessed = state.harnessed;
    result.pulled = state.pulled;
    result.catch_speed_mps = state.catch_speed_mps;
    result.catch_loss_j = state.catch_loss_j;
    result.hand_force_n = state.hand_force_n;
    result.peak_hand_force_n = state.peak_hand_force_n;
    result.catches = state.catches;
    result.slips = state.slips;
    result.angle_rad = state.angle_rad;
    result.angular_speed = state.angular_speed;
    result.peak_angle_rad = state.peak_angle_rad;
    result.pulled_in_m = state.pulled_in_m;
    result.arm_work_j = state.arm_work_j;
    result.arm_work_out_j = state.arm_work_out_j;
    result.mechanical_j = state.mechanical_j;
    result.energy_residual_j = state.energy_residual_j;
    result.pumps = state.pumps;
    const JPH::RVec3 bar = physics_world_->trapeze().bar_position();
    result.bar = Vector3{static_cast<double>(bar.GetX()), static_cast<double>(bar.GetY()),
                         static_cast<double>(bar.GetZ())};
    return result;
}

MatronSnapshot Simulation::matron_state() const noexcept {
    const Matron &matron = physics_world_->matron();
    const auto &view = matron.view();
    MatronSnapshot result;
    result.state = static_cast<std::uint8_t>(view.state);
    result.alive = view.alive;
    result.released = view.released;
    result.sees_player = view.sees_player;
    result.position = Vector3{view.x, view.y, view.z};
    result.yaw = view.yaw;
    result.speed = view.speed;
    result.stride = view.stride;
    result.strain = view.strain;
    result.bolts_popped = view.bolts_popped;
    result.noise_heard = view.noise_heard;
    result.target_x = view.target_x;
    result.target_z = view.target_z;
    result.catwalk_allowed = view.catwalk_allowed;
    result.turn = static_cast<std::uint8_t>(view.turn);
    result.turn_s = view.turn_s;
    result.catches = matron.catches();
    result.planks_fallen = matron.planks_fallen();
    result.plank_load_peak_n = matron.strain_force_peak_n();
    const auto &planks = matron.planks();
    for (std::size_t i = 0; i < planks.size(); ++i) {
        const auto &p = planks[i];
        const std::size_t o = i * 9;
        result.planks[o + 0] = p.x;
        result.planks[o + 1] = p.y;
        result.planks[o + 2] = p.z;
        result.planks[o + 3] = p.qx;
        result.planks[o + 4] = p.qy;
        result.planks[o + 5] = p.qz;
        result.planks[o + 6] = p.qw;
        result.planks[o + 7] = p.bolted ? 1.0 : 0.0;
        result.planks[o + 8] = p.gone ? 1.0 : 0.0;
    }
    return result;
}

SummitSnapshot Simulation::summit_state() const noexcept {
    const auto &state = physics_world_->summit().state();
    SummitSnapshot result;
    result.lit = state.lit;
    result.striker_travel_m = state.striker_travel_m;
    result.striker_peak_m = state.striker_peak_m;
    result.trip_travel_m = Summit::kTripTravel;
    result.lamp_yaw_rad = state.lamp_yaw_rad;
    result.lamp_speed_rad_s = state.lamp_speed_rad_s;
    return result;
}

void Simulation::light_summit() noexcept { physics_world_->summit().light(); }

SwingSnapshot Simulation::swing_state() const noexcept {
    const auto to_vector = [](const JPH::RVec3 p) {
        return Vector3{static_cast<double>(p.GetX()), static_cast<double>(p.GetY()),
                       static_cast<double>(p.GetZ())};
    };
    const Swing &swing = physics_world_->swing();
    const auto &state = swing.state();
    SwingSnapshot result;
    result.station_available = state.station_available;
    result.seated = state.seated;
    result.may_leave = state.may_leave;
    result.ram_held = state.ram_held;
    result.tripped = state.tripped;
    result.held_at_top = state.tooth >= 0;
    result.tooth = state.tooth;
    result.seat_angle_rad = state.seat_angle_rad;
    result.ram_angle_rad = state.ram_angle_rad;
    result.seat_speed_mps = state.seat_speed_mps;
    result.ram_speed_mps = state.ram_speed_mps;
    result.seat_floor_y = state.seat_floor_y;
    result.apex_floor_y = state.apex_floor_y;
    result.kick_travel_m = state.kick_travel_m;
    result.buffer_compression_m = state.buffer_compression_m;
    result.buffer_force_n = state.buffer_force_n;
    result.peak_buffer_force_n = state.peak_buffer_force_n;
    result.peak_seat_accel_mps2 = state.peak_seat_accel_mps2;
    result.buffer_loss_j = state.buffer_loss_j;
    result.energy_residual_j = state.energy_residual_j;
    result.mechanical_j = state.mechanical_j;
    result.seat_pivot = to_vector(Swing::seat_pivot());
    result.ram_pivot = to_vector(Swing::ram_pivot());
    result.seat_pin = to_vector(swing.seat_pin());
    result.ram_pin = to_vector(swing.ram_pin());
    return result;
}

std::vector<std::uint8_t> Simulation::save_game() const {
    SaveWriter w;
    w.pod(save_format_key());
    w.pod(tick_index_);
    physics_world_->write_save(w);
    return std::move(w.out);
}

bool Simulation::load_game(const std::uint8_t *data, const std::size_t size) {
    if (data == nullptr) {
        return false;
    }
    SaveReader r(data, size);
    std::uint64_t key = 0, tick = 0;
    if (!r.pod(key) || key != save_format_key() || !r.pod(tick)) {
        return false;
    }
    if (!physics_world_->load_save(r)) {
        return false;
    }
    tick_index_ = tick;
    remainder_seconds_ = 0.0;
    snapshot_ = physics_world_->state();
    snapshot_.tick_index = tick_index_;
    snapshot_.simulation_time_seconds = static_cast<double>(tick_index_) * kFixedStepSeconds;
    snapshot_.fixed_step_seconds = kFixedStepSeconds;
    previous_player_position_ = snapshot_.player_position;
    return true;
}

bool Simulation::set_lift_haul(const bool held) noexcept {
    lift_haul_ = held;
    return true;
}

bool Simulation::request_lift_action() noexcept {
    if (lift_state().machine < 0) {
        return false;
    }
    lift_action_ = true;
    return true;
}

LiftSnapshot Simulation::lift_state() const noexcept {
    using scraperx::sim::vertical::Route;
    const Route &route = physics_world_->vertical();
    const Route::Here here = route.at(physics_world_->standing_entity());
    LiftSnapshot result;
    result.machine = here.machine;
    result.role = here.role == Route::Role::Deck    ? 1
                  : here.role == Route::Role::Entry ? 2
                  : here.role == Route::Role::Exit  ? 3
                                                    : 0;
    result.travel = here.travel;
    result.target = here.target;
    result.moving = here.moving;
    result.machine_count = static_cast<int>(route.size());
    for (std::size_t i = 0; i < route.size() && i < result.travels.size(); ++i) {
        result.travels[i] = route.travel(i);
    }
    result.incline_state = static_cast<int>(route.incline_state());
    result.incline_brake_n = route.incline_brake_n();
    result.incline_speed_mps = route.incline() >= 0 ? route.deck_speed(static_cast<std::size_t>(route.incline())) : 0.0;
    result.tram_departs_s = route.tram_departs_s();
    result.derrick_iron_aboard = route.derrick_iron_aboard();
    result.derrick_holding = route.derrick_holding();
    result.pitman = route.pitman();
    result.pitman_haul_n = route.pitman_haul_n();
    result.wheel_hopper_kg = route.wheel_hopper_kg();
    result.wheel_skip_kg = route.wheel_skip_kg();
    return result;
}

} // namespace scraperx::sim
