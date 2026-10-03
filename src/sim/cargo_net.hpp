#pragma once
#include "sim/mechanism_kit.hpp"
#include <string>
#include <vector>
namespace scraperx::sim {
// One actual Jolt soft body. Read-back is the same deforming mesh used for
// collision, grip material coordinates, rider load and Godot presentation.
class CargoNet final {
public:
    static constexpr std::uint64_t kEntity = 1953;
    CargoNet(JPH::PhysicsSystem &system, JPH::ObjectLayer layer);
    ~CargoNet();
    CargoNet(const CargoNet &) = delete;
    CargoNet &operator=(const CargoNet &) = delete;
    [[nodiscard]] JPH::BodyID body() const noexcept { return body_; }
    void refresh();
    [[nodiscard]] bool grip(JPH::RVec3 centre, JPH::RVec3 aim, JPH::Vec3 facing, JPH::RVec3 &point) const noexcept;
    [[nodiscard]] JPH::Vec3 material_point(JPH::RVec3 world) const noexcept;
    [[nodiscard]] JPH::Vec3 material_velocity(JPH::Vec3 material) const noexcept;
    [[nodiscard]] JPH::RVec3 world_point(JPH::Vec3 material) const noexcept;
    void load(JPH::RVec3 point, JPH::Vec3 force, float dt);
    [[nodiscard]] const std::vector<JPH::RVec3> &vertices() const noexcept { return vertices_; }
    [[nodiscard]] std::vector<JPH::RVec3> render_vertices(float alpha) const;
    [[nodiscard]] JPH::RVec3 render_world_point(JPH::Vec3 material, float alpha) const noexcept;
    [[nodiscard]] const std::vector<std::uint32_t> &indices() const noexcept { return indices_; }
    [[nodiscard]] std::string capture() const;
    void restore(const std::string &state);
private:
    [[nodiscard]] JPH::RVec3 node(int c, int r) const noexcept;
    [[nodiscard]] JPH::RVec3 surface(float c, float r) const noexcept;
    [[nodiscard]] JPH::RVec3 surface(float c, float r, const std::vector<JPH::RVec3> &vertices) const noexcept;
    JPH::PhysicsSystem &system_;
    JPH::BodyID body_;
    std::vector<JPH::RVec3> vertices_;
    std::vector<JPH::RVec3> previous_vertices_;
    std::vector<std::uint32_t> indices_;
};
}
