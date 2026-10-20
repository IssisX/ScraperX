#pragma once
#include "sim/line_material_source.hpp"
#include "sim/portable_line_path.hpp"
#include <Jolt/Physics/PhysicsSystem.h>

namespace scraperx::sim {
// Reduced-order traversal line: two persistent material masses, three tensile
// elements. No twist, bending stiffness, or remeshing. A hand coordinate stays
// on the same material while the actual masses and mounts move in Jolt.
class MaterialTraversalLine final : public LineMaterialSource {
public:
    static constexpr unsigned element_count = 3, node_count = 2;
    static constexpr float node_mass_kg = .25F; // Explicit lumped model mass.
    struct NodeState {
        JPH::RVec3 position = JPH::RVec3::sZero();
        JPH::Vec3 velocity = JPH::Vec3::sZero();
    };
    struct Checkpoint {
        bool installed = false, broken = false;
        PortableLinePath::Endpoint a, b;
        float rest_length_m = 0;
        std::array<NodeState, node_count> nodes{};
        std::array<PortableLinePath::State, element_count> paths{};
    };
    MaterialTraversalLine(JPH::PhysicsSystem &, JPH::BodyID player, JPH::ObjectLayer);
    ~MaterialTraversalLine() override;
    MaterialTraversalLine(const MaterialTraversalLine &) = delete;
    MaterialTraversalLine &operator=(const MaterialTraversalLine &) = delete;
    // Outside Update. Caller releases hands before stop/restore/destruction.
    bool install(const PortableLinePath::Endpoint &, const PortableLinePath::Endpoint &, float rest_length_m);
    void stop();
    bool restore(const Checkpoint &);
    Checkpoint capture() const;
    void collision_step(); // Serialized OnStep, before material hands.
    bool sample_material(LineMaterialHandle, LineMaterialSample &) const override;
    bool nearest_material(JPH::RVec3Arg, float reach_m, LineMaterialHandle &, JPH::RVec3 &) const;
    bool installed() const { return installed_; }
    bool valid() const;
    bool broken() const;
    float tension_n() const;
    float rest_length_m() const { return rest_length_; }
    double stored_energy_j() const;
    double kinetic_energy_j() const;
    float maximum_node_speed_mps() const;
    std::uint64_t generation() const { return generation_; }
    const auto &paths() const { return paths_; }
private:
    JPH::PhysicsSystem &world_;
    JPH::BodyID player_;
    std::array<JPH::BodyID, node_count> nodes_{};
    std::array<PortableLinePath::Endpoint, element_count + 1> ends_{};
    std::array<JPH::Ref<FiniteCableConstraint>, element_count> rows_{};
    std::array<PortableLinePath::State, element_count> states_{};
    std::array<PortableLinePath::Result, element_count> paths_{};
    float rest_length_ = 0;
    bool installed_ = false, broken_ = false;
    std::uint64_t generation_ = 1;
    bool connect();
    JPH::Body *body(const PortableLinePath::Endpoint &) const;
};
} // namespace scraperx::sim
