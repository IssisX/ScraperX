#pragma once

// The Matron's skirt as cloth (#89, J-011): a coarse grid of points hung from
// her pelvis, stepped as verlet with distance links and her leg capsules.
// matron_rig.gd owns what it means; this is only its inner loop, native so it
// fits the phone's frame. Presentation only: nothing here touches the sim.

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <cstdint>
#include <vector>

namespace scraperx::bridge {

class SkirtCloth final : public godot::RefCounted {
    GDCLASS(SkirtCloth, godot::RefCounted)

public:
    // rest: cols * (rows + 1) points, column by column, row 0 pinned.
    // links: index pairs into rest; lengths: one rest length per pair.
    // shape: per row, how hard each iteration pulls a point to its rest place.
    bool configure(std::int64_t cols, std::int64_t rows, const godot::PackedVector3Array &rest,
                   const godot::PackedInt32Array &links, const godot::PackedFloat32Array &lengths,
                   const godot::PackedFloat32Array &shape, std::int64_t iterations, std::int64_t passes,
                   double damp);
    // One frame. caps: capsule ends a0, b0, a1, b1, ...; radii: one per capsule.
    // Everything in skeleton space. Returns the points, as rest is laid out.
    godot::PackedVector3Array step(const godot::Transform3D &pelvis, const godot::Vector3 &gravity, double dt,
                                   const godot::PackedVector3Array &caps, const godot::PackedFloat32Array &radii,
                                   double jump_m);
    void reset();
    // The last step's worst link ratio after its last capsule push, after its
    // last link pass, and the deepest any point then sits inside a capsule (m).
    godot::PackedFloat32Array get_stats() const;

protected:
    static void _bind_methods();

private:
    struct Link {
        std::int32_t a;
        std::int32_t b;
        float length;
        float wa;
        float wb;
    };
    int cols_ = 0;
    int rows_ = 0;
    int iterations_ = 0;
    int passes_ = 0;
    float damp_ = 0.9F;
    std::vector<godot::Vector3> rest_;
    std::vector<godot::Vector3> pts_;
    std::vector<godot::Vector3> prev_;
    std::vector<godot::Vector3> target_;
    std::vector<float> shape_;     // per point
    std::vector<Link> links_;
    bool fresh_ = true;
    float stats_[3] = {0.0F, 0.0F, 0.0F};
    float worst_link() const;
};

}  // namespace scraperx::bridge
