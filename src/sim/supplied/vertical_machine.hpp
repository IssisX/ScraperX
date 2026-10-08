#pragma once
#include "sim/mechanism_kit.hpp"
#include <memory>
#include <string>
#include <vector>
#include <limits>

namespace scraperx::sim::vertical {
using namespace JPH;
using kit::BodyIndex;
using kit::Part;
using kit::Material;
struct Placement { RVec3 origin = RVec3::sZero(); float yaw = 0; };
// Caller allocates unoccupied bands. No implicit production IDs.
struct EntityRange { uint64 first; uint32 count; };
struct BuildContext {
    kit::Kit &kit;
    PhysicsSystem &world;
    Placement placement;
    EntityRange statics, dynamics;
};
struct Command { float travel = 0; bool powered = true; };
struct ControlState { Command command; std::vector<float> target_speeds; };
struct Port { std::string name; BodyIndex body; Vec3 local_point; bool walkable; };
struct NamedBody { std::string name; BodyIndex index; };
struct Drive {
    Ref<HingeConstraint> hinge;
    Ref<SliderConstraint> slider;
    float extent = 0, speed = 0;
    bool continuous = false;
    Ref<SliderConstraint> translation_feedback;
    float radians_per_metre = 0;
    BodyIndex feedback_body;
    Vec3 feedback_origin = Vec3::sZero();
    Vec3 feedback_axis = Vec3::sAxisY();
    float acceleration = std::numeric_limits<float>::infinity();
    float target_speed = 0;
};

// Scene-scoped. All mutation on the host's physics thread, outside Update().
// Owns constraints, never Kit bodies. Destroy before the associated Kit.
class Machine final {
public:
    Machine(BuildContext context, std::string id, uint32 static_count, uint32 dynamic_count);
    ~Machine();
    Machine(const Machine &) = delete;
    Machine &operator=(const Machine &) = delete;
    void command(Command value);
    void pre_step(float dt);
    ControlState capture_control() const;
    // Restore Kit body checkpoint first, then this state; clears solver history.
    void restore_control(ControlState value);
    bool owns_traversal_entity(uint64 entity) const;
    RVec3 port_position(const Port &port) const;
    const std::string id;
    std::vector<NamedBody> bodies;
    std::vector<Port> ports;
    std::vector<Drive> drives;
    std::vector<Ref<Constraint>> transmissions;
    std::vector<kit::RopeIndex> ropes;
    BuildContext context;
    RVec3 point(Vec3 local) const;
    Vec3 direction(Vec3 local) const;
    BodyIndex body(std::string name, std::vector<Part> parts, Vec3 position, float mass, float roll = 0);
    Body &native(BodyIndex body);
    Ref<HingeConstraint> hinge(BodyIndex a, BodyIndex b, Vec3 pivot, float min_angle = -JPH_PI, float max_angle = JPH_PI, float stiffness = 0, float damping = 0);
    Ref<SliderConstraint> slider(BodyIndex a, BodyIndex b, Vec3 axis, float low, float high);
    void walkable(std::string name, BodyIndex b, Vec3 point = Vec3::sZero());
    void own(Ref<Constraint> constraint);
    Ref<HingeConstraint> axis_hinge(BodyIndex a, BodyIndex b, Vec3 pivot, Vec3 axis, float low=-JPH_PI, float high=JPH_PI);
    void no_collision(BodyIndex a, BodyIndex b);
private:
    Command command_;
    uint32 static_used_ = 0, dynamic_used_ = 0;
    std::vector<Ref<Constraint>> constraints_;
};
Part box(Vec3 half, Material material = Material::Steel, Vec3 offset = Vec3::sZero());
Part wheel(float radius, float half_depth, Vec3 offset = Vec3::sZero());
std::unique_ptr<Machine> gravity_balance(BuildContext);
std::unique_ptr<Machine> traction_tram(BuildContext);
std::unique_ptr<Machine> rack_climber(BuildContext);
std::unique_ptr<Machine> crown_gondola(BuildContext);
std::unique_ptr<Machine> barrel_helix(BuildContext);
std::unique_ptr<Machine> cascade_mast(BuildContext);
std::unique_ptr<Machine> pitman_lift(BuildContext);
std::unique_ptr<Machine> create(const std::string &id, BuildContext context);
} // namespace scraperx::sim::vertical
