// Her risers (matron.hpp): any tall vertical member her scan found
// (traversal::Riser) with decks beside it is a way of hers between them.
// riser_routes reads each from the world once: the decks it stands beside,
// her stand on each, the top her hands take at the deck's edge (a rail's top,
// or the edge itself), the face she climbs and her line beside it, chosen
// where there is room for all of her the whole way. Nothing here names a
// place. Gravity is on all the way and she meets every steel she passes. Her
// hands are one capped force at real contacts, the edge's top or the riser's
// face, kept in that body's frame and checked every step, as on her ladders
// (her Climber's holds); a dynamic support takes the reaction. On the face
// both hands hold it, one over the other, and the grip is friction: what
// holds her up or lets her down is at most her squeeze times kPipeFriction.
// Her hips keep as close to the face as the steel round it leaves room for
// (her stand-off widens past a flange). A hand that loses its steel, a load
// past her grip for a moment, or no headway for 2.5 s lets go: on the riser
// her legs alone slow her (kPipeSlideAccel) until they meet the steel under
// her; at an edge she drops where she is.

#include "sim/matron.hpp"
#include "sim/matron_grip.hpp"
#include "sim/matron_effort.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace scraperx::sim {
using namespace JPH;

namespace {
using grip::ClimbBodyFilter;
using grip::hand_rests_on;
constexpr float kTurnRate = 7.0F;
constexpr float kPiF = 3.14159265358979F;
constexpr float kPipeStallS = 2.5F;
constexpr float kPipeOverloadS = 0.18F;
constexpr float kPipeOffAxisM = 0.85F;    // a hold on the riser or its flange: its corner is 0.49 m off its axis, a flange's 0.78
constexpr float kPipeLegsLineN = 800.0F;  // her legs' pull back onto her line while she slides
// Her legs come down onto a flange: they bear on it, a support, not a grip.
// What they can take is her one budget (3.7 times her weight); past it they
// give and she sinks, but they do not open.
constexpr float kPipeLegsOnFlangeN = traversal::Limits{}.total_force_n;
// Her stand-off from the face (her side's surface from it), least first: as
// close as there is room for her. The widest is what the route must leave
// her the whole way.
constexpr float kRiserGaps[] = {0.04F, 0.12F, 0.2F, 0.3F, 0.4F, 0.5F};
constexpr float kRiserWidestGap = 0.5F;
constexpr float kRiserDeckReachM = 1.3F;   // a deck this near the riser is one it joins (her arms over the edge)
constexpr float kRiserDeckAreaM2 = 2.0F;   // a deck, not a ledge
constexpr float kRiserStoryMinM = 2.5F;    // less is a step, not a climb
constexpr float kRiserHandSideM = 0.12F;   // each hand off her middle along the face
// The deck's edge beam, under its edge: a hand's breadth inside the rail it
// stands under (the corner pipe: the rail at z -133.0, the beam at -133.23).
constexpr float kPipeLipInFromRailM = 0.23F;
float turn_to(const float yaw, const float want, const float rate, const float dt) {
    float delta = std::remainder(want - yaw, 2.0F * kPiF);
    delta = std::clamp(delta, -rate * dt, rate * dt);
    return std::remainder(yaw + delta, 2.0F * kPiF);
}
float distance_xz(const float ax, const float az, const float bx, const float bz) { return std::hypot(ax - bx, az - bz); }
float distance_xz(const Vec3 a, const Vec3 b) { return std::hypot(a.GetX() - b.GetX(), a.GetZ() - b.GetZ()); }
// Level, from a point toward a riser's axis.
Vec3 into_riser(const RVec3 from, const float cx, const float cz) {
    const Vec3 in(cx - float(from.GetX()), 0.0F, cz - float(from.GetZ()));
    return in.LengthSq() > 1.0e-6F ? in.Normalized() : Vec3::sAxisX();
}
// The first top under a point within a span below it (normal up), or none.
bool top_under(const NarrowPhaseQuery &query, const BodyFilter &filter, const float x, const float from_y,
               const float z, const float span, float &top) {
    const RRayCast ray(RVec3(x, from_y, z), Vec3(0.0F, -span, 0.0F));
    RayCastSettings settings;
    settings.mTreatConvexAsSolid = false;
    ClosestHitCollisionCollector<CastRayCollector> hit;
    query.CastRay(ray, settings, hit, {}, {}, filter);
    if (!hit.HadHit()) {
        return false;
    }
    top = from_y - span * hit.mHit.mFraction;
    return true;
}
} // namespace

// ---- her risers, from her scan ---------------------------------------------

Vec3 Matron::riser_line(const RiserRoute &r, const float gap) const {
    return Vec3(r.cx, 0.0F, r.cz) + r.out * (r.face + kRadius + gap) + r.along * r.lateral;
}

Vec3 Matron::riser_line_at(const RiserRoute &r, const float y, const float ahead) const {
    for (const float gap : kRiserGaps) {
        const Vec3 line = riser_line(r, gap);
        if (shape_fits(shape_, RVec3(line.GetX(), y, line.GetZ())) &&
            (ahead == 0.0F || shape_fits(shape_, RVec3(line.GetX(), y + ahead, line.GetZ())))) {
            return line + Vec3(0.0F, gap, 0.0F);   // (y carries the gap chosen)
        }
    }
    return riser_line(r, kRiserWidestGap) + Vec3(0.0F, kRiserWidestGap, 0.0F);
}

bool Matron::on_riser_line(const RiserRoute &r, const float x, const float z) const {
    // Within 0.5 m of the stretch of her stand-offs out from the face.
    const Vec3 a = riser_line(r, kRiserGaps[0]);
    const Vec3 b = riser_line(r, kRiserWidestGap);
    const Vec3 ab = b - a;
    const float u = std::clamp((Vec3(x, 0.0F, z) - a).Dot(ab) / std::max(1.0e-6F, ab.LengthSq()), 0.0F, 1.0F);
    return distance_xz(Vec3(x, 0.0F, z), a + ab * u) < 0.5F;
}

