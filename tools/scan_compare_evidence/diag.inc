// Throwaway diagnostic, included by scan_compare.cpp: every floor-grid ray
// of split_patches over the tower, cast by Jolt's NarrowPhaseQuery::CastRay
// (trunk) and by J3's column cast, compared bit for bit.
namespace {
class DiagFixedFilter final : public BodyFilter {
public:
    explicit DiagFixedFilter(const traversal::Ignore &i) : i_(i) {}
    bool ShouldCollide(const BodyID &id) const override { return id != i_.self && id != i_.player; }
    bool ShouldCollideLocked(const Body &b) const override {
        return !b.IsSensor() && !b.IsDynamic() && b.GetID() != i_.self && b.GetID() != i_.player;
    }

private:
    traversal::Ignore i_;
};
class DiagBodies final : public CollideShapeBodyCollector {
public:
    std::vector<BodyID> ids;
    void AddHit(const BodyID &id) override { ids.push_back(id); }
};
class DiagLeaves final : public TransformedShapeCollector {
public:
    std::vector<TransformedShape> shapes;
    void AddHit(const TransformedShape &s) override { shapes.push_back(s); }
};
struct DPatch {
    BodyID body;
    float top, min_x, max_x, min_z, max_z;
};
struct DSteel {
    BodyID id;
    TransformedShape shape;
    AABox bounds;
};

void print_body(const PhysicsSystem &physics, const BodyID id, const char *what) {
    BodyLockRead lock(physics.GetBodyLockInterface(), id);
    if (!lock.Succeeded()) {
        std::printf("   %s %08x: no body\n", what, id.GetIndexAndSequenceNumber());
        return;
    }
    const Body &b = lock.GetBody();
    const AABox wb = b.GetWorldSpaceBounds();
    std::printf("   %s %08x motion=%d layer=%u user=%llu active=%d in_bp=%d subtype=%d bounds=(%.5f %.5f %.5f)-(%.5f %.5f %.5f)\n", what,
                id.GetIndexAndSequenceNumber(), int(b.GetMotionType()), unsigned(b.GetObjectLayer()),
                static_cast<unsigned long long>(b.GetUserData()), int(b.IsActive()), int(b.IsInBroadPhase()),
                int(b.GetShape()->GetSubType()), wb.mMin.GetX(), wb.mMin.GetY(), wb.mMin.GetZ(), wb.mMax.GetX(), wb.mMax.GetY(),
                wb.mMax.GetZ());
}

void diag() {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    g_label = "diag_tower";
    (void)s.advance_frame(0.1);
    (void)s.matron_affordances();
    const PhysicsSystem &physics = *g_last.physics;
    const traversal::Ignore ignore = g_last.ignore;
    const traversal::Limits limits = g_last.limits;
    const AABox bounds = g_last.bounds;
    const BroadPhaseLayerFilter any_bp;
    const ObjectLayerFilter any_ol;
    const DefaultBroadPhaseLayerFilter bp0 = physics.GetDefaultBroadPhaseLayerFilter(ObjectLayer(ignore.layer < 0 ? 0 : ignore.layer));
    const DefaultObjectLayerFilter ol0 = physics.GetDefaultLayerFilter(ObjectLayer(ignore.layer < 0 ? 0 : ignore.layer));
    const BroadPhaseLayerFilter &bp = ignore.layer < 0 ? any_bp : static_cast<const BroadPhaseLayerFilter &>(bp0);
    const ObjectLayerFilter &ol = ignore.layer < 0 ? any_ol : static_cast<const ObjectLayerFilter &>(ol0);
    const DiagFixedFilter filter(ignore);
    std::printf("ignore self=%08x player=%08x layer=%d\n", ignore.self.GetIndexAndSequenceNumber(),
                ignore.player.GetIndexAndSequenceNumber(), ignore.layer);
    // Tops, as scan_steel reads them before split_patches.
    DiagBodies found;
    physics.GetBroadPhaseQuery().CollideAABox(bounds, found);
    std::sort(found.ids.begin(), found.ids.end());
    std::vector<DPatch> patches;
    for (const BodyID id : found.ids) {
        if (id == ignore.self || id == ignore.player) continue;
        BodyLockRead lock(physics.GetBodyLockInterface(), id);
        if (!lock.Succeeded()) continue;
        const Body &body = lock.GetBody();
        if (body.IsSensor() || body.IsDynamic()) continue;
        DiagLeaves leaves;
        body.GetShape()->CollectTransformedShapes(bounds, body.GetCenterOfMassPosition(), body.GetRotation(), Vec3::sReplicate(1.0F),
                                                  SubShapeIDCreator(), leaves, {});
        for (const TransformedShape &ts : leaves.shapes) {
            const AABox box = ts.GetWorldSpaceBounds();
            const Vec3 size = box.GetSize();
            const float sx = size.GetX(), sy = size.GetY(), sz = size.GetZ();
            if (sy <= 0.10F && std::min(sx, sz) <= 0.10F && std::max(sx, sz) >= 0.25F && std::max(sx, sz) <= 3.0F) continue;
            if (sy >= 1.5F && std::max(sx, sz) <= 0.16F) continue;
            const bool aligned = ts.mShapeRotation.RotateAxisY().GetY() > 0.995F && ts.mShape->GetSubType() == EShapeSubType::Box;
            if (aligned && std::min(sx, sz) >= 0.18F)
                patches.push_back({id, box.mMax.GetY(), box.mMin.GetX(), box.mMax.GetX(), box.mMin.GetZ(), box.mMax.GetZ()});
        }
    }
    constexpr float kCell = 0.10F, kTall = 3.0F;
    long long rays = 0, frac_diff = 0, floor_diff = 0;
    int shown = 0;
    for (std::size_t i = 0; i < patches.size(); ++i) {
        const DPatch &p = patches[i];
        bool candidate = false;
        for (std::size_t j = 0; j < patches.size() && !candidate; ++j) {
            const DPatch &q = patches[j];
            candidate = j != i && q.top > p.top + limits.step_max_m && q.top < p.top + kTall && q.max_x > p.min_x &&
                        q.min_x < p.max_x && q.max_z > p.min_z && q.min_z < p.max_z;
        }
        const int nx = int(std::ceil((p.max_x - p.min_x) / kCell)), nz = int(std::ceil((p.max_z - p.min_z) / kCell));
        if (!candidate || nx < 2 || nz < 2 || nx * nz > 400000) continue;
        const float from_y = p.top + 2.0F * limits.center_above_feet() + 0.05F;
        const float depth = from_y - p.top + 0.1F;
        const AABox column(Vec3(p.min_x - 0.05F, p.top - 0.15F, p.min_z - 0.05F),
                           Vec3(p.max_x + 0.05F, from_y + 0.05F, p.max_z + 0.05F));
        DiagBodies col;
        physics.GetBroadPhaseQuery().CollideAABox(column, col, bp, ol);
        std::sort(col.ids.begin(), col.ids.end());
        std::vector<DSteel> steel;
        for (const BodyID id : col.ids) {
            if (!filter.ShouldCollide(id)) continue;
            BodyLockRead lock(physics.GetBodyLockInterface(), id);
            if (!lock.SucceededAndIsInBroadPhase()) continue;
            const Body &body = lock.GetBody();
            if (!filter.ShouldCollideLocked(body)) continue;
            steel.push_back({id, body.GetTransformedShape(), body.GetWorldSpaceBounds()});
        }
        const Vec3 down(0.0F, -depth, 0.0F);
        const RayInvDirection inverse(down);
        for (int ix = 0; ix < nx; ++ix) {
            const float x = std::min(p.min_x + (float(ix) + 0.5F) * kCell, p.max_x - 0.01F);
            for (int iz = 0; iz < nz; ++iz) {
                const float z = std::min(p.min_z + (float(iz) + 0.5F) * kCell, p.max_z - 0.01F);
                const RRayCast ray(RVec3(x, from_y, z), down);
                RayCastResult a;
                const bool any_a = physics.GetNarrowPhaseQuery().CastRay(ray, a, bp, ol, filter);
                RayCastResult b;
                for (const DSteel &st : steel) {
                    const Vec3 lo = st.bounds.mMin, hi = st.bounds.mMax;
                    if (!(lo.GetX() <= x && x <= hi.GetX())) continue;
                    if (!(lo.GetZ() <= z && z <= hi.GetZ())) continue;
                    const float entry = RayAABox4(Vec3(ray.mOrigin), inverse, lo.SplatX(), lo.SplatY(), lo.SplatZ(), hi.SplatX(),
                                                  hi.SplatY(), hi.SplatZ()).GetX();
                    if (entry < b.mFraction) (void)st.shape.CastRay(ray, b);
                }
                const bool any_b = b.mFraction <= 1.0F;
                ++rays;
                const float ha = any_a ? from_y - a.mFraction * depth : p.top;
                const float hb = any_b ? from_y - b.mFraction * depth : p.top;
                const bool fa = ha <= p.top + limits.step_max_m, fb = hb <= p.top + limits.step_max_m;
                const bool differs = any_a != any_b || (any_a && a.mFraction != b.mFraction);
                if (differs) ++frac_diff;
                if (fa != fb) ++floor_diff;
                if (differs && shown < 12) {
                    ++shown;
                    std::printf("MISMATCH top#%u on %08x top_y=%.5f x=%.5f z=%.5f from_y=%.5f | jolt any=%d f=%.9g body=%08x floor=%d | "
                                "j3 any=%d f=%.9g body=%08x floor=%d | column bodies=%zu\n",
                                unsigned(i), p.body.GetIndexAndSequenceNumber(), p.top, x, z, from_y, int(any_a), a.mFraction,
                                a.mBodyID.GetIndexAndSequenceNumber(), int(fa), int(any_b), b.mFraction,
                                b.mBodyID.GetIndexAndSequenceNumber(), int(fb), steel.size());
                    if (any_a) {
                        print_body(physics, a.mBodyID, "jolt hit");
                        bool in_col = false;
                        for (const DSteel &st : steel) in_col = in_col || st.id == a.mBodyID;
                        const Vec3 at = Vec3(ray.mOrigin) + a.mFraction * down;
                        DiagBodies probe;
                        physics.GetBroadPhaseQuery().CollideAABox(AABox(at - Vec3::sReplicate(0.001F), at + Vec3::sReplicate(0.001F)),
                                                                  probe, bp, ol);
                        bool bp_has = false;
                        for (const BodyID id : probe.ids) bp_has = bp_has || id == a.mBodyID;
                        std::printf("   jolt hit body in J3 column list=%d; hit point (%.5f %.5f %.5f); broadphase box there finds it=%d\n",
                                    int(in_col), at.GetX(), at.GetY(), at.GetZ(), int(bp_has));
                    }
                    if (any_b) print_body(physics, b.mBodyID, "j3 hit");
                }
            }
        }
    }
    std::printf("DIAG candidate-top rays=%lld fraction_differs=%lld floor_cell_differs=%lld\n", rays, frac_diff, floor_diff);
    // The top whose split differs (body index 14, top 44): its first columns,
    // x fused (one rounding) and unfused (two), and what a ray at each hits.
    for (std::size_t i = 0; i < patches.size(); ++i) {
        const DPatch &p = patches[i];
        if (p.body.GetIndex() != 14 || p.top != 44.0F) continue;
        std::printf("TOP#%zu body=%08x top=%a x=[%a, %a] z=[%a, %a]\n", i, p.body.GetIndexAndSequenceNumber(), p.top, p.min_x, p.max_x,
                    p.min_z, p.max_z);
        const float from_y = p.top + 2.0F * limits.center_above_feet() + 0.05F;
        const float depth = from_y - p.top + 0.1F;
        const int nz = int(std::ceil((p.max_z - p.min_z) / 0.10F));
        for (int ix = 385; ix < 391; ++ix) {
            volatile float vprod = (float(ix) + 0.5F) * 0.10F;
            const float x_two = std::min(p.min_x + vprod, p.max_x - 0.01F);
            const float x_one = std::min(std::fma(float(ix) + 0.5F, 0.10F, p.min_x), p.max_x - 0.01F);
            int differ = 0, floor_one = 0, floor_two = 0;
            for (int iz = 0; iz < nz; ++iz) {
                const float z = std::min(p.min_z + (float(iz) + 0.5F) * 0.10F, p.max_z - 0.01F);
                RayCastResult a, b;
                const bool any_a = physics.GetNarrowPhaseQuery().CastRay(RRayCast(RVec3(x_one, from_y, z), Vec3(0, -depth, 0)), a, bp, ol, filter);
                const bool any_b = physics.GetNarrowPhaseQuery().CastRay(RRayCast(RVec3(x_two, from_y, z), Vec3(0, -depth, 0)), b, bp, ol, filter);
                const float ha = any_a ? from_y - a.mFraction * depth : p.top, hb = any_b ? from_y - b.mFraction * depth : p.top;
                floor_one += ha <= p.top + limits.step_max_m;
                floor_two += hb <= p.top + limits.step_max_m;
                if (any_a != any_b || a.mFraction != b.mFraction) {
                    if (differ++ < 3)
                        std::printf("   ix=%d iz=%d fused x hits %08x f=%a | unfused x hits %08x f=%a\n", ix, iz,
                                    a.mBodyID.GetIndexAndSequenceNumber(), a.mFraction, b.mBodyID.GetIndexAndSequenceNumber(), b.mFraction);
                }
            }
            std::printf("  ix=%d x fused=%a unfused=%a same=%d | rays differing=%d floor cells fused=%d unfused=%d of %d\n", ix, x_one, x_two,
                        int(x_one == x_two), differ, floor_one, floor_two, nz);
        }
    }
    // Every body whose bounds edge in x sits within 1e-4 of 12.75.
    BodyIDVector all;
    physics.GetBodies(all);
    for (const BodyID id : all) {
        BodyLockRead lock(physics.GetBodyLockInterface(), id);
        if (!lock.Succeeded() || lock.GetBody().IsDynamic()) continue;
        const AABox wb = lock.GetBody().GetWorldSpaceBounds();
        if ((std::abs(wb.mMin.GetX() - 12.75F) < 1.0e-4F || std::abs(wb.mMax.GetX() - 12.75F) < 1.0e-4F) && wb.mMax.GetY() > 43.9F &&
            wb.mMin.GetY() < 46.5F && wb.mMax.GetZ() > -132.0F && wb.mMin.GetZ() < -129.9F)
            std::printf("  EDGE body %08x x=[%a, %a] y=[%.4f, %.4f] z=[%.4f, %.4f]\n", id.GetIndexAndSequenceNumber(), wb.mMin.GetX(),
                        wb.mMax.GetX(), wb.mMin.GetY(), wb.mMax.GetY(), wb.mMin.GetZ(), wb.mMax.GetZ());
    }
}
} // namespace
