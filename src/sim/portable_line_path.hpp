#pragma once
#include "sim/finite_cable_constraint.hpp"
#include "sim/rounded_box_line_path.hpp"
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <array>
#include <cstdint>

namespace JPH { class PhysicsSystem; }
namespace scraperx::sim {
// Actual-world binding for a massless cable with one supported Box-leaf contact.
// Persistent topology contains IDs and owning shape references, never Body*.
// Evaluate only outside Update or in the owner's serialized OnStep listener.
class PortableLinePath final {
public:
    static constexpr std::size_t participant_capacity=FiniteCableConstraint::capacity;
    static constexpr std::size_t visual_capacity=35;
    static constexpr std::size_t query_capacity=32;
    struct Contact {
        JPH::BodyID body;
        JPH::SubShapeID subshape;
        JPH::RefConst<JPH::Shape> body_shape_identity;
        JPH::RefConst<JPH::Shape> leaf_shape_identity;
        RoundedBoxLineTopology topology;
    };
    struct State {
        bool has_contact=false;
        Contact contact;
        std::uint64_t token=0; // increments on contact entry/release/invalidation
    };
    struct Input {
        JPH::BodyID player, fitting, anchor_body;
        JPH::RefConst<JPH::Shape> anchor_shape_identity;
        JPH::RVec3 endpoint_a=JPH::RVec3::sZero(); // actual fitting COM
        JPH::Vec3 anchor_shape_local_point=JPH::Vec3::sZero();
    };
    enum class Reason : std::uint8_t {
        None, InvalidInput, EndpointMissing, AnchorChanged, ContactChanged,
        UnsupportedShape, InvalidLeaf, UnsupportedGeometry, AdditionalContact,
        NearbyBoundaryGeometry, CapacityExceeded
    };
    struct Result {
        bool valid=false; // false always has zero participant_count
        Reason reason=Reason::InvalidInput;
        double length_m=0;
        std::array<FiniteCableConstraint::Participant,participant_capacity> participants{};
        std::uint32_t participant_count=0;
        std::array<JPH::RVec3,visual_capacity> visual_points{};
        std::uint32_t visual_point_count=0;
        bool has_contact=false;
        Contact contact;
        std::uint64_t topology_token=0;
        RoundedBoxLineStatus geometry_status=RoundedBoxLineStatus::Invalid;
    };
    // The direct chord and tangent legs use actual narrowphase leaf discovery,
    // then exact Box geometry (outer ray-box false positives are skipped).
    // Other geometry intersecting a conservative curved-boundary envelope is
    // rejected explicitly. Multiple contacts/endcaps are unsupported, never
    // approximated as pins or chord lengths. Samples are presentation only.
    static Result evaluate(JPH::PhysicsSystem &,const Input &,State &);
};
} // namespace scraperx::sim