const std::vector<Matron::RiserRoute> &Matron::riser_routes() {
    if (scanned_ == riser_routes_of_) {
        return riser_routes_;
    }
    riser_routes_of_ = scanned_;
    riser_routes_.clear();
    if (scanned_ == nullptr) {
        return riser_routes_;
    }
    const NarrowPhaseQuery &query = physics_.GetNarrowPhaseQuery();
    const ClimbBodyFilter filter(body_, player_);
    const auto &patches = scanned_->patches();
    const auto &regions = scanned_->regions();
    const auto stands_fit = [&](const Vec3 feet) {
        return shape_fits(shape_, RVec3(feet.GetX(), feet.GetY() + kStandCentre + 0.05F, feet.GetZ()));
    };
    for (std::size_t ri = 0; ri < scanned_->risers().size(); ++ri) {
        const traversal::Riser &rs = scanned_->risers()[ri];
        struct Deck {
            RiserDeck d;
            bool foot = false;   // the riser stands on it
            float off = 0.0F;
        };
        std::vector<Deck> decks;
        std::vector<float> collars;
        for (const traversal::Patch &p : patches) {
            if (p.region < 0 || p.top < rs.bottom - 0.3F || p.top > rs.top + 0.3F) {
                continue;
            }
            const float gx = std::max({0.0F, p.min_x - (rs.x + rs.half_x), (rs.x - rs.half_x) - p.max_x});
            const float gz = std::max({0.0F, p.min_z - (rs.z + rs.half_z), (rs.z - rs.half_z) - p.max_z});
            Deck deck;
            deck.d.floor_y = p.top;
            deck.d.region = p.region;
            if (gx <= 0.02F && gz <= 0.02F) {
                // Steel it threads, round it and no wider than a collar: a flange.
                const bool collar = p.min_x > rs.x - rs.half_x - 0.6F && p.max_x < rs.x + rs.half_x + 0.6F &&
                                    p.min_z > rs.z - rs.half_z - 0.6F && p.max_z < rs.z + rs.half_z + 0.6F;
                if (collar) {
                    if (p.top < rs.top - 0.3F) {
                        collars.push_back(p.top);
                    }
                    continue;
                }
                if (std::abs(p.top - rs.bottom) > 0.3F) {
                    continue;   // a deck it goes up through: not a way of hers by its edge
                }
                deck.foot = true;
            } else {
                if (std::hypot(gx, gz) > kRiserDeckReachM ||
                    regions[static_cast<std::size_t>(p.region)].area < kRiserDeckAreaM2) {
                    continue;
                }
                // Her stand: the deck's nearest to the riser, room for all of
                // her; along its edge to the nearest that has it.
                const float inset = kRadius + 0.1F;
                const auto clamp_in = [&](const float v, const float lo, const float hi) {
                    return lo + inset > hi - inset ? 0.5F * (lo + hi) : std::clamp(v, lo + inset, hi - inset);
                };
                const Vec3 near(clamp_in(rs.x, p.min_x, p.max_x), p.top, clamp_in(rs.z, p.min_z, p.max_z));
                const Vec3 edge = std::abs(near.GetX() - rs.x) > std::abs(near.GetZ() - rs.z) ? Vec3::sAxisZ() : Vec3::sAxisX();
                bool found = false;
                for (float slide = 0.0F; slide <= 2.0F && !found; slide += 0.25F) {
                    for (const float sign : {1.0F, -1.0F}) {
                        const Vec3 at = near + edge * (sign * slide);
                        if (at.GetX() < p.min_x || at.GetX() > p.max_x || at.GetZ() < p.min_z || at.GetZ() > p.max_z ||
                            !stands_fit(at)) {
                            continue;
                        }
                        deck.d.stand = at;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    continue;
                }
                deck.off = distance_xz(deck.d.stand, Vec3(rs.x, 0.0F, rs.z));
            }
            // One deck a height: the nearest.
            bool merged = false;
            for (Deck &other : decks) {
                if (std::abs(other.d.floor_y - deck.d.floor_y) < 0.3F) {
                    if (deck.foot || (!other.foot && deck.off < other.off)) {
                        other = deck;
                    }
                    merged = true;
                }
            }
            if (!merged) {
                decks.push_back(deck);
            }
        }
        std::sort(decks.begin(), decks.end(), [](const Deck &a, const Deck &b) { return a.d.floor_y < b.d.floor_y; });
        std::sort(collars.begin(), collars.end());
        for (std::size_t k = 0; k + 1 < decks.size(); ++k) {
            Deck lo = decks[k];
            Deck hi = decks[k + 1];
            if (hi.d.floor_y - lo.d.floor_y < kRiserStoryMinM || hi.foot) {
                continue;
            }
            RiserRoute best;
            float best_score = std::numeric_limits<float>::max();
            // Its four faces and along each, her line: nearest her stands
            // first, the first with room for her the whole way.
            struct Candidate {
                Vec3 out, along;
                float face, face_half, lateral, score;
            };
            std::vector<Candidate> candidates;
            for (int f = 0; f < 4; ++f) {
                const bool x_face = f < 2;
                const float sign = (f % 2 == 0) ? 1.0F : -1.0F;
                Candidate c;
                c.out = x_face ? Vec3(sign, 0.0F, 0.0F) : Vec3(0.0F, 0.0F, sign);
                c.along = x_face ? Vec3::sAxisZ() : Vec3::sAxisX();
                c.face = x_face ? rs.half_x : rs.half_z;
                c.face_half = x_face ? rs.half_z : rs.half_x;
                for (const float u : {-1.0F, -0.5F, 0.0F, 0.5F, 1.0F}) {
                    c.lateral = u * c.face_half + (u > 0.9F ? 0.1F : u < -0.9F ? -0.1F : 0.0F);
                    const Vec3 line = Vec3(rs.x, 0.0F, rs.z) + c.out * (c.face + kRadius + kRiserGaps[2]) + c.along * c.lateral;
                    c.score = (lo.foot ? 0.0F : distance_xz(line, lo.d.stand)) + distance_xz(line, hi.d.stand);
                    candidates.push_back(c);
                }
            }
            std::sort(candidates.begin(), candidates.end(),
                      [](const Candidate &a, const Candidate &b) { return a.score < b.score; });
            for (const Candidate &c : candidates) {
                RiserRoute r;
                r.riser = static_cast<int>(ri);
                r.cx = rs.x;
                r.cz = rs.z;
                r.hx = rs.half_x;
                r.hz = rs.half_z;
                r.bottom = rs.bottom;
                r.top = rs.top;
                r.out = c.out;
                r.along = c.along;
                r.face = c.face;
                r.face_half = c.face_half;
                r.lateral = c.lateral;
                r.lo = lo.d;
                r.hi = hi.d;
                const Vec3 line = riser_line(r, kRiserGaps[2]);
                // Each deck's edge between her stand and her line: the highest
                // top along the way over (a rail's), or the deck's own edge.
                const auto edge_of = [&](RiserDeck &d, const bool foot) {
                    float top = 0.0F;
                    d.line_on_floor = foot || (top_under(query, filter, line.GetX(), d.floor_y + 0.3F, line.GetZ(), 0.45F, top) &&
                                               std::abs(top - d.floor_y) < 0.1F);
                    if (d.line_on_floor) {
                        // Her stand is her line, as close to the face as she fits.
                        const Vec3 close = riser_line(r, kRiserGaps[0]);
                        d.stand = Vec3(close.GetX(), d.floor_y, close.GetZ());
                        if (!stands_fit(d.stand)) {
                            d.stand = Vec3(line.GetX(), d.floor_y, line.GetZ());
                        }
                        d.rail = d.stand;
                        return stands_fit(d.stand);
                    }
                    const float run = distance_xz(d.stand, line);
                    bool any = false;
                    for (float t = 0.0F; t <= run; t += 0.1F) {
                        const Vec3 at = d.stand + (Vec3(line.GetX(), d.floor_y, line.GetZ()) - d.stand) * (t / std::max(run, 1.0e-3F));
                        if (!top_under(query, filter, at.GetX(), d.floor_y + 1.6F, at.GetZ(), 1.75F, top) ||
                            top < d.floor_y - 0.05F) {
                            continue;
                        }
                        if (top > d.floor_y + 1.5F) {
                            return false;   // a wall between: not her way over
                        }
                        if (!any || top >= d.rail.GetY() - 0.01F) {
                            d.rail = Vec3(at.GetX(), top, at.GetZ());
                            any = true;
                        }
                    }
                    if (!any) {
                        return false;
                    }
                    const float over = d.rail.GetY() + kPipeOverRailM + kStandCentre;
                    return shape_fits(shape_, RVec3(d.stand.GetX(), over, d.stand.GetZ())) &&
                           shape_fits(shape_, RVec3(line.GetX(), over, line.GetZ()));
                };
                if (!edge_of(r.lo, lo.foot) || !edge_of(r.hi, false) || r.hi.line_on_floor) {
                    continue;
                }
                // Room for all of her on her line the whole way, at her
                // widest stand-off at most.
                bool room = true;
                const float from = r.lo.line_on_floor ? r.lo.floor_y + kStandCentre + 0.1F
                                                      : r.lo.rail.GetY() + kPipeOverRailM + kStandCentre;
                const float to = r.hi.floor_y + kStandCentre - 0.3F;
                for (float y = from; y <= to + 0.01F && room; y = (y + 0.5F > to && y < to) ? to : y + 0.5F) {
                    bool fits = false;
                    for (const float gap : {kRiserWidestGap, kRiserGaps[2], kRiserGaps[0]}) {
                        const Vec3 l = riser_line(r, gap);
                        if (shape_fits(shape_, RVec3(l.GetX(), y, l.GetZ()))) {
                            fits = true;
                            break;
                        }
                    }
                    room = fits;
                }
                if (!room) {
                    continue;
                }
                for (const float top : collars) {
                    if (top > r.lo.floor_y && top < r.hi.floor_y) {
                        r.collars.push_back(top);
                    }
                }
                best = r;
                best_score = c.score;
                break;
            }
            if (best.riser >= 0 && best_score < std::numeric_limits<float>::max()) {
                riser_routes_.push_back(best);
            }
        }
    }
    return riser_routes_;
}

bool Matron::riser_joins(const float a, const float b) {
    for (const RiserRoute &r : riser_routes()) {
        if ((std::abs(r.lo.floor_y - a) <= kOnSteelM && std::abs(r.hi.floor_y - b) <= kOnSteelM) ||
            (std::abs(r.lo.floor_y - b) <= kOnSteelM && std::abs(r.hi.floor_y - a) <= kOnSteelM)) {
            return true;
        }
    }
    return false;
}

// ---- her hands on it -------------------------------------------------------

bool Matron::riser_face_probe(const RVec3 at, const float y, const float side, RiserHold &out) const {
    if (pipe_route_ < 0 || static_cast<std::size_t>(pipe_route_) >= riser_routes_.size()) {
        return false;
    }
    const RiserRoute &r = riser_routes_[static_cast<std::size_t>(pipe_route_)];
    const ClimbBodyFilter filter(body_, player_);
    // From her side of the face, over the spot her hand goes (on the face's
    // run), level into it: the first steel met, in her reach.
    const float along = std::clamp(r.lateral + side, -r.face_half + 0.05F, r.face_half - 0.05F);
    const Vec3 over = Vec3(r.cx, 0.0F, r.cz) + r.out * (r.face + kRadius + kRiserWidestGap) + r.along * along;
    const RRayCast ray(RVec3(over.GetX(), y, over.GetZ()), -r.out * (kRadius + kRiserWidestGap + 0.1F));
    RayCastSettings settings;
    settings.mTreatConvexAsSolid = false;
    ClosestHitCollisionCollector<CastRayCollector> hit;
    physics_.GetNarrowPhaseQuery().CastRay(ray, settings, hit, {}, {}, filter);
    if (!hit.HadHit()) {
        return false;
    }
    const RVec3 point = ray.GetPointOnRay(hit.mHit.mFraction);
    // On the riser (or a collar on it), and in her reach.
    if (Vec3(point - at).Length() > kClimbReachM ||
        distance_xz(float(point.GetX()), float(point.GetZ()), r.cx, r.cz) > std::hypot(r.hx, r.hz) + 0.36F) {
        return false;
    }
    const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mHit.mBodyID);
    if (!lock.Succeeded() || !lock.GetBody().IsStatic() ||
        std::abs(lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mHit.mSubShapeID2, point).GetY()) > 0.3F) {
        return false;
    }
    out.body = hit.mHit.mBodyID;
    out.member = hit.mHit.mSubShapeID2;
    out.local = Vec3(lock.GetBody().GetInverseCenterOfMassTransform() * point);
    out.point = point;
    return true;
}

bool Matron::riser_hold_kept(const RiserHold &hold, const RVec3 at) const {
    if (pipe_route_ < 0 || hold.body.IsInvalid() || !physics_.GetBodyInterface().IsAdded(hold.body)) {
        return false;
    }
    const RiserRoute &r = riser_routes_[static_cast<std::size_t>(pipe_route_)];
    const RVec3 point = physics_.GetBodyInterface().GetCenterOfMassTransform(hold.body) * hold.local;
    if (Vec3(point - at).Length() > kClimbReachM) {
        return false;
    }
    // The face met reaching into it from just off the hand, within a few centimetres.
    const ClimbBodyFilter filter(body_, player_);
    const RRayCast ray(point + r.out * 0.05F, -r.out * 0.1F);
    RayCastSettings settings;
    settings.mTreatConvexAsSolid = false;
    ClosestHitCollisionCollector<CastRayCollector> hit;
    physics_.GetNarrowPhaseQuery().CastRay(ray, settings, hit, {}, {}, filter);
    return hit.HadHit() && hit.mHit.mBodyID == hold.body && hit.mHit.mSubShapeID2 == hold.member;
}

