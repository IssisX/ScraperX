#include "sim/simulation/world.hpp"

namespace scraperx::sim::detail {

[[nodiscard]] JPH::RVec3 spawn_position(const scraperx::sim::InitialSpawn spawn) noexcept {
    switch (spawn) {
    case scraperx::sim::InitialSpawn::StaticDeck:
        return {0.0, 3.0, -8.0};
    case scraperx::sim::InitialSpawn::RotatingSupport:
        return {-6.5, 3.0, 0.0};
    case scraperx::sim::InitialSpawn::VaultApproach:
        return {1.2, 1.2, -6.0};
    case scraperx::sim::InitialSpawn::MantleApproach:
        return {7.9, 1.2, -6.0};
    case scraperx::sim::InitialSpawn::HangApproach:
        return {7.6, 4.2, 4.0};
    case scraperx::sim::InitialSpawn::MovingLedgeApproach:
        return {6.1, 4.2, 12.0};
    case scraperx::sim::InitialSpawn::BlockedLedgeApproach:
        return {-3.6, 1.2, -8.0};
    case scraperx::sim::InitialSpawn::HighDrop:
        // ~61 m above the static deck: unmitigated free fall reaches
        // sqrt(2*g*61) ~= 34.6 m/s, well past kLethalImpactSpeedMps.
        return {0.0, 62.0, -8.0};
    case scraperx::sim::InitialSpawn::SurvivableDrop:
        // ~12 m above the static deck: unmitigated free fall reaches
        // sqrt(2*g*12) ~= 15.3 m/s, under kLethalImpactSpeedMps with margin.
        return {0.0, 13.0, -8.0};
    case scraperx::sim::InitialSpawn::TeeterEntry:
        return {25.3, 66.9, -139.0};
    case scraperx::sim::InitialSpawn::TeeterFarDrop:
        return {32.4, 70.0, -139.0};
    case scraperx::sim::InitialSpawn::BracedBayEntry:
        return {25.1, 77.9, -132.0};
    case scraperx::sim::InitialSpawn::NorthFrameEntry:
        return {16.0, 88.9, -174.5};
    case scraperx::sim::InitialSpawn::CatwalkTreadle:
        // Above the outboard half of the treadle plate, where a body has real
        // leverage on the hinge.
        return {15.3, 10.3, -106.0};
    case scraperx::sim::InitialSpawn::KernelJibStation:
        return {kJibStationX, 1.2, kJibStationZ};
    case scraperx::sim::InitialSpawn::KernelCrateTop:
        // Offset from the crate's centre so the player doesn't spawn inside
        // the hook, which hangs directly above the centre via the pin link.
        return {static_cast<double>(kJibMastX + kJibBoomLength) + 0.4, 2.4,
                static_cast<double>(kJibMastZ)};
    case scraperx::sim::InitialSpawn::KernelNeedleStation:
        // On the approach pier top, not at grade: see the station-siting
        // note by kNeedleStationX above.
        return {static_cast<double>(kNeedleStationX), static_cast<double>(kNeedlePierTopY) + 1.0,
                static_cast<double>(kNeedleStationZ)};
    case scraperx::sim::InitialSpawn::KernelSumpStation:
        // On the fixed approach decking, not the grate -- see the station-
        // siting note by kSumpStationX above.
        return {static_cast<double>(kSumpStationX), static_cast<double>(kSumpPlatformTopY) + 1.0,
                static_cast<double>(kSumpStationZ)};
    case scraperx::sim::InitialSpawn::IntakePendant:
        // Standing on the belt catwalk at CAP-PENDANT, inside the station
        // radius, so the freight sequence is driven without a climb.
        return {static_cast<double>(kIntakePendantX),
                static_cast<double>(kIntakePendantTopY) + 1.0,
                static_cast<double>(kIntakePendantZ)};
    case scraperx::sim::InitialSpawn::IntakeThroat:
        // On the apron 3 m outside the MOD-DOG-A throat, facing into the bay.
        return {static_cast<double>(kIntakeBayCenterX), 1.2,
                static_cast<double>(kIntakeBayFrontZ) + 3.0};
    case scraperx::sim::InitialSpawn::IntakeSkinFoot:
        // On the apron just south of MOD-SKIN-LADDER-S's lowest rung.
        return {static_cast<double>(kIntakeSkinCenterX), 1.2,
                static_cast<double>(kIntakeSkinFootZ + kIntakeSkinRungHalfZ) + 1.2};
    case scraperx::sim::InitialSpawn::IntakeHandoffDeck:
        // On the +24.1872 m handoff deck itself, near its east edge, facing
        // MOD-STAIR-A-SWING's stowed footprint.
        return {-3.0, static_cast<double>(kIntakeHandoffY) + 1.0872, -112.5};
    case scraperx::sim::InitialSpawn::Hook5Cage:
        // On the cage floor under the roof hatch, where the drop lands.
        return {10.30, 1.0, static_cast<double>(kHook5MinZ) + 2.2};
    case scraperx::sim::InitialSpawn::Hook5Apron:
        return {static_cast<double>(kHook5MaxX) + 1.5, 1.0, static_cast<double>(kHook5MinZ) - 3.0};
    case scraperx::sim::InitialSpawn::StairTop:
        // The 154 m deck's north band, north of Stage A's cage.
        return {-10.5, 155.0, -128.2};
    case scraperx::sim::InitialSpawn::WaterScrewStation:
        return {static_cast<double>(kWaterScrewStationX), 0.9,
                static_cast<double>(kWaterScrewStationZ) - 0.8};
    case scraperx::sim::InitialSpawn::WaterLiftValveStation:
        return {kWaterLiftValveStationX, kWaterLiftValveStationY, kWaterLiftValveStationZ + 0.8};
    case scraperx::sim::InitialSpawn::WaterLiftCage:
        return {-12.50, 1.15, -108.20};
    case scraperx::sim::InitialSpawn::WaterLiftUpperDock:
        return {-9.45, 9.15, -108.20};
    case scraperx::sim::InitialSpawn::MachineYard:
        return {31.2, 5.0, -96.0};
    case scraperx::sim::InitialSpawn::LiftPlatform:
        return {16.4, 2.0, -100.0};
    case scraperx::sim::InitialSpawn::ExteriorGrade:
        // At grade, outdoors, 120 m short of the tower face: far enough that the
        // mass reads as something you approach, close enough that its lower
        // third already fills the frame.
        return {6.0, 1.2, -25.0};
    case scraperx::sim::InitialSpawn::TranslatingSupport:
        return {0.0, 3.0, 8.0};
    default:
        return {6.0, 1.2, -25.0};
    }
}

void approach_relative_horizontal_velocity(JPH::Vec3 &world_velocity,
                                           const JPH::Vec3 reference_velocity,
                                           const double move_input_x,
                                           const double move_input_z,
                                           const float acceleration,
                                           const float delta_seconds,
                                           const float full_speed) noexcept {
    float relative_x = world_velocity.GetX() - reference_velocity.GetX();
    float relative_z = world_velocity.GetZ() - reference_velocity.GetZ();
    const float target_x = static_cast<float>(move_input_x) * full_speed;
    const float target_z = static_cast<float>(move_input_z) * full_speed;
    float delta_x = target_x - relative_x;
    float delta_z = target_z - relative_z;
    const float delta_length = std::sqrt(delta_x * delta_x + delta_z * delta_z);
    const float maximum_delta = acceleration * delta_seconds;

    if (delta_length > maximum_delta && delta_length > 0.0F) {
        const float scale = maximum_delta / delta_length;
        delta_x *= scale;
        delta_z *= scale;
    }

    relative_x += delta_x;
    relative_z += delta_z;
    world_velocity.SetX(reference_velocity.GetX() + relative_x);
    world_velocity.SetZ(reference_velocity.GetZ() + relative_z);
}

[[nodiscard]] float smoothstep(const float edge0, const float edge1, const float value) noexcept {
    if (edge1 <= edge0) {
        return value < edge0 ? 0.0F : 1.0F;
    }
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] float ease_to_rest(const float value, const float entry_slope) noexcept {
    const float t = std::clamp(value, 0.0F, 1.0F);
    const float slope = std::clamp(entry_slope, 0.0F, 3.0F);
    return slope * t * (1.0F - t) * (1.0F - t) + t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] float ease_with_carried_speed(const float value, const float slope) noexcept {
    const float t = std::clamp(value, 0.0F, 1.0F);
    const float s = std::clamp(slope, 0.0F, 3.0F);
    return t * t * (3.0F - 2.0F * t) + s * t * (1.0F - t) * (1.0F - 2.0F * t);
}

[[nodiscard]] BodyCheckpoint capture_body(const JPH::BodyInterface &bodies,
                                          const JPH::BodyID id) noexcept {
    return {bodies.GetPosition(id), bodies.GetRotation(id), bodies.GetLinearVelocity(id),
            bodies.GetAngularVelocity(id)};
}

void restore_body(JPH::BodyInterface &bodies, const JPH::BodyID id,
                  const BodyCheckpoint &checkpoint) noexcept {
    bodies.SetPositionAndRotation(id, checkpoint.position, checkpoint.rotation,
                                  JPH::EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(id, checkpoint.linear_velocity,
                                       checkpoint.angular_velocity);
}

[[nodiscard]] scraperx::sim::Vector3 to_vector3(const JPH::RVec3 value) noexcept {
    return {value.GetX(), value.GetY(), value.GetZ()};
}

[[nodiscard]] scraperx::sim::Quaternion to_quaternion(const JPH::Quat value) noexcept {
    return {value.GetX(), value.GetY(), value.GetZ(), value.GetW()};
}

[[nodiscard]] bool box_is_hold(const LeafBox &box, std::uint32_t &long_axis) noexcept {
    long_axis = 0;
    for (std::uint32_t axis = 1; axis < 3; ++axis) {
        if (box.half[axis] > box.half[long_axis]) {
            long_axis = axis;
        }
    }
    if (2.0F * box.half[long_axis] < kGripMinLength) {
        return false;
    }
    for (std::uint32_t axis = 0; axis < 3; ++axis) {
        if (axis != long_axis && 2.0F * box.half[axis] > kGripMaxSection) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool leaf_box(const JPH::Body &body, const JPH::SubShapeID &sub_shape,
                            LeafBox &out) noexcept {
    JPH::SubShapeID remainder;
    const JPH::TransformedShape leaf = body.GetShape()->GetSubShapeTransformedShape(
        sub_shape, body.GetCenterOfMassPosition(), body.GetRotation(), JPH::Vec3::sReplicate(1.0F),
        remainder);
    if (leaf.mShape == nullptr || leaf.mShape->GetSubType() != JPH::EShapeSubType::Box) {
        return false;
    }
    out.half = static_cast<const JPH::BoxShape *>(leaf.mShape.GetPtr())->GetHalfExtent();
    const JPH::RMat44 frame = leaf.GetCenterOfMassTransform();
    out.centre = frame.GetTranslation();
    for (std::uint32_t axis = 0; axis < 3; ++axis) {
        out.axes[axis] = frame.GetColumn3(axis).Normalized();
    }
    return true;
}

[[nodiscard]] bool entity_is_causal_section_support(std::uint64_t entity) noexcept {
    return entity == scraperx::sim::kCatchNetEntity || entity == scraperx::sim::kCatchNetFrameEntity || scraperx::sim::is_parkour_route_entity(entity) || scraperx::sim::SuppliedAscent::owns_support(entity) || scraperx::sim::ServiceLift::owns_support(entity) || entity==scraperx::sim::kCargoGantryEntity || entity==2954 ||
        scraperx::sim::is_facade_route_entity(entity) ||
        scraperx::sim::is_grapple_practice_route_entity(entity) ||
        scraperx::sim::is_deformable_plank_segment_entity(entity) ||
        entity == scraperx::sim::kDeformablePlankSeatEntity;
}

[[nodiscard]] bool entity_is_moving_support(const std::uint64_t entity_id) noexcept {
    using Sim = scraperx::sim::Simulation;
    // Mechanism-kit bodies that move are machines by construction.
    return scraperx::sim::kit::is_dynamic_entity(entity_id) ||
           scraperx::sim::is_facade_route_entity(entity_id) ||
           scraperx::sim::is_grapple_practice_route_entity(entity_id) ||
           entity_id == Sim::kTranslatingSupportEntityId ||
           entity_id == Sim::kRotatingSupportEntityId ||
           entity_id == Sim::kMovingLedgeEntityId ||
           entity_id == Sim::kHoistScoopEntityId ||
           entity_id == Sim::kTipperEntityId ||
           entity_id == Sim::kLiftPlatformEntityId ||
           entity_id == Sim::kCounterweightEntityId ||
           entity_id == Sim::kTreadleEntityId ||
           entity_id == Sim::kJibHookEntityId ||
           entity_id == Sim::kCrateEntityId ||
           entity_id == Sim::kNeedleBeamEntityId ||
           entity_id == Sim::kIntakeBeltEntityId ||
           entity_id == Sim::kIntakeJibHookEntityId ||
           entity_id == Sim::kIntakePackEntityId ||
           entity_id == Sim::kIntakeDogEntityId ||
           // AS-002: the deployed swing flight is a dynamic body under real
           // load, not scenery -- a player standing on it while it is still
           // settling against its stop must inherit its point velocity under
           // the WO-002 law, exactly like every other machine member above.
           entity_id == Sim::kIntakeSwingFlightEntityId;
}

} // namespace scraperx::sim::detail
