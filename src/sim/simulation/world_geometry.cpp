#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

namespace {
// The presentation's static dressing as native collision (see
// kWorldSolidEntityId). Boxes are oriented; hulls are world-space point sets.
struct WorldSolidBox final {
    float px, py, pz;
    float qx, qy, qz, qw;
    float hx, hy, hz;
};
struct WorldSolidHull final {
    std::uint32_t first;
    std::uint32_t count;
};
struct WorldSolidSoundTag final {
    std::uint32_t box_index;
    std::uint8_t surface;
};
#include "sim/world_solids.inc"
namespace regression_geometry {
#include "../../../tests/fixtures/world_solids.inc"
}

// A drawn box whose world bounds match an owned body this closely is that

} // namespace

void Simulation::PhysicsWorld::build_world_solids(JPH::BodyInterface &bodies) {
    const WorldSolidBox *boxes = regression_fixtures_ ? regression_geometry::kWorldSolidBoxes : kWorldSolidBoxes;
    const std::size_t box_count = regression_fixtures_ ? std::size(regression_geometry::kWorldSolidBoxes) : std::size(kWorldSolidBoxes);
    const WorldSolidHull *hulls = regression_fixtures_ ? regression_geometry::kWorldSolidHulls : kWorldSolidHulls;
    const std::size_t hull_count = regression_fixtures_ ? std::size(regression_geometry::kWorldSolidHulls) : std::size(kWorldSolidHulls);
    const float *hull_points = regression_fixtures_ ? regression_geometry::kWorldSolidHullPoints : kWorldSolidHullPoints;
    for (std::size_t i = 0; i < box_count; ++i) {
        const WorldSolidBox &box = boxes[i];
        const JPH::Vec3 half(box.hx, box.hy, box.hz);
        const JPH::RVec3 position(box.px, box.py, box.pz);
        const JPH::Quat rotation = JPH::Quat(box.qx, box.qy, box.qz, box.qw).Normalized();
        const JPH::AABox bounds = JPH::AABox(-half, half).Transformed(
            JPH::Mat44::sRotationTranslation(rotation, JPH::Vec3(position)));
        if (mirrors_owned_body(bounds)) {
            ++world_solid_mirrors_;
            continue;
        }
        const float convex_radius = std::min(JPH::cDefaultConvexRadius, half.ReduceMin());
        auto *shape = new JPH::BoxShape(half, convex_radius);
        if (!regression_fixtures_) {
            const auto tag = std::lower_bound(kWorldSolidSoundTags.begin(), kWorldSolidSoundTags.end(), i,
                [](const WorldSolidSoundTag &entry, const std::size_t index) { return entry.box_index < index; });
            if (tag != kWorldSolidSoundTags.end() && tag->box_index == i)
                shape->SetUserData(tag->surface);
        }
        add_shape_body(bodies, shape, position, rotation,
                       JPH::EMotionType::Static, object_layers::kStatic, kWorldSolidFriction,
                       Simulation::kWorldSolidEntityId, 0.0F);
        ++world_solid_bodies_;
    }
    JPH::Array<JPH::Vec3> points;
    for (std::size_t i = 0; i < hull_count; ++i) {
        const WorldSolidHull &hull = hulls[i];
        JPH::DVec3 sum = JPH::DVec3::sZero();
        for (std::uint32_t index = 0; index < hull.count; ++index) {
            const float *point = &hull_points[(hull.first + index) * 3U];
            sum += JPH::DVec3(point[0], point[1], point[2]);
        }
        const JPH::DVec3 centre = sum / static_cast<double>(std::max<std::uint32_t>(hull.count, 1U));
        points.clear();
        for (std::uint32_t index = 0; index < hull.count; ++index) {
            const float *point = &hull_points[(hull.first + index) * 3U];
            points.push_back(JPH::Vec3(static_cast<float>(point[0] - centre.GetX()),
                                       static_cast<float>(point[1] - centre.GetY()),
                                       static_cast<float>(point[2] - centre.GetZ())));
        }
        const JPH::ConvexHullShapeSettings settings(points.data(), static_cast<int>(points.size()));
        const JPH::ShapeSettings::ShapeResult result = settings.Create();
        if (result.HasError()) {
            ++world_solid_rejected_;
            continue;
        }
        add_shape_body(bodies, result.Get().GetPtr(),
                       JPH::RVec3(centre.GetX(), centre.GetY(), centre.GetZ()),
                       JPH::Quat::sIdentity(), JPH::EMotionType::Static, object_layers::kStatic,
                       kWorldSolidFriction, Simulation::kWorldSolidEntityId, 0.0F);
        ++world_solid_bodies_;
    }
}

SupportSoundSurface Simulation::PhysicsWorld::support_sound_surface() const noexcept {
    return grounded_ ? contact_sound_surface(support_sample_) : SupportSoundSurface::Unknown;
}