bool Matron::pipe_top_under(const RVec3 rail, const Vec3 across, BodyID &body, SubShapeID &member,
                            RVec3 &point) const {
    const ClimbBodyFilter filter(body_, player_);
    for (const float dz : {0.0F, 0.03F, -0.03F}) {
        const RVec3 over = rail + across * dz;
        const RRayCast ray(RVec3(over.GetX(), rail.GetY() + 0.15F, over.GetZ()), Vec3(0.0F, -0.3F, 0.0F));
        RayCastSettings settings;
        settings.mTreatConvexAsSolid = false;
        ClosestHitCollisionCollector<CastRayCollector> hit;
        physics_.GetNarrowPhaseQuery().CastRay(ray, settings, hit, {}, {}, filter);
        if (!hit.HadHit()) {
            continue;
        }
        const RVec3 at = ray.GetPointOnRay(hit.mHit.mFraction);
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mHit.mBodyID);
        if (!lock.Succeeded() || lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mHit.mSubShapeID2, at).GetY() < 0.7F) {
            continue;
        }
        BodyID rests_body;
        SubShapeID rests_member;
        if (!hand_rests_on(physics_.GetNarrowPhaseQuery(), at, filter, rests_body, rests_member) ||
            rests_body != hit.mHit.mBodyID) {
            continue;
        }
        body = rests_body;
        member = rests_member;
        point = at;
        return true;
    }
    return false;
}

bool Matron::pipe_riser_top_under(const RVec3 at, float &top_y) const {
    if (pipe_route_ < 0 || static_cast<std::size_t>(pipe_route_) >= riser_routes_.size()) {
        return false;
    }
    const RiserRoute &r = riser_routes_[static_cast<std::size_t>(pipe_route_)];
    const ClimbBodyFilter filter(body_, player_);
    const RRayCast ray(RVec3(r.cx, at.GetY(), r.cz), Vec3(0.0F, -(kStandCentre + kPipeCatchFallM), 0.0F));
    RayCastSettings settings;
    settings.mTreatConvexAsSolid = true;   // starting inside the riser: a hit at 0, it goes on past her
    ClosestHitCollisionCollector<CastRayCollector> hit;
    physics_.GetNarrowPhaseQuery().CastRay(ray, settings, hit, {}, {}, filter);
    if (!hit.HadHit() || hit.mHit.mFraction <= 0.0F) {
        return false;
    }
    const RVec3 point = ray.GetPointOnRay(hit.mHit.mFraction);
    const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mHit.mBodyID);
    if (!lock.Succeeded() || !lock.GetBody().IsStatic() ||
        lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mHit.mSubShapeID2, point).GetY() < 0.7F) {
        return false;
    }
    top_y = float(point.GetY());
    return true;
}

void Matron::pipe_take(const PipeGrip grip, const BodyID body, const SubShapeID member, const RVec3 point) {
    const BodyLockRead lock(physics_.GetBodyLockInterface(), body);
    pipe_support_ = body;
    pipe_member_ = member;
    pipe_local_hand_ = lock.Succeeded() ? Vec3(lock.GetBody().GetInverseCenterOfMassTransform() * point) : Vec3(point);
    pipe_hand_ = point;
    pipe_grip_ = grip;
    if (grip != PipeGrip::Face) {
        pipe_hand2_held_ = false;   // the other hand holds the face only
    }
}

bool Matron::pipe_top_hold(const RVec3 rail, const Vec3 across, const RVec3 at, const PipeGrip grip) {
    // Along the edge's top, level with her shoulders.
    const Vec3 run(-across.GetZ(), 0.0F, across.GetX());
    for (const float s : {0.0F, kPipeShoulderSideM, -kPipeShoulderSideM}) {
        BodyID body;
        SubShapeID member;
        RVec3 point;
        if (pipe_top_under(rail + run * s, across, body, member, point) && pipe_in_hand_reach(at, point)) {
            pipe_take(grip, body, member, point);
            return true;
        }
    }
    return false;
}

bool Matron::pipe_rail_hold(const float deck, const RVec3 at) {
    // The ref's rail hold: the rail's top under her hands, taken when it is
    // within her climb reach (kClimbReachM from her middle), not her shoulder
    // reach — the rail stands out from her line by more than a shoulder's
    // length, and the tight reach starved the dismount at the west pipe.
    if (pipe_route_ < 0) {
        return false;
    }
    const RiserRoute &r = riser_routes_[static_cast<std::size_t>(pipe_route_)];
    const RiserDeck &d = std::abs(r.lo.floor_y - deck) < std::abs(r.hi.floor_y - deck) ? r.lo : r.hi;
    const ClimbBodyFilter filter(body_, player_);
    const float top = d.rail.GetY();
    Vec3 across = Vec3(riser_line(r, kRiserGaps[2]).GetX() - d.stand.GetX(), 0.0F,
                       riser_line(r, kRiserGaps[2]).GetZ() - d.stand.GetZ());
    across = across.LengthSq() > 1.0e-6F ? across.Normalized() : r.out;
    for (const float dz : {0.0F, -0.03F, 0.03F, -0.06F}) {
        const Vec3 over = d.rail + across * dz;
        const RRayCast ray(RVec3(over.GetX(), top + 0.15F, over.GetZ()), Vec3(0.0F, -0.3F, 0.0F));
        RayCastSettings settings;
        settings.mTreatConvexAsSolid = false;
        ClosestHitCollisionCollector<CastRayCollector> hit;
        physics_.GetNarrowPhaseQuery().CastRay(ray, settings, hit, {}, {}, filter);
        if (!hit.HadHit()) {
            continue;
        }
        const RVec3 point = ray.GetPointOnRay(hit.mHit.mFraction);
        if (Vec3(point - at).Length() > kClimbReachM) {
            continue;
        }
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mHit.mBodyID);
        if (!lock.Succeeded() ||
            lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mHit.mSubShapeID2, point).GetY() < 0.7F) {
            continue;
        }
        BodyID rests_body;
        SubShapeID rests_member;
        if (!hand_rests_on(physics_.GetNarrowPhaseQuery(), point, filter, rests_body, rests_member) ||
            rests_body != hit.mHit.mBodyID) {
            continue;
        }
        pipe_support_ = rests_body;
        pipe_member_ = rests_member;
        pipe_local_hand_ = Vec3(lock.GetBody().GetInverseCenterOfMassTransform() * point);
        pipe_hand_ = point;
        pipe_hand2_held_ = false;
        pipe_grip_ = PipeGrip::Rail;   // the stepped mount/dismount read the grip; the ref's smooth phases do not
        return true;
    }
    return false;
}

bool Matron::pipe_lip_hold(const float deck, const RVec3 at) {
    if (pipe_route_ < 0) {
        return false;
    }
    const RiserRoute &r = riser_routes_[static_cast<std::size_t>(pipe_route_)];
    const RiserDeck &d = std::abs(r.lo.floor_y - deck) < std::abs(r.hi.floor_y - deck) ? r.lo : r.hi;
    Vec3 across = Vec3(riser_line(r, kRiserGaps[2]).GetX() - d.stand.GetX(), 0.0F,
                       riser_line(r, kRiserGaps[2]).GetZ() - d.stand.GetZ());
    across = across.LengthSq() > 1.0e-6F ? across.Normalized() : r.out;
    // The deck's edge beam, under its edge: the rail's point moved toward
    // the line by a hand's breadth, its top under the deck.
    const RVec3 lip(d.rail.GetX() + across.GetX() * kPipeLipInFromRailM, d.floor_y - kPipeLipUnderDeckM,
                    d.rail.GetZ() + across.GetZ() * kPipeLipInFromRailM);
    return pipe_top_hold(lip, across, at, PipeGrip::Lip);
}

// Her feet on the edge's top: under her soles' span, as her feet stand on a
// floor (sole_contact: from her middle, kStandCentre + 0.3 m down at most).
bool Matron::pipe_feet_hold(const float deck, const RVec3 at) {
    if (pipe_route_ < 0) {
        return false;
    }
    const RiserRoute &r = riser_routes_[static_cast<std::size_t>(pipe_route_)];
    const RiserDeck &d = std::abs(r.lo.floor_y - deck) < std::abs(r.hi.floor_y - deck) ? r.lo : r.hi;
    Vec3 across = Vec3(riser_line(r, kRiserGaps[2]).GetX() - d.stand.GetX(), 0.0F,
                       riser_line(r, kRiserGaps[2]).GetZ() - d.stand.GetZ());
    across = across.LengthSq() > 1.0e-6F ? across.Normalized() : r.out;
    BodyID body;
    SubShapeID member;
    RVec3 point;
    if (!pipe_top_under(d.rail, across, body, member, point)) {
        return false;
    }
    const float below = float(at.GetY() - point.GetY());
    if (distance_xz(float(at.GetX()), float(at.GetZ()), float(point.GetX()), float(point.GetZ())) > kPipeFootSpanM ||
        below < kStandCentre - 0.1F || below > kStandCentre + 0.3F) {
        return false;
    }
    pipe_take(PipeGrip::Feet, body, member, point);
    return true;
}

bool Matron::pipe_in_hand_reach(const RVec3 at, const RVec3 point) const noexcept {
    const Vec3 ahead(std::sin(yaw_), 0.0F, std::cos(yaw_));
    const Vec3 side(ahead.GetZ(), 0.0F, -ahead.GetX());
    const RVec3 middle = at + Vec3(0.0F, kPipeShoulderUpM - kPipeTuckDropM * pipe_tuck_drawn_, 0.0F) +
                         ahead * (kPipeShoulderAheadM + kPipeTuckAheadM * pipe_tuck_drawn_);
    for (const float s : {1.0F, -1.0F}) {
        if (Vec3(point - (middle + side * (s * kPipeShoulderSideM))).Length() <= kPipeHandReachM) {
            return true;
        }
    }
    return false;
}

bool Matron::pipe_grip_in_reach(const RVec3 at) const {
    switch (pipe_grip_) {
    case PipeGrip::Rail:
    case PipeGrip::Face:
    case PipeGrip::Lip:
        return pipe_in_hand_reach(at, pipe_hand_);
    case PipeGrip::Feet: {
        const float below = float(at.GetY() - pipe_hand_.GetY());
        return distance_xz(float(at.GetX()), float(at.GetZ()), float(pipe_hand_.GetX()), float(pipe_hand_.GetZ())) <=
                   kPipeFootSpanM + 0.01F &&
               below >= kStandCentre - 0.1F && below <= kStandCentre + 0.3F;
    }
    case PipeGrip::Legs:
        return true;
    case PipeGrip::None:
    default:
        return false;
    }
}

