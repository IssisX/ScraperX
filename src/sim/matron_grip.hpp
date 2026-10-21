// Her hands on steel (matron.cpp climb(), matron_pipe.cpp): the queries both
// her ladders and her corner pipe take and keep holds by. Bounded, no heap.
#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>

#include <algorithm>
#include <array>

namespace scraperx::sim::grip {

// Two exclusions without the heap storage used by IgnoreMultipleBodiesFilter.
class ClimbBodyFilter final : public JPH::BodyFilter {
public:
    ClimbBodyFilter(JPH::BodyID actor, JPH::BodyID player) : actor_(actor), player_(player) {}
    bool ShouldCollide(const JPH::BodyID &id) const override { return id != actor_ && id != player_; }
    bool ShouldCollideLocked(const JPH::Body &body) const override { return !body.IsSensor(); }

private:
    JPH::BodyID actor_, player_;
};

// The hits along one ray, at most kMax of them: bounded, no heap.
class RayHits final : public JPH::CastRayCollector {
public:
    static constexpr int kMax = 12;
    void AddHit(const JPH::RayCastResult &hit) override {
        if (count < kMax) {
            hits[static_cast<std::size_t>(count++)] = hit;
        } else {
            ForceEarlyOut();
        }
    }
    void sort() {
        std::sort(hits.begin(), hits.begin() + count,
                  [](const JPH::RayCastResult &a, const JPH::RayCastResult &b) { return a.mFraction < b.mFraction; });
    }
    int count = 0;
    std::array<JPH::RayCastResult, kMax> hits{};
};

// Anything solid at a point (sensors and the excluded bodies aside).
class PointHit final : public JPH::CollidePointCollector {
public:
    void AddHit(const JPH::CollidePointResult &) override {
        hit = true;
        ForceEarlyOut();
    }
    bool hit = false;
};

// What a hand at `point` rests on: the closest top met just over it, steel the
// check starts inside not counted (a bridge over a ladder's head). The hold
// is taken and kept by this one query, so a seam between two parts of a
// compound reads the same every time.
inline bool hand_rests_on(const JPH::NarrowPhaseQuery &query, const JPH::RVec3 point, const JPH::BodyFilter &filter,
                          JPH::BodyID &body, JPH::SubShapeID &member) {
    const JPH::RRayCast ray(point + JPH::Vec3(0.0F, 0.04F, 0.0F), JPH::Vec3(0.0F, -0.08F, 0.0F));
    JPH::RayCastSettings settings;
    settings.mTreatConvexAsSolid = false;
    JPH::ClosestHitCollisionCollector<JPH::CastRayCollector> hit;
    query.CastRay(ray, settings, hit, {}, {}, filter);
    if (!hit.HadHit()) {
        return false;
    }
    body = hit.mHit.mBodyID;
    member = hit.mHit.mSubShapeID2;
    return true;
}

} // namespace scraperx::sim::grip