SupportSoundSurface Simulation::PhysicsWorld::contact_sound_surface(const SupportSample &support) const noexcept {
    // The contact listener resets this sample each collision step. No ray,
    // remembered surface or visual material can supply a grounded receipt.
    if (!support.grounded || support.body_id.IsInvalid() ||
        support.shape_identity == nullptr || support.entity_id != Simulation::kWorldSolidEntityId)
        return SupportSoundSurface::Unknown;
    const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), support.body_id);
    if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase() ||
        lock.GetBody().GetShape() != support.shape_identity ||
        lock.GetBody().GetUserData() != support.entity_id)
        return SupportSoundSurface::Unknown;
    const auto surface = lock.GetBody().GetShape()->GetSubShapeUserData(support.subshape);
    switch (surface) {
    case static_cast<std::uint64_t>(SupportSoundSurface::Concrete): return SupportSoundSurface::Concrete;
    case static_cast<std::uint64_t>(SupportSoundSurface::Metal): return SupportSoundSurface::Metal;
    default: return SupportSoundSurface::Unknown;
    }
}

[[nodiscard]] bool Simulation::PhysicsWorld::mirrors_owned_body(const JPH::AABox &bounds) const {
    JPH::AllHitCollisionCollector<JPH::CollideShapeBodyCollector> collector;
    physics_system_.GetBroadPhaseQuery().CollideAABox(bounds, collector);
    for (const JPH::BodyID id : collector.mHits) {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), id);
        if (!lock.Succeeded() ||
            lock.GetBody().GetUserData() == Simulation::kWorldSolidEntityId) {
            continue;
        }
        const JPH::AABox owned = lock.GetBody().GetWorldSpaceBounds();
        if ((owned.mMin - bounds.mMin).Abs().ReduceMax() < kWorldSolidMirrorTolerance &&
            (owned.mMax - bounds.mMax).Abs().ReduceMax() < kWorldSolidMirrorTolerance) {
            return true;
        }
    }
    return false;
}

void Simulation::PhysicsWorld::build_stack(JPH::BodyInterface &bodies) {
    const auto track = [this](const JPH::BodyID id) {
        machine_bodies_.push_back(id);
        return id;
    };
    const auto frame = [&](const JPH::Vec3 half_extent, const JPH::RVec3 position,
                           const JPH::Quat rotation = JPH::Quat::sIdentity()) {
        track(add_box(bodies, half_extent, position, JPH::EMotionType::Static,
                      object_layers::kStatic, 0.85F, Simulation::kTowerEntityId, rotation));
    };

    const float band_center = kStackHalfExtent - kStackDeckBandDepth * 0.5F;
    const float inner_half = kStackHalfExtent - kStackDeckBandDepth;
    const int level_count = regression_fixtures_ ? kRegressionStackLevelCount : kStackLevelCount;

    // Continuous deck rings retain the tower's receiving surfaces while
    // closing every opening cut for the rejected fourteen-flight bypass.
    for (int level = 1; level <= level_count; ++level) {
        const float deck_y = static_cast<float>(level) * kStackLevelHeight;
        const float slab_y = deck_y - kStackDeckHalfThickness;
        for (const float sz : {1.0F, -1.0F}) {
            frame(JPH::Vec3(kStackHalfExtent, kStackDeckHalfThickness,
                            kStackDeckBandDepth * 0.5F),
                  JPH::RVec3(kStackCenterX, slab_y, kStackCenterZ + sz * band_center));
        }
        for (const float sx : {1.0F, -1.0F}) {
            frame(JPH::Vec3(kStackDeckBandDepth * 0.5F, kStackDeckHalfThickness, inner_half),
                  JPH::RVec3(kStackCenterX + sx * band_center, slab_y, kStackCenterZ));
        }
    }

    // Columns: corners and edge mid-spans, one run per storey. No ordinary
    // stair or ramp bodies are built; ascent is authored by the campaign's
    // mechanisms and parkour routes.
    for (int level = 0; level < level_count; ++level) {
        const float base_y = static_cast<float>(level) * kStackLevelHeight;
        const float column_half = kStackLevelHeight * 0.5F;
        for (const float sx : {1.0F, -1.0F}) {
            for (const float sz : {1.0F, -1.0F}) {
                frame(JPH::Vec3(kStackColumnHalf, column_half, kStackColumnHalf),
                      JPH::RVec3(kStackCenterX + sx * kStackHalfExtent, base_y + column_half,
                                 kStackCenterZ + sz * kStackHalfExtent));
            }
            frame(JPH::Vec3(kStackColumnHalf, column_half, kStackColumnHalf),
                  JPH::RVec3(kStackCenterX + sx * kStackHalfExtent, base_y + column_half,
                             kStackCenterZ));
            frame(JPH::Vec3(kStackColumnHalf, column_half, kStackColumnHalf),
                  JPH::RVec3(kStackCenterX, base_y + column_half,
                             kStackCenterZ + sx * kStackHalfExtent));
        }
    }
}

} // namespace scraperx::sim