// Both hands onto the riser's face from her line at height y: the one she
// leads with (her grip, on the mover's account) and the other beside and
// below it. False (her hands as they were) when the face is not in her real
// arm's reach.
bool Matron::pipe_face_hold(const RVec3 at, const float y) {
    if (pipe_route_ < 0 || static_cast<std::size_t>(pipe_route_) >= riser_routes().size()) {
        return false;
    }
    RiserHold lead;
    if (!riser_face_probe(at, y, kRiserHandSideM, lead) || !pipe_in_hand_reach(at, lead.point)) {
        return false;
    }
    pipe_take(PipeGrip::Face, lead.body, lead.member, lead.point);
    RiserHold other;
    pipe_hand2_held_ = riser_face_probe(at, y - 0.3F, -kRiserHandSideM, other);
    if (pipe_hand2_held_) {
        pipe_hand2_ = other;
    }
    return true;
}

// Her hands on the edge where she stands (or, from a floor her line stands
// on, on the riser), and room for all of her over it and out at her line;
// else nothing starts and she tries again in a moment.
bool Matron::get_on_pipe(const float from_floor, const float to_floor) {
    if (climb_retry_s_ > 0.0F || pipe_route_ < 0 || static_cast<std::size_t>(pipe_route_) >= riser_routes().size()) {
        return false;
    }
    const RiserRoute &r = riser_routes_[static_cast<std::size_t>(pipe_route_)];
    const RiserDeck &from = to_floor > from_floor ? r.lo : r.hi;
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    bool hands = false;
    if (from.line_on_floor) {
        hands = distance_xz(float(at.GetX()), float(at.GetZ()), from.stand.GetX(), from.stand.GetZ()) < 0.35F &&
                pipe_face_hold(at, float(at.GetY()) + 0.6F);
    } else {
        const float over = from.rail.GetY() + kPipePerchM + kStandCentre;
        const Vec3 line = riser_line(r, kRiserGaps[2]);
        Vec3 across = Vec3(line.GetX() - from.stand.GetX(), 0.0F, line.GetZ() - from.stand.GetZ());
        across = across.LengthSq() > 1.0e-6F ? across.Normalized() : r.out;
        BodyID rail;
        SubShapeID member;
        RVec3 top;
        hands = shape_fits(shape_, RVec3(from.stand.GetX(), over, from.stand.GetZ())) &&
                shape_fits(shape_, RVec3(line.GetX(), over, line.GetZ())) &&
                pipe_top_under(from.rail, across, rail, member, top);
    }
    if (!hands) {
        pipe_support_ = BodyID();
        pipe_hand2_held_ = false;
        climb_retry_s_ = 2.0F;
        return false;
    }
    // Her hands on the edge's top if it is in their reach; else she steps up
    // to it first (the press).
    pipe_tuck_ = 0.0F;
    pipe_tuck_drawn_ = 0.0F;
    pipe_grip_ = PipeGrip::None;
    pipe_support_ = BodyID();
    if (!from.line_on_floor) {
        (void)pipe_rail_hold(from_floor, at);
    }
    drop_stock();
    pipe_from_ = from_floor;
    pipe_to_ = to_floor;
    pipe_phase_ = from.line_on_floor ? PipePhase::Climb : PipePhase::Mount;
    pipe_step_ = PipeStep::Press;
    pipe_failed_ = false;
    pipe_s_ = 0.0F;
    pipe_hold_m_ = 0.0F;
    pipe_best_ = -std::numeric_limits<float>::max();
    pipe_stall_s_ = 0.0F;
    pipe_overload_s_ = 0.0F;
    ++pipe_links_;
    ++pipe_holds_;   // her hands on the steel: a knock
    return true;
}

void Matron::get_off_pipe(const bool failed) {
    pipe_phase_ = PipePhase::Off;
    pipe_support_ = BodyID();
    pipe_grip_ = PipeGrip::None;
    pipe_tuck_ = 0.0F;
    pipe_tuck_drawn_ = 0.0F;
    pipe_hand2_held_ = false;
    pipe_drive_dv_ = Vec3::sZero();
    pipe_force_n_ = 0.0F;
    pipe_failed_ = failed;
    pipe_swat_body_ = BodyID();
    pipe_slide_mps_ = 0.0F;
    pipe_stun_s_ = 0.0F;
    pipe_overload_s_ = 0.0F;
    pipe_stall_s_ = 0.0F;
    const int way = pipe_way(pipe_from_, pipe_to_);
    PipeWayMemory *memory = pipe_way_memory(way);
    if (failed) {
        // This way of the riser she leaves alone a while (past its retries,
        // until the world changes). Her level is what her feet find
        // (settle_level), not where she meant to go.
        if (memory != nullptr) {
            if (memory->failures < 255) {
                ++memory->failures;
            }
            memory->blocked_until_s = memory->failures > kPipeWayRetries ? std::numeric_limits<float>::infinity()
                                                                         : clock_s_ + kPipeWayBlockS;
        }
        ++climb_releases_;
        climb_retry_s_ = 2.0F;
        return;
    }
    // Along it: this way of the riser carried her, and is worth trying again.
    if (memory != nullptr) {
        memory->failures = 0;
        memory->blocked_until_s = 0.0F;
    }
    for (std::size_t i = 0; i < levels_.size(); ++i) {
        if (std::abs(levels_[i].floor_y - pipe_to_) < 0.3F) {
            level_ = static_cast<int>(i);
        }
    }
    feet_y_ = pipe_to_;
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    pick_patrol(float(at.GetX()), float(at.GetZ()));
}

void Matron::tear_off_pipe() {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    const float feet = float(at.GetY()) - kStandCentre;
    // The steel under her legs: straight down beside the riser's face, the
    // first top met (a flange, a deck, its foot).
    pipe_stop_y_ = -std::numeric_limits<float>::max();
    if (pipe_route_ >= 0 && static_cast<std::size_t>(pipe_route_) < riser_routes_.size()) {
        const RiserRoute &r = riser_routes_[static_cast<std::size_t>(pipe_route_)];
        const Vec3 beside = Vec3(r.cx, 0.0F, r.cz) + r.out * (r.face + 0.1F) +
                            r.along * std::clamp(r.lateral, -r.face_half + 0.05F, r.face_half - 0.05F);
        const ClimbBodyFilter filter(body_, player_);
        float top = 0.0F;
        if (top_under(physics_.GetNarrowPhaseQuery(), filter, beside.GetX(), feet + 0.05F, beside.GetZ(),
                      std::max(1.0F, feet - r.bottom + 0.5F), top)) {
            pipe_stop_y_ = top;
        }
    }
    pipe_slide_mps_ = std::max(0.0F, -bodies.GetLinearVelocity(body_).GetY());
    pipe_support_ = BodyID();
    pipe_grip_ = PipeGrip::None;
    pipe_tuck_ = 0.0F;
    pipe_tuck_drawn_ = 0.0F;
    pipe_hand2_held_ = false;
    pipe_swat_body_ = BodyID();
    pipe_best_ = -std::numeric_limits<float>::max();
    pipe_stall_s_ = 0.0F;
    pipe_overload_s_ = 0.0F;
    pipe_phase_ = PipePhase::Slide;
}

