#include "sim/material_traversal_line.hpp"
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/EmptyShape.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace scraperx::sim {
namespace {
using namespace JPH;
template<class V> bool finite(const V &v) {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ());
}
}
MaterialTraversalLine::MaterialTraversalLine(PhysicsSystem &world, BodyID player, ObjectLayer layer)
    : world_(world), player_(player) {
    // Zero-volume translation-only masses cannot become invisible collision
    // supports. They still carry real inertia, gravity and reciprocal impulses.
    auto &bi = world_.GetBodyInterface();
    for (unsigned i = 0; i < node_count; ++i) {
        BodyCreationSettings settings(new EmptyShape, RVec3::sZero(), Quat::sIdentity(), EMotionType::Dynamic, layer);
        settings.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ;
        settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = node_mass_kg;
        settings.mMotionQuality = EMotionQuality::Discrete;
        settings.mLinearDamping = 0;
        settings.mAngularDamping = 0;
        settings.mAllowSleeping = false;
        Body *created = bi.CreateBody(settings);
        if (!created) {
            for (unsigned j = 0; j < i; ++j) bi.DestroyBody(nodes_[j]);
            throw std::runtime_error("Material traversal line: expected capacity for two 0.25kg material nodes");
        }
        nodes_[i] = created->GetID();
    }
}
MaterialTraversalLine::~MaterialTraversalLine() {
    stop();
    for (auto id : nodes_) world_.GetBodyInterface().DestroyBody(id);
}
Body *MaterialTraversalLine::body(const PortableLinePath::Endpoint &e) const {
    auto *b = world_.GetBodyLockInterfaceNoLock().TryGetBody(e.body);
    return b && b->IsInBroadPhase() && b->IsRigidBody() && !b->IsSensor() &&
        b->GetShape() == e.shape_identity.GetPtr() && finite(e.shape_local_point) ? b : nullptr;
}
bool MaterialTraversalLine::install(const PortableLinePath::Endpoint &a, const PortableLinePath::Endpoint &b, float length) {
    if (installed_ || !body(a) || !body(b) || !std::isfinite(length) || length < .6F || length > 18) return false;
    const RVec3 pa = body(a)->GetWorldTransform() * a.shape_local_point;
    const RVec3 pb = body(b)->GetWorldTransform() * b.shape_local_point;
    const double distance = (pb - pa).Length();
    if (distance < .6 || distance > length) return false;
    auto &bi = world_.GetBodyInterface();
    ends_[0] = a; ends_[element_count] = b; rest_length_ = length; states_ = {};
    for (unsigned i = 0; i < node_count; ++i) {
        const float t = float(i + 1) / element_count;
        const RVec3 at = pa + t * (pb - pa);
        const Vec3 velocity = (1 - t) * body(a)->GetPointVelocity(pa) + t * body(b)->GetPointVelocity(pb);
        bi.SetPositionAndRotation(nodes_[i], at, Quat::sIdentity(), EActivation::DontActivate);
        bi.SetLinearAndAngularVelocity(nodes_[i], velocity, Vec3::sZero());
        bi.AddBody(nodes_[i], EActivation::Activate);
        const auto *node = world_.GetBodyLockInterfaceNoLock().TryGetBody(nodes_[i]);
        ends_[i + 1] = {nodes_[i], node->GetShape(), Vec3::sZero()};
    }
    installed_ = true; broken_ = false;
    if (!connect()) { stop(); return false; }
    ++generation_; return true;
}
bool MaterialTraversalLine::connect() {
    // Three series elements retain the same whole-line stiffness and damping.
    // This is a reduced-order constitutive model, not measured rope material.
    collision_step();
    if (!valid()) return false;
    for (unsigned i = 0; i < element_count; ++i) {
        rows_[i] = new FiniteCableConstraint(rest_length_ / element_count,
            element_count * GrappleRopeSettings::stiffness_n_per_m,
            element_count * GrappleRopeSettings::damping_ns_per_m);
        const auto &p = paths_[i];
        if (rows_[i]->set_geometry(p.participants.data(), p.participant_count, p.length_m) != FiniteCableConstraint::Admission::Accepted) {
            rows_[i] = nullptr; return false;
        }
        world_.AddConstraint(rows_[i]);
    }
    return true;
}
void MaterialTraversalLine::stop() {
    for (auto &row : rows_) if (row) { world_.RemoveConstraint(row); row = nullptr; }
    auto &bi = world_.GetBodyInterface();
    for (auto id : nodes_) if (bi.IsAdded(id)) bi.RemoveBody(id);
    installed_ = broken_ = false; rest_length_ = 0; ends_ = {}; paths_ = {}; states_ = {}; ++generation_;
}
void MaterialTraversalLine::collision_step() {
    if (!installed_) return;
    for (unsigned i = 0; i < element_count; ++i) {
        PortableLinePath::SpanInput span; span.player = player_; span.a = ends_[i]; span.b = ends_[i + 1];
        paths_[i] = PortableLinePath::evaluate_span(world_, span, states_[i]);
        if (!rows_[i]) continue;
        const auto &p = paths_[i];
        if (!p.valid) (void)rows_[i]->set_geometry(nullptr, 0, 0);
        else (void)rows_[i]->set_geometry(p.participants.data(), p.participant_count, p.length_m);
    }
}
bool MaterialTraversalLine::valid() const {
    if (!installed_) return false;
    for (const auto &p : paths_) if (!p.valid) return false;
    return true;
}
bool MaterialTraversalLine::broken() const {
    if (broken_) return true;
    for (const auto &row : rows_) if (row && row->broken()) return true;
    return false;
}
float MaterialTraversalLine::tension_n() const {
    float peak = 0;
    for (const auto &row : rows_) if (row) peak = std::max(peak, row->tension());
    return peak;
}
bool MaterialTraversalLine::sample_material(LineMaterialHandle handle, LineMaterialSample &out) const {
    out = {};
    if (!installed_ || broken() || handle.generation != generation_ || !std::isfinite(handle.coordinate_m) ||
        handle.coordinate_m < 0 || handle.coordinate_m > rest_length_) return false;
    const float fraction = handle.coordinate_m / (rest_length_ / element_count);
    const unsigned i = std::min(element_count - 1, unsigned(fraction));
    // A routed element needs material derivatives along its contact route.
    // Until provided, never grip a chord through the obstruction.
    if (!paths_[i].valid || paths_[i].has_contact) return false;
    auto *a = body(ends_[i]); auto *b = body(ends_[i + 1]);
    if (!a || !b) return false;
    const float alpha = std::clamp(fraction - i, 0.0F, 1.0F);
    const RVec3 pa = a->GetWorldTransform() * ends_[i].shape_local_point;
    const RVec3 pb = b->GetWorldTransform() * ends_[i + 1].shape_local_point;
    out.world_point = pa + alpha * (pb - pa);
    out.point_velocity = (1 - alpha) * a->GetPointVelocity(pa) + alpha * b->GetPointVelocity(pb);
    out.endpoints[0] = {a, ends_[i].shape_local_point, 1 - alpha};
    out.endpoints[1] = {b, ends_[i + 1].shape_local_point, alpha};
    return true;
}
bool MaterialTraversalLine::nearest_material(RVec3Arg at, float reach, LineMaterialHandle &handle, RVec3 &point) const {
    if (!finite(at) || !std::isfinite(reach) || reach < 0 || !valid() || broken()) return false;
    double best = double(reach) * reach; bool found = false;
    for (unsigned i = 0; i < element_count; ++i) {
        if (paths_[i].has_contact) continue;
        const auto *a = body(ends_[i]); const auto *b = body(ends_[i + 1]);
        if (!a || !b) continue;
        const RVec3 pa = a->GetWorldTransform() * ends_[i].shape_local_point;
        const RVec3 pb = b->GetWorldTransform() * ends_[i + 1].shape_local_point;
        const Vec3 delta(pb - pa);
        const float alpha = delta.LengthSq() > 1.e-10F ? std::clamp(Vec3(at - pa).Dot(delta) / delta.LengthSq(), 0.0F, 1.0F) : 0;
        const RVec3 p = pa + alpha * (pb - pa);
        const double d2 = (p - at).LengthSq();
        if (d2 > best) continue;
        best = d2; point = p; handle = {generation_, (float(i) + alpha) * rest_length_ / element_count}; found = true;
    }
    return found;
}
double MaterialTraversalLine::stored_energy_j() const {
    double total = 0;
    const double k = element_count * GrappleRopeSettings::stiffness_n_per_m;
    for (const auto &p : paths_) if (p.valid) {
        const double extension = std::max(0.0, p.length_m - rest_length_ / element_count);
        total += .5 * k * extension * extension;
    }
    return total;
}
double MaterialTraversalLine::kinetic_energy_j() const {
    double total = 0;
    for (auto id : nodes_) {
        const auto *node = world_.GetBodyLockInterfaceNoLock().TryGetBody(id);
        if (node && node->IsInBroadPhase()) total += .5 * node_mass_kg * node->GetLinearVelocity().LengthSq();
    }
    return total;
}
float MaterialTraversalLine::maximum_node_speed_mps() const {
    float peak = 0;
    for (auto id : nodes_) {
        const auto *node = world_.GetBodyLockInterfaceNoLock().TryGetBody(id);
        if (node && node->IsInBroadPhase()) peak = std::max(peak, node->GetLinearVelocity().Length());
    }
    return peak;
}
MaterialTraversalLine::Checkpoint MaterialTraversalLine::capture() const {
    Checkpoint state;
    state.installed = installed_; state.broken = broken(); state.a = ends_[0]; state.b = ends_[element_count];
    state.rest_length_m = rest_length_; state.paths = states_;
    if (installed_) for (unsigned i = 0; i < node_count; ++i) {
        const auto *node = world_.GetBodyLockInterfaceNoLock().TryGetBody(nodes_[i]);
        state.nodes[i] = {node->GetPosition(), node->GetLinearVelocity()};
    }
    return state;
}
bool MaterialTraversalLine::restore(const Checkpoint &state) {
    stop();
    if (!state.installed) return true;
    if (!body(state.a) || !body(state.b) || !std::isfinite(state.rest_length_m) ||
        state.rest_length_m < .6F || state.rest_length_m > 18) return false;
    for (const auto &node : state.nodes) if (!finite(node.position) || !finite(node.velocity)) return false;
    ends_[0] = state.a; ends_[element_count] = state.b; rest_length_ = state.rest_length_m; states_ = state.paths;
    auto &bi = world_.GetBodyInterface();
    for (unsigned i = 0; i < node_count; ++i) {
        bi.SetPositionAndRotation(nodes_[i], state.nodes[i].position, Quat::sIdentity(), EActivation::DontActivate);
        bi.SetLinearAndAngularVelocity(nodes_[i], state.nodes[i].velocity, Vec3::sZero());
        bi.AddBody(nodes_[i], EActivation::Activate);
        const auto *node = world_.GetBodyLockInterfaceNoLock().TryGetBody(nodes_[i]);
        ends_[i + 1] = {nodes_[i], node->GetShape(), Vec3::sZero()};
    }
    installed_ = true; broken_ = state.broken;
    if (!broken_ && !connect()) { stop(); return false; }
    ++generation_; return true;
}
} // namespace scraperx::sim