void Matron::ride_pipe(const float dt) {
    if (pipe_route_ < 0 || static_cast<std::size_t>(pipe_route_) >= riser_routes().size()) {
        get_off_pipe(true);
        return;
    }
    const RiserRoute &r = riser_routes_[static_cast<std::size_t>(pipe_route_)];
    BodyInterface &bodies = physics_.GetBodyInterface();
    const NarrowPhaseQuery &query = physics_.GetNarrowPhaseQuery();
    const RVec3 at = bodies.GetPosition(body_);
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const float feet = float(at.GetY()) - kStandCentre;
    const Vec3 v = bodies.GetLinearVelocity(body_);
    const ClimbBodyFilter filter(body_, player_);
    const bool down = pipe_to_ < pipe_from_;
    const RiserDeck &from = down ? r.hi : r.lo;
    const RiserDeck &to = down ? r.lo : r.hi;
    const float from_rail = from.rail.GetY();
    const float to_rail = to.rail.GetY();
    const bool on_line = on_riser_line(r, ax, az);
    // Her line this tick: her hips as close to the face as there is room
    // for all of her, here and ahead of her along it.
    // (Ahead, no lower than where she leaves it: the floor or the edge she
    // gets off onto.)
    const float lowest = (down ? (to.line_on_floor ? to.floor_y + 0.05F : to_rail + kPipeOverRailM)
                               : (from.line_on_floor ? from.floor_y + 0.05F : from_rail + kPipeOverRailM)) + kStandCentre;
    const float ahead_y = std::max(lowest, float(at.GetY()) + (down ? -1.0F : 1.0F) * std::max(0.6F, std::abs(v.GetY()) * 0.6F));
    const Vec3 line_gap = riser_line_at(r, std::max(lowest, float(at.GetY())), ahead_y - std::max(lowest, float(at.GetY())));
    pipe_gap_ = line_gap.GetY();
    const Vec3 line(line_gap.GetX(), 0.0F, line_gap.GetZ());
    const Vec3 line_ref = riser_line(r, kRiserGaps[2]);
    // Her way from her stand to her line: level, and the edge's top along it.
    Vec3 across = line_ref - Vec3(from.stand.GetX(), 0.0F, from.stand.GetZ());
    across.SetY(0.0F);
    across = across.LengthSq() > 1.0e-6F ? across.Normalized() : r.out;
    const float run = (Vec3(to.rail) - line_ref).Dot(across);   // the edge's top, out from her line
    const Vec3 hang_point = line_ref + across * (0.25F * run);   // hanging off it, a quarter of the way out
    const Vec3 out_point = line_ref + across * (0.52F * run);    // out over it, halfway out
    pipe_s_ += dt;
    speed_ = 0.0F;
    stride_ += v.Length() * dt;
    // Each kPipeHoldM her hands go a knock (the rail's too); each flange passed a bong.
    pipe_hold_m_ += v.Length() * dt;
    while (pipe_hold_m_ >= kPipeHoldM) {
        pipe_hold_m_ -= kPipeHoldM;
        ++pipe_holds_;
    }
    const float was_feet = feet - v.GetY() * dt;
    for (const float top : r.collars) {
        if ((was_feet - top) * (feet - top) < 0.0F) {
            ++pipe_flanges_;
        }
    }
    // Her one budget on the pipe this tick: her hands' (or legs') force on
    // the steel, and any swat of her arm, all drawn from it.
    const traversal::Limits law{};
    traversal::Budget budget{law.total_force_n, 0.0F, law.power_w, 0.0F, v};
    Vec3 on_her = Vec3::sZero();
    const auto report = [&](const Vec3 force, const Vec3 against) {
        on_her += force;
        pipe_force_n_ = budget.used;
        pipe_peak_force_n_ = std::max(pipe_peak_force_n_, pipe_force_n_);
        pipe_peak_power_w_ = std::max(pipe_peak_power_w_, std::abs(force.Dot(against)));
        pipe_drive_dv_ = (on_her / kMassKg + physics_.GetGravity()) * dt;
    };
    const auto apply = [&](const Vec3 wanted_force) {
        const bool support = !pipe_support_.IsInvalid() && bodies.IsAdded(pipe_support_);
        budget.velocity = v - (support ? bodies.GetPointVelocity(pipe_support_, pipe_hand_) : Vec3::sZero());
        const Vec3 force = budget.take(wanted_force);
        bodies.AddForce(body_, force);
        if (support && bodies.GetMotionType(pipe_support_) == EMotionType::Dynamic) {
            bodies.AddForce(pipe_support_, -force, pipe_hand_);
        }
        report(force, budget.velocity);
    };
    // Her hands gone: on the riser her legs still hold it; at an edge, down.
    const auto lost = [&] {
        if (on_line && (pipe_phase_ != PipePhase::Mount || pipe_step_ == PipeStep::Catch)) {
            tear_off_pipe();
        } else {
            get_off_pipe(true);
        }
    };
    const Vec3 beside = Vec3(r.cx, 0.0F, r.cz) + r.out * (r.face + 0.1F) +
                        r.along * std::clamp(r.lateral, -r.face_half + 0.05F, r.face_half - 0.05F);
    if (pipe_phase_ == PipePhase::Slide) {
        if (feet <= pipe_stop_y_ + 0.02F) {
            // Her legs onto the flange: the slam, and it holds her.
            pipe_slam_mps_ = std::max(0.0F, -v.GetY());
            pipe_stun_s_ = kPipeStunSeconds + kPipeStunPerMps * std::max(0.0F, pipe_slam_mps_ - kPipeStunFromMps);
            pipe_slide_mps_ = 0.0F;
            ++pipe_slams_;
            ++knockdowns_;
            rage_ = std::min(1.0F, rage_ + kKnockdownRage);
            BodyID flange;
            SubShapeID member;
            const RVec3 legs(beside.GetX(), pipe_stop_y_, beside.GetZ());
            if (!hand_rests_on(query, legs, filter, flange, member)) {
                get_off_pipe(true);
                return;
            }
            const BodyLockRead lock(physics_.GetBodyLockInterface(), flange);
            if (!lock.Succeeded()) {
                get_off_pipe(true);
                return;
            }
            pipe_support_ = flange;
            pipe_member_ = member;
            pipe_local_hand_ = Vec3(lock.GetBody().GetInverseCenterOfMassTransform() * legs);
            pipe_hand_ = legs;
            pipe_grip_ = PipeGrip::Legs;
            pipe_phase_ = PipePhase::Stunned;
            pipe_stall_s_ = 0.0F;
            return;
        }
        // Her squeeze, less than her weight, and her legs round to her line.
        Vec3 pull = kMassKg * (Vec3(line.GetX() - ax, 0.0F, line.GetZ() - az) * 3.0F - Vec3(v.GetX(), 0.0F, v.GetZ())) * 7.0F;
        if (pull.Length() > kPipeLegsLineN) {
            pull *= kPipeLegsLineN / pull.Length();
        }
        const float slide_progress = -feet;
        if (slide_progress > pipe_best_ + 0.04F) {
            pipe_best_ = slide_progress;
            pipe_stall_s_ = 0.0F;
        } else {
            pipe_stall_s_ += dt;
        }
        if (!on_line || pipe_stall_s_ > kPipeStallS) {
            get_off_pipe(true);
            return;
        }
        pipe_slide_mps_ = std::max(0.0F, -v.GetY());
        apply(pull + Vec3(0.0F, kMassKg * (-physics_.GetGravity().GetY() - kPipeSlideAccel), 0.0F));
        return;
    }
    // What has her: still on its steel, the very part of it, and in the reach
    // of the hand or the feet that hold it (rule 37: her real arm, her soles'
    // span). Out of reach it is let go, and the step below takes what is in
    // her reach now, or she is off the pipe: never a hold past her reach.
    if (pipe_grip_ != PipeGrip::None) {
        bool held = !pipe_support_.IsInvalid() && bodies.IsAdded(pipe_support_);
        if (held) {
            pipe_hand_ = bodies.GetCenterOfMassTransform(pipe_support_) * pipe_local_hand_;
            if (pipe_grip_ == PipeGrip::Face) {
                RiserHold lead;
                lead.body = pipe_support_;
                lead.member = pipe_member_;
                lead.local = pipe_local_hand_;
                held = riser_hold_kept(lead, at);
            } else {
                BodyID body;
                SubShapeID member;
                held = hand_rests_on(query, pipe_hand_, filter, body, member) && body == pipe_support_ &&
                       member == pipe_member_ && pipe_grip_in_reach(at);
            }
        }
        if (!held) {
            pipe_grip_ = PipeGrip::None;
            pipe_support_ = BodyID();
            pipe_let_go_y_ = float(at.GetY());
        }
    }
    // Her other hand: still on the face, or off it (taken again below).
    if (pipe_hand2_held_ && (pipe_grip_ != PipeGrip::Face || !riser_hold_kept(pipe_hand2_, at))) {
        pipe_hand2_held_ = false;
    }
    float pace = 0.4F;   // gone frail on it: slow
    if (stage_ == Stage::Changed) {
        const bool burst = std::fmod(pipe_s_, kTurnCycleSeconds) < kTurnBurstSeconds;
        pace = burst ? (down ? kPipeChangedDownBurstMps : kPipeChangedUpBurstMps) : kPipeChangedPauseMps;
    } else if (stage_ == Stage::Dangerous) {
        pace = down ? kPipeDownMps : kPipeUpMps;
    }
    // The edge she crosses (Mount: the deck she leaves; else the deck she
    // goes to), her middle perched over it, and her tuck as she rises over it.
    const float deck = pipe_phase_ == PipePhase::Mount ? pipe_from_ : pipe_to_;
    const RVec3 &rail_point = pipe_phase_ == PipePhase::Mount ? from.rail : to.rail;
    const float rail = float(rail_point.GetY());
    const float perch_y = rail + kStandCentre + kPipePerchM;
    // Up over the edge, her middle where her feet are: hanging off it while
    // still under it, out over it once clear of the edge beam.
    const Vec3 rise_point = feet < deck - kPipeLipUnderDeckM + 0.05F ? hang_point : out_point;
    const auto tuck_for = [&](const float y) { return std::clamp(y - rail - kPipeTuckFromM, 0.0F, 1.0F); };
    const float shoulder_y = float(at.GetY()) + kPipeShoulderUpM - kPipeTuckDropM * pipe_tuck_drawn_;
    // The riser's face level with her shoulders, a little under or over them.
    const auto face_near = [&] {
        return pipe_face_hold(at, shoulder_y) || pipe_face_hold(at, shoulder_y - 0.3F) ||
               pipe_face_hold(at, shoulder_y + 0.3F);
    };
    // The riser's standing face level with her shins, in her legs' reach (her
    // brace on it: matron_contact.cpp).
    const auto riser_at_shins = [&] {
        const RVec3 shins(at.GetX(), at.GetY() - kStandCentre + kBraceShinM, at.GetZ());
        const RRayCast ray(shins, into_riser(shins, r.cx, r.cz) * kBraceReachM);
        RayCastSettings settings;
        settings.mTreatConvexAsSolid = false;
        ClosestHitCollisionCollector<CastRayCollector> hit;
        query.CastRay(ray, settings, hit, {}, {}, filter);
        if (!hit.HadHit()) {
            return false;
        }
        const RVec3 point = ray.GetPointOnRay(hit.mHit.mFraction);
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mHit.mBodyID);
        return lock.Succeeded() && lock.GetBody().IsStatic() &&
               std::abs(lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mHit.mSubShapeID2, point).GetY()) < 0.3F;
    };
    const auto yaw_off = [&](const float want) { return std::abs(std::remainder(want - yaw_, 2.0F * kPiF)); };
    const auto next_step = [&](const PipeStep step) {
        pipe_step_ = step;
        pipe_best_ = -std::numeric_limits<float>::max();
        pipe_stall_s_ = 0.0F;
    };
    // Facing the shaft from her stand (out to her line), and facing a deck
    // from her line (back to its stand): the route's own directions.
    const float kToShaft = std::atan2(line_ref.GetX() - from.stand.GetX(), line_ref.GetZ() - from.stand.GetZ());
    const float kToDeck = std::atan2(to.stand.GetX() - line_ref.GetX(), to.stand.GetZ() - line_ref.GetZ());
    const float to_riser = std::atan2(r.cx - ax, r.cz - az);
    Vec3 target(at);
    float rate = kMantleMps;
    float progress = 0.0F;
    float facing = to_riser;
    float tuck_want = 0.0F;
    switch (pipe_phase_) {
    case PipePhase::Mount:
        switch (pipe_step_) {
        case PipeStep::Press:
            // Up beside the rail on her hands, her legs drawn up under her so
            // her shoulders stay over it (a squat vault). Not yet on it, she
            // steps up to it on her feet, turned to it.
            if (pipe_grip_ == PipeGrip::None && !pipe_rail_hold(deck, at)) {
                walk_toward(dt, from.stand.GetX(), from.stand.GetZ(), 1.0F, kFootAccel);
                yaw_ = turn_to(yaw_, kToShaft, kTurnRate, dt);
                const float near = -distance_xz(ax, az, from.stand.GetX(), from.stand.GetZ()) - yaw_off(kToShaft);
                if (near > pipe_best_ + 0.04F) {
                    pipe_best_ = near;
                    pipe_stall_s_ = 0.0F;
                } else if ((pipe_stall_s_ += dt) > kPipeStallS) {
                    lost();
                    return;
                }
                pipe_force_n_ = 0.0F;
                return;
            }
            target = Vec3(from.stand.GetX(), perch_y, from.stand.GetZ());
            tuck_want = tuck_for(float(at.GetY()) + v.GetY() * kPipeTuckLeadS);
            facing = kToShaft;
            progress = feet;
            rate = kPipePressMps;
            if (feet > rail + 0.5F * kPipePerchM) {
                next_step(PipeStep::Over);
            }
            break;
        case PipeStep::Over:
            // Over its top, tucked; her feet onto it once it is under her soles.
            if (pipe_grip_ != PipeGrip::Feet) {
                (void)pipe_feet_hold(deck, at);
            }
            if (pipe_grip_ == PipeGrip::None && !pipe_rail_hold(deck, at)) {
                lost();
                return;
            }
            target = Vec3(rail_point.GetX(), perch_y, rail_point.GetZ());
            tuck_want = 1.0F;
            facing = kToShaft;
            progress = -std::abs((Vec3(at) - Vec3(rail_point)).Dot(across));
            if (std::abs((Vec3(at) - Vec3(rail_point)).Dot(across)) <= 0.04F && pipe_grip_ == PipeGrip::Feet) {
                next_step(PipeStep::Perch);
            }
            break;
        case PipeStep::Perch:
            // Perched on it, turned to the riser.
            if (pipe_grip_ == PipeGrip::None && !pipe_feet_hold(deck, at)) {
                lost();
                return;
            }
            target = Vec3(rail_point.GetX(), perch_y, rail_point.GetZ());
            tuck_want = 1.0F;
            progress = -yaw_off(to_riser);
            if (yaw_off(to_riser) < 0.15F) {
                next_step(PipeStep::StepOff);
            }
            break;
        case PipeStep::StepOff:
            // Off it toward her line; the riser taken as soon as it is in her
            // reach, else she lets go and falls to it (Catch).
            if (face_near()) {
                pipe_phase_ = PipePhase::Climb;
                next_step(PipeStep::Press);
                return;
            }
            if (pipe_grip_ == PipeGrip::None) {
                next_step(PipeStep::Catch);
                return;
            }
            // Where the riser ends below her (its top found straight down its
            // axis) she falls to it: off the rail just fast enough to be at
            // her line when she has fallen to its top, else at kPipeStepOffMps.
            target = Vec3(line.GetX(), perch_y, line.GetZ());
            rate = kPipeStepOffMps;
            if (float riser_top = 0.0F; pipe_riser_top_under(at, riser_top)) {
                const float fall = std::max(0.1F, float(at.GetY()) - (riser_top + kPipeCatchOverTopM));
                const float out = std::max(0.0F, distance_xz(ax, az, line.GetX(), line.GetZ()) - kPipeFootSpanM);
                rate = std::clamp(out / std::sqrt(2.0F * fall / 9.81F), 0.2F, kPipeStepOffMps);
            }
            tuck_want = 1.0F;
            progress = -distance_xz(ax, az, line.GetX(), line.GetZ());
            break;
        case PipeStep::Catch:
        default:
            // Falling: the riser taken once her hand reaches it and her legs
            // can wrap it (its face level with her shins, as on it).
            if (riser_at_shins() && face_near()) {
                pipe_phase_ = PipePhase::Climb;
                next_step(PipeStep::Press);
                return;
            }
            if (pipe_let_go_y_ - float(at.GetY()) > kPipeCatchFallM) {
                lost();
                return;
            }
            tuck_want = 1.0F;
            progress = pipe_let_go_y_ - float(at.GetY());
            break;
        }
        break;
    case PipePhase::Climb: {
        rate = pace;
        // Off her line (just caught from a rail), she keeps her tuck till her
        // hold has drawn her in to it.
        const bool off_line = distance_xz(ax, az, line.GetX(), line.GetZ()) > 0.15F;
        if (off_line) {
            tuck_want = pipe_tuck_;
        }
        if (pipe_grip_ == PipeGrip::None && !face_near()) {
            lost();
            return;
        }
        // Near the deck she goes to, turned to it: its rail's top (or, where
        // the riser ends at it, its edge beam) in her reach, she takes it.
        if (Vec3(RVec3(to.rail) - at).Length() < kClimbReachM - 0.1F) {
            facing = kToDeck;
            if (pipe_rail_hold(deck, at) || pipe_lip_hold(deck, at)) {
                pipe_phase_ = PipePhase::Dismount;
                next_step(PipeStep::Hang);
                return;
            }
        }
        if (!down) {
            // Hand over hand: with this hold down at her chest (pulled up to
            // within a few centimetres of it, not crawling the last of the
            // way), the next over it, in her reach.
            // (Just caught, still tucked, her shoulders low: the face over
            // them, else level with them, if that is higher than her hold.)
            if (pipe_hand_.GetY() - at.GetY() < kPipeRegripUpM) {
                const RVec3 kept = pipe_hand_;
                if (!pipe_face_hold(at, float(at.GetY()) + kPipeNextHoldUpM) &&
                    !pipe_face_hold(at, float(at.GetY()) + kPipeNextHoldLowM) &&
                    shoulder_y + 0.3F > float(kept.GetY()) + 0.05F) {
                    (void)(pipe_face_hold(at, shoulder_y + 0.3F) ||
                           (shoulder_y > float(kept.GetY()) + 0.05F && pipe_face_hold(at, shoulder_y)));
                }
            }
            // Always pulling for the hand at her shoulder, so each stroke runs at
            // her pace to the next hold rather than easing into a stop. Her drawn
            // shoulders, not upright ones: just caught, tucked, they sit under her
            // middle, and a pull for upright shoulders at the hold would draw her
            // down the riser till her stall ran out.
            const float shoulder_up = shoulder_y - float(at.GetY());
            target = Vec3(line.GetX(), float(pipe_hand_.GetY()) - shoulder_up + (kPipeShoulderUpM - 0.30F), line.GetZ());
            progress = float(at.GetY());
        } else {
            // Down onto a floor her line stands on: her feet on it, and off.
            if (to.line_on_floor && feet <= to.floor_y + 0.08F) {
                RayCastResult floor;
                if (query.CastRay(RRayCast(at, Vec3(0.0F, -(kStandCentre + 0.3F), 0.0F)), floor, {}, {}, filter)) {
                    get_off_pipe();
                    pipe_listen_s_ = kPipeListenSeconds;
                    return;
                }
            }
            // Down it at her pace, her grip sliding with her, past the deck's
            // rail till it is in her reach.
            (void)pipe_face_hold(at, float(at.GetY()) + 0.3F);
            // (no more than a metre under her at a time, so her pull in to
            // her line is not lost in her way down)
            target = Vec3(line.GetX(), std::max(to_rail - 0.6F, float(at.GetY()) - (off_line ? 0.2F : 1.0F)), line.GetZ());
            progress = -float(at.GetY());
        }
        break;
    }
    case PipePhase::Dismount:
        switch (pipe_step_) {
        case PipeStep::Hang:
            // Hanging off the rail (or the edge beam, till the rail is in her
            // reach) beside the deck's edge, turned to it.
            if (pipe_grip_ != PipeGrip::Rail) {
                (void)pipe_rail_hold(deck, at);
            }
            if (pipe_grip_ == PipeGrip::None && !pipe_lip_hold(deck, at)) {
                lost();
                return;
            }
            facing = kToDeck;
            rate = kClimbMps;
            if (pipe_grip_ == PipeGrip::Lip) {
                target = Vec3(hang_point.GetX(), rail - 0.70F, hang_point.GetZ());
                tuck_want = 0.3F;
            } else {
                target = Vec3(hang_point.GetX(), float(at.GetY()), hang_point.GetZ());
                tuck_want = tuck_for(float(at.GetY()) + v.GetY() * kPipeTuckLeadS);
            }
            progress = -Vec3(target - Vec3(at)).Length();
            if (pipe_grip_ == PipeGrip::Rail && std::abs((Vec3(at) - hang_point).Dot(across)) <= 0.06F) {
                next_step(PipeStep::Rise);
            }
            break;
        case PipeStep::Rise:
            // Up on the rail's top, tucked: clear of the edge beam, then in by
            // the rail, till her soles are over it.
            if (pipe_grip_ == PipeGrip::None && !pipe_rail_hold(deck, at)) {
                lost();
                return;
            }
            target = Vec3(rise_point.GetX(), perch_y, rise_point.GetZ());
            tuck_want = tuck_for(float(at.GetY()) + v.GetY() * kPipeTuckLeadS);
            facing = kToDeck;
            progress = feet;
            rate = kPipePressMps;
            if (feet > rail + 0.5F * kPipePerchM) {
                next_step(PipeStep::Over);
            }
            break;
        case PipeStep::Over:
            if (pipe_grip_ != PipeGrip::Feet) {
                (void)pipe_feet_hold(deck, at);
            }
            if (pipe_grip_ == PipeGrip::None && !pipe_rail_hold(deck, at)) {
                lost();
                return;
            }
            target = Vec3(rail_point.GetX(), perch_y, rail_point.GetZ());
            tuck_want = 1.0F;
            facing = kToDeck;
            progress = -std::abs((Vec3(at) - Vec3(rail_point)).Dot(across));
            if (std::abs((Vec3(at) - Vec3(rail_point)).Dot(across)) <= 0.04F && pipe_grip_ == PipeGrip::Feet) {
                next_step(PipeStep::Perch);
            }
            break;
        case PipeStep::Perch:
            // Perched on it, turned to the shaft to go down off it backwards.
            if (pipe_grip_ == PipeGrip::None && !pipe_feet_hold(deck, at)) {
                lost();
                return;
            }
            target = Vec3(rail_point.GetX(), perch_y, rail_point.GetZ());
            tuck_want = 1.0F;
            facing = kToShaft;
            progress = -yaw_off(kToShaft);
            if (yaw_off(kToShaft) < 0.15F) {
                next_step(PipeStep::Back);
            }
            break;
        case PipeStep::Back:
            // Back off it over the deck, her hands taking the rail as her feet
            // leave it.
            if (pipe_grip_ == PipeGrip::None && !pipe_rail_hold(deck, at)) {
                lost();
                return;
            }
            target = Vec3(to.stand.GetX(), perch_y, to.stand.GetZ());
            rate = kClimbMps;
            tuck_want = 1.0F;
            facing = kToShaft;
            progress = -std::abs((Vec3(at) - Vec3(to.stand)).Dot(across));
            if (std::abs((Vec3(at) - Vec3(to.stand)).Dot(across)) <= 0.05F && pipe_grip_ == PipeGrip::Rail) {
                next_step(PipeStep::Down);
            }
            break;
        case PipeStep::Down:
        default: {
            // Down onto the deck on her hands: done only standing on its steel.
            if (pipe_grip_ == PipeGrip::None && !pipe_rail_hold(deck, at)) {
                lost();
                return;
            }
            target = Vec3(to.stand.GetX(), pipe_to_ + kStandCentre, to.stand.GetZ());
            tuck_want = tuck_for(float(at.GetY()) + v.GetY() * kPipeTuckLeadS);
            rate = kPipePressMps;
            facing = kToShaft;
            progress = -feet;
            RayCastResult floor;
            if (feet <= pipe_to_ + 0.06F &&
                query.CastRay(RRayCast(at, Vec3(0.0F, -(kStandCentre + 0.3F), 0.0F)), floor, {}, {}, filter)) {
                get_off_pipe();
                pipe_listen_s_ = kPipeListenSeconds;
                return;
            }
            break;
        }
        }
        break;
    case PipePhase::Stunned: {
        // Her legs over the flange, still a moment; then her hands again.
        target = Vec3(line.GetX(), pipe_stop_y_ + kStandCentre, line.GetZ());
        rate = kClimbMps;
        progress = 0.0F;
        pipe_stall_s_ = 0.0F;
        pipe_stun_s_ -= dt;
        if (pipe_stun_s_ <= 0.0F) {
            // (Folded over the flange her shoulders are low: the face level
            // with her middle first.)
            if (!pipe_face_hold(at, float(at.GetY())) && !pipe_face_hold(at, float(at.GetY()) + 0.3F)) {
                lost();
                return;
            }
            pipe_phase_ = PipePhase::Climb;   // and on, the way she was going
            pipe_best_ = -std::numeric_limits<float>::max();
            return;
        }
        break;
    }
    default:
        return;
    }
    // Her tuck follows what the step asks, at a joint's pace.
    const float tuck_step = kPipeTuckRate * dt;
    pipe_tuck_ = std::clamp(tuck_want, pipe_tuck_ - tuck_step, pipe_tuck_ + tuck_step);
    pipe_tuck_drawn_ += (pipe_tuck_ - pipe_tuck_drawn_) * std::min(1.0F, dt / kPipeTuckDrawnTauS);
    if (progress > pipe_best_ + 0.04F) {
        pipe_best_ = progress;
        pipe_stall_s_ = 0.0F;
    } else {
        pipe_stall_s_ += dt;
    }
    // Nothing in her reach: no hold to pull on, she falls (gravity alone).
    if (pipe_grip_ == PipeGrip::None) {
        pipe_force_n_ = 0.0F;
        pipe_drive_dv_ = physics_.GetGravity() * dt;
        pipe_overload_s_ = 0.0F;
        if (pipe_stall_s_ > kPipeStallS) {
            lost();
            return;
        }
        yaw_ = turn_to(yaw_, facing, kTurnRate, dt);
        pipe_swat_body_ = BodyID();
        return;
    }
    // One capped force toward where her hands (or her feet, perched on an
    // edge's top) take her, as on her ladders; on the riser's face its hold
    // along the steel is friction.
    const Vec3 support_v = bodies.GetPointVelocity(pipe_support_, pipe_hand_);
    Vec3 wanted = (target - Vec3(at)) * 3.0F;
    if (pipe_phase_ == PipePhase::Climb || pipe_phase_ == PipePhase::Stunned) {
        // Along the riser at her pace; her hips held in to her line beside
        // it on their own (her pace is not spent on them).
        Vec3 level(wanted.GetX(), 0.0F, wanted.GetZ());
        if (level.Length() > kClimbMps) {
            level *= kClimbMps / level.Length();
        }
        wanted = level + Vec3(0.0F, std::clamp(wanted.GetY(), -rate, rate), 0.0F);
    } else if (wanted.Length() > rate) {
        wanted *= rate / wanted.Length();
    }
    Vec3 force = kMassKg * ((wanted - (v - support_v)) * 7.0F - physics_.GetGravity());
    const float friction = kPipeSqueezeN * kPipeFriction;
    if (pipe_grip_ == PipeGrip::Face && std::abs(force.GetY()) > friction) {
        force.SetY(std::copysign(friction, force.GetY()));
    }
    // Her legs (on a flange, or her feet on a rail's top) bear on it, a
    // support, not a grip: her one budget, and they do not open.
    const bool on_legs = pipe_grip_ == PipeGrip::Legs || pipe_grip_ == PipeGrip::Feet;
    const float most = on_legs ? kPipeLegsOnFlangeN : kClimbGripMaxN;
    const float requested = force.Length();
    pipe_overload_s_ = requested > most && !on_legs ? pipe_overload_s_ + dt : 0.0F;
    if (requested > most) {
        force *= most / requested;
    }
    if (pipe_overload_s_ > kPipeOverloadS || pipe_stall_s_ > kPipeStallS) {
        lost();
        return;
    }
    apply(force);
    yaw_ = turn_to(yaw_, facing, kTurnRate, dt);
    // A loose piece lying in her way (a drum come to rest on a flange) she
    // swats out, away from the riser and the deck: her arm's push on it over
    // the ticks it is against her, from what her budget has left after her
    // hold, the equal and opposite push on her. It ends when the piece has
    // what her swat gives (1.6 m/s, at most kPipeSwatNs of momentum) or is
    // out of reach.
    Vec3 away(line_ref.GetX() - from.stand.GetX(), 0.0F, line_ref.GetZ() - from.stand.GetZ());
    away = away.LengthSq() > 1.0e-6F ? away.Normalized() : Vec3::sZero();
    Vec3 swat_level = r.out + away;
    swat_level = swat_level.LengthSq() > 1.0e-6F ? swat_level.Normalized() : r.out;
    const Vec3 swat = (swat_level * 0.99F + Vec3(0.0F, 0.3F, 0.0F)).Normalized();
    bool swatting = false;
    for (std::size_t i = 0; i < near_count_; ++i) {
        const Near &n = near_[i];
        if (!bodies.IsAdded(n.body) || n.mass_kg <= 0.0F) {
            continue;
        }
        const bool on_it = n.body == pipe_swat_body_;
        if (!on_it && n.velocity.Length() > 0.5F) {
            continue;
        }
        const RVec3 there = bodies.GetCenterOfMassPosition(n.body);
        const float along = std::clamp(float(there.GetY() - at.GetY()), -kHalfHeight, kHalfHeight);
        if ((Vec3(there - at) - Vec3(0.0F, along, 0.0F)).Length() > kRadius + n.radius + 0.1F) {
            continue;
        }
        const Vec3 its_v = bodies.GetLinearVelocity(n.body);
        const float gives = std::min(1.6F, kPipeSwatNs / n.mass_kg);
        const float has = its_v.Dot(swat);
        if (has >= gives) {
            continue;
        }
        budget.velocity = its_v - v;
        const Vec3 push = budget.take(swat * (n.mass_kg * (gives - has) / dt));
        if (push.LengthSq() <= 0.0F) {
            break;
        }
        bodies.AddForce(n.body, push, EActivation::Activate);
        bodies.AddForce(body_, -push);
        // J-046 measurement only: the take above put +push on her in the
        // ledger; on her body it is -push (no new force, charged above).
        effort::add(effort::Path::HandsPush, 0.0F, 0.0F, 0.0F, -2.0F * push);
        report(-push, budget.velocity);
        pipe_swat_body_ = n.body;
        swatting = true;
        break;
    }
    if (!swatting) {
        pipe_swat_body_ = BodyID();
    }
}

bool Matron::follow_pipe(const float dt, const RVec3 at) {
    // Frail, she cannot take hold of it.
    if (stage_ == Stage::Pest || pipe_ruled_out_) {
        return false;
    }
    const float floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
    if (std::abs(lead_floor_ - floor_y) <= 0.6F) {
        return false;
    }
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const int region = scanned_ != nullptr ? scanned_->region_at(Vec3(ax, float(at.GetY()) - kStandCentre, az)) : -1;
    // A riser of hers from this deck toward the lead's (a way that failed
    // under her a moment ago she leaves alone a while), the nearest.
    const int route = riser_between(floor_y, lead_floor_, ax, az, region);
    if (route < 0) {
        return false;
    }
    pipe_route_ = route;
    const RiserRoute &r = riser_routes_[static_cast<std::size_t>(route)];
    const bool up = lead_floor_ > floor_y;
    const RiserDeck &from = up ? r.lo : r.hi;
    const RiserDeck &to = up ? r.hi : r.lo;
    const float off = distance_xz(ax, az, from.stand.GetX(), from.stand.GetZ());
    const float accel = turn_ != Turn::None ? kTurnFootAccel : kFootAccel;
    if (off > 0.25F) {
        // To her stand by it along her lines, slowing into it.
        float wx = from.stand.GetX();
        float wz = from.stand.GetZ();
        route_toward(ax, az, from.stand.GetX(), from.stand.GetZ(), false, wx, wz);
        walk_toward(dt, wx, wz, std::min(kHuntMps, 1.0F + 3.0F * off), accel);
        return true;
    }
    // Her hands on the edge: over it and onto the riser (or, her line on
    // this floor, her hands straight onto it).
    climb_retry_s_ = std::max(0.0F, climb_retry_s_ - dt);
    // Her hands on the rail: over it and onto the pipe (matron_pipe.cpp).
    walk_toward(dt, ax, az, 0.0F, accel);
    const Vec3 line = riser_line(r, kRiserGaps[2]);
    yaw_ = from.line_on_floor ? turn_to(yaw_, std::atan2(r.cx - ax, r.cz - az), kTurnRate, dt)
                              : turn_to(yaw_, std::atan2(line.GetX() - from.stand.GetX(), line.GetZ() - from.stand.GetZ()),
                                        kTurnRate, dt);
    const bool tried = climb_retry_s_ <= 0.0F;
    if (!get_on_pipe(from.floor_y, to.floor_y) && tried) {
        // Her hands found no hold there twice: that way she leaves alone a
        // while, rather than trying it again for ever (after J1, fb-bot).
        if (PipeWayMemory *memory = pipe_way_memory(2 * route + (up ? 0 : 1)); memory != nullptr && ++memory->failures >= 2) {
            memory->blocked_until_s = clock_s_ + kPipeWayBlockS;
        }
    }
    return true;
}

int Matron::riser_between(const float from_floor, const float to_floor, const float x, const float z, const int region) {
    // From this floor toward that one (to it, or to a deck between), its way
    // open; the nearest whose stand her map walks her to from the region she
    // is on (-1: any).
    const auto &routes = riser_routes();
    const bool up = to_floor > from_floor;
    std::vector<std::pair<float, int>> near;
    for (std::size_t i = 0; i < routes.size(); ++i) {
        const RiserRoute &r = routes[i];
        const RiserDeck &from = up ? r.lo : r.hi;
        const RiserDeck &to = up ? r.hi : r.lo;
        const bool toward = up ? to.floor_y <= to_floor + 0.6F : to.floor_y >= to_floor - 0.6F;
        if (std::abs(from.floor_y - from_floor) > kOnSteelM || !toward ||
            !pipe_way_open(static_cast<int>(2 * i + (up ? 0 : 1)))) {
            continue;
        }
        near.emplace_back(distance_xz(x, z, from.stand.GetX(), from.stand.GetZ()), static_cast<int>(i));
    }
    std::sort(near.begin(), near.end());
    for (const auto &[d, i] : near) {
        const RiserDeck &from = up ? routes[static_cast<std::size_t>(i)].lo : routes[static_cast<std::size_t>(i)].hi;
        if (region < 0 || scanned_ == nullptr || from.region == region || !scanned_->plan(region, from.region).empty()) {
            return i;
        }
    }
    return -1;
}

int Matron::pipe_way(const float from_floor, const float to_floor) const noexcept {
    if (pipe_route_ < 0 || static_cast<std::size_t>(pipe_route_) >= riser_routes_.size()) {
        return -1;
    }
    return 2 * pipe_route_ + (to_floor < from_floor ? 1 : 0);
}

Matron::PipeWayMemory *Matron::pipe_way_memory(const int way) noexcept {
    if (way < 0 || static_cast<std::size_t>(way / 2) >= riser_routes_.size()) {
        return nullptr;
    }
    return &riser_routes_[static_cast<std::size_t>(way / 2)].ways[way % 2];
}

bool Matron::pipe_way_open(const int way) const noexcept {
    if (pipe_ruled_out_) {
        return false;
    }
    if (way < 0 || static_cast<std::size_t>(way / 2) >= riser_routes_.size()) {
        return true;
    }
    const PipeWayMemory &memory = riser_routes_[static_cast<std::size_t>(way / 2)].ways[way % 2];
    return !memory.ruled_out && clock_s_ >= memory.blocked_until_s;
}

void Matron::pipe_struck(const RVec3 at) {
    if (pipe_route_ >= 0 && static_cast<std::size_t>(pipe_route_) < riser_routes_.size() &&
        on_riser_line(riser_routes_[static_cast<std::size_t>(pipe_route_)], float(at.GetX()), float(at.GetZ()))) {
        tear_off_pipe();
        return;
    }
    // Still at an edge, over the deck: her hands off it, down onto whatever
    // steel her feet find.
    get_off_pipe(true);
    const Vec3 way = blow_impulse_.LengthSq() > 1.0e-4F ? Vec3(blow_impulse_.GetX(), 0.0F, blow_impulse_.GetZ())
                                                       : Vec3::sZero();
    fall_x_ = way.LengthSq() > 1.0e-6F ? way.Normalized().GetX() : std::sin(yaw_);
    fall_z_ = way.LengthSq() > 1.0e-6F ? way.Normalized().GetZ() : std::cos(yaw_);
    knock_down();
}

void Matron::report_pipe() {
    view_.on_pipe = pipe_phase_ != PipePhase::Off;
    view_.pipe_head_down = pipe_phase_ == PipePhase::Climb && stage_ == Stage::Changed && pipe_to_ < pipe_from_;
    view_.pipe_sliding = pipe_phase_ == PipePhase::Slide;
    view_.pipe_peak_force_n = pipe_peak_force_n_;
    view_.pipe_peak_power_w = pipe_peak_power_w_;
    view_.pipe_stunned = pipe_phase_ == PipePhase::Stunned;
    view_.pipe_tuck = pipe_phase_ == PipePhase::Off ? 0.0F : pipe_tuck_;
    view_.pipe_listening = pipe_phase_ == PipePhase::Off && pipe_listen_s_ > 0.0F;
    view_.pipe_links = pipe_links_;
    view_.pipe_holds = pipe_holds_;
    view_.pipe_flanges = pipe_flanges_;
    view_.pipe_slams = pipe_slams_;
    view_.pipe_slam_mps = pipe_slam_mps_;
    view_.pipe_on_face = pipe_phase_ == PipePhase::Climb && pipe_grip_ == PipeGrip::Face &&
                         pipe_support_ != BodyID();
    if (pipe_route_ >= 0 && static_cast<std::size_t>(pipe_route_) < riser_routes_.size()) {
        const RiserRoute &r = riser_routes_[static_cast<std::size_t>(pipe_route_)];
        view_.pipe_axis = Vec3(r.cx, 0.0F, r.cz);
        view_.pipe_half_x = r.hx;
        view_.pipe_half_z = r.hz;
    }
}

void Matron::restore_pipe() noexcept {
    // Off the pipe, whatever had her on it.
    pipe_support_ = BodyID();
    pipe_grip_ = PipeGrip::None;
    pipe_tuck_ = 0.0F;
    pipe_tuck_drawn_ = 0.0F;
    pipe_hand2_held_ = false;
    pipe_swat_body_ = BodyID();
    pipe_drive_dv_ = Vec3::sZero();
    pipe_force_n_ = 0.0F;
    pipe_slide_mps_ = 0.0F;
    pipe_overload_s_ = 0.0F;
    pipe_stall_s_ = 0.0F;
    pipe_phase_ = PipePhase::Off;
    travel_riser_ = false;
    pipe_links_ = 0;
    pipe_holds_ = 0;
    pipe_flanges_ = 0;
    pipe_slams_ = 0;
    pipe_slam_mps_ = 0.0F;
    pipe_stun_s_ = 0.0F;
    pipe_listen_s_ = 0.0F;
}

// A riser as a way of hers in the hunt (matron_mover.hpp): to her stand by
// it and onto it (follow_pipe), then along it (ride_pipe) inside her hunt.
MoverStatus Matron::PipeWay::step(const float dt, traversal::Budget & /*budget*/) {
    if (m_.pipe_phase_ != PipePhase::Off) {
        m_.ride_pipe(dt);
        return MoverStatus::Running;
    }
    if (!m_.follow_pipe(dt, goal_.at)) {
        return MoverStatus::Idle;
    }
    return status() == MoverStatus::Running ? MoverStatus::Running : MoverStatus::Walking;
}

void Matron::PipeWay::cancel() {
    m_.pipe_phase_ = PipePhase::Off;
    m_.pipe_support_ = BodyID();
    m_.pipe_grip_ = PipeGrip::None;
    m_.pipe_tuck_ = 0.0F;
    m_.pipe_tuck_drawn_ = 0.0F;
    m_.pipe_hand2_held_ = false;
    m_.pipe_swat_body_ = BodyID();
    m_.pipe_drive_dv_ = Vec3::sZero();
    m_.pipe_force_n_ = 0.0F;
    m_.pipe_slide_mps_ = 0.0F;
    m_.pipe_stun_s_ = 0.0F;
    m_.pipe_overload_s_ = 0.0F;
    m_.pipe_stall_s_ = 0.0F;
}

MoverStatus Matron::PipeWay::status() const {
    return m_.pipe_phase_ != PipePhase::Off ? MoverStatus::Running : MoverStatus::Idle;
}

MoverView Matron::PipeWay::view() const {
    MoverView v;
    v.tick = MoverTick::Hunt;
    if (m_.pipe_phase_ == PipePhase::Off) {
        return v;
    }
    v.drives = true;
    v.held = true;
    v.lands = true;
    // Her legs sliding, or stunned on a flange: no blow is read. Else her
    // hands hold her against her weight, and a blow is read in all three
    // directions (take_blow).
    v.blow_proof = m_.pipe_phase_ == PipePhase::Slide || m_.pipe_phase_ == PipePhase::Stunned;
    v.floor_in_hands = true;
    v.drive_dv = m_.pipe_drive_dv_;
    v.force_shown = true;
    v.force_n = m_.pipe_force_n_;
    v.has_goal = true;
    v.goal_floor_y = m_.pipe_to_;
    return v;
}

bool Matron::PipeWay::let_go() {
    if (m_.pipe_phase_ == PipePhase::Off || m_.pipe_phase_ == PipePhase::Slide) {
        return false;
    }
    m_.tear_off_pipe();
    return true;
}

} // namespace scraperx::sim
