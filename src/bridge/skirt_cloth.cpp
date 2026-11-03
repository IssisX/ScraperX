#include "bridge/skirt_cloth.hpp"

#include <godot_cpp/core/class_db.hpp>

#include <algorithm>
#include <cmath>

namespace scraperx::bridge {

namespace {

godot::Vector3 out_of_capsule(const godot::Vector3 &q, const godot::Vector3 &a, const godot::Vector3 &b,
                              const float radius) {
    const godot::Vector3 ab = b - a;
    const float t = std::clamp((q - a).dot(ab) / std::max(ab.length_squared(), 1.0e-8F), 0.0F, 1.0F);
    const godot::Vector3 c = a + ab * t;
    const godot::Vector3 d = q - c;
    const float l = d.length();
    if (l >= radius || l < 1.0e-6F) {
        return q;
    }
    return c + d / l * radius;
}

}  // namespace

bool SkirtCloth::configure(const std::int64_t cols, const std::int64_t rows, const godot::PackedVector3Array &rest,
                           const godot::PackedInt32Array &links, const godot::PackedFloat32Array &lengths,
                           const godot::PackedFloat32Array &shape, const std::int64_t iterations,
                           const std::int64_t passes, const double damp) {
    const std::int64_t n = cols * (rows + 1);
    if (cols <= 0 || rows <= 0 || rest.size() != n || links.size() != 2 * lengths.size() || shape.size() == 0) {
        return false;
    }
    cols_ = static_cast<int>(cols);
    rows_ = static_cast<int>(rows);
    iterations_ = static_cast<int>(std::max<std::int64_t>(iterations, 1));
    passes_ = static_cast<int>(std::max<std::int64_t>(passes, 1));
    damp_ = static_cast<float>(damp);
    rest_.assign(static_cast<std::size_t>(n), godot::Vector3());
    shape_.assign(static_cast<std::size_t>(n), 0.0F);
    for (std::int64_t i = 0; i < n; ++i) {
        rest_[static_cast<std::size_t>(i)] = rest[i];
        const std::int64_t r = i % (rows + 1);
        shape_[static_cast<std::size_t>(i)] = shape[std::min<std::int64_t>(r, shape.size() - 1)];
    }
    links_.clear();
    for (std::int64_t k = 0; k < lengths.size(); ++k) {
        Link l{links[2 * k], links[2 * k + 1], lengths[k], 1.0F, 1.0F};
        if (l.a < 0 || l.b < 0 || l.a >= n || l.b >= n) {
            return false;
        }
        const bool pa = l.a % (rows_ + 1) == 0;
        const bool pb = l.b % (rows_ + 1) == 0;
        if (pa && pb) {
            continue;
        }
        l.wa = pa ? 0.0F : (pb ? 1.0F : 0.5F);
        l.wb = pb ? 0.0F : (pa ? 1.0F : 0.5F);
        links_.push_back(l);
    }
    pts_.assign(rest_.size(), godot::Vector3());
    prev_ = pts_;
    target_ = pts_;
    fresh_ = true;
    return true;
}

void SkirtCloth::reset() { fresh_ = true; }

godot::PackedVector3Array SkirtCloth::step(const godot::Transform3D &pelvis, const godot::Vector3 &gravity,
                                           const double dt_in, const godot::PackedVector3Array &caps,
                                           const godot::PackedFloat32Array &radii, const double jump_m) {
    godot::PackedVector3Array out;
    if (cols_ == 0) {
        return out;
    }
    const std::size_t n = rest_.size();
    const int stride = rows_ + 1;
    for (std::size_t i = 0; i < n; ++i) {
        target_[i] = pelvis.xform(rest_[i]);
    }
    if (!fresh_ && pts_[0].distance_to(target_[0]) > static_cast<float>(jump_m)) {
        fresh_ = true;
    }
    if (fresh_) {
        pts_ = target_;
        prev_ = target_;
        fresh_ = false;
    }
    const float dt = static_cast<float>(std::clamp(dt_in, 1.0 / 240.0, 1.0 / 30.0));
    const godot::Vector3 g = gravity * (dt * dt);
    for (std::size_t i = 0; i < n; ++i) {
        if (static_cast<int>(i) % stride == 0) {
            pts_[i] = target_[i];
            prev_[i] = target_[i];
            continue;
        }
        const godot::Vector3 cur = pts_[i];
        const godot::Vector3 vel = (cur - prev_[i]) * damp_;
        prev_[i] = cur;
        pts_[i] = cur + vel + g;
    }
    const std::int64_t ncap = std::min<std::int64_t>(caps.size() / 2, radii.size());
    for (int it = 0; it < iterations_; ++it) {
        // Out of her legs first, then the cloth's own lengths last.
        for (std::size_t i = 0; i < n; ++i) {
            if (static_cast<int>(i) % stride == 0) {
                continue;
            }
            godot::Vector3 q = pts_[i].lerp(target_[i], shape_[i]);
            for (std::int64_t c = 0; c < ncap; ++c) {
                q = out_of_capsule(q, caps[2 * c], caps[2 * c + 1], radii[c]);
            }
            pts_[i] = q;
        }
        if (it == iterations_ - 1) {
            stats_[0] = worst_link();
        }
        for (int p = 0; p < passes_; ++p) {
            for (const Link &l : links_) {
                godot::Vector3 &pa = pts_[static_cast<std::size_t>(l.a)];
                godot::Vector3 &pb = pts_[static_cast<std::size_t>(l.b)];
                const godot::Vector3 d = pb - pa;
                const float len = d.length();
                if (len < 1.0e-6F) {
                    continue;
                }
                const godot::Vector3 fix = d * ((len - l.length) / len);
                pa += fix * l.wa;
                pb -= fix * l.wb;
            }
        }
    }
    stats_[1] = worst_link();
    float deepest = 0.0F;
    for (std::size_t i = 0; i < n; ++i) {
        for (std::int64_t c = 0; c < ncap; ++c) {
            deepest = std::max(deepest, pts_[i].distance_to(out_of_capsule(pts_[i], caps[2 * c], caps[2 * c + 1], radii[c])));
        }
    }
    stats_[2] = deepest;
    out.resize(static_cast<std::int64_t>(n));
    for (std::size_t i = 0; i < n; ++i) {
        out.set(static_cast<std::int64_t>(i), pts_[i]);
    }
    return out;
}

float SkirtCloth::worst_link() const {
    float worst = 0.0F;
    for (const Link &l : links_) {
        worst = std::max(worst, pts_[static_cast<std::size_t>(l.a)].distance_to(pts_[static_cast<std::size_t>(l.b)]) /
                                    std::max(l.length, 1.0e-6F));
    }
    return worst;
}

godot::PackedFloat32Array SkirtCloth::get_stats() const {
    godot::PackedFloat32Array out;
    for (const float v : stats_) {
        out.append(v);
    }
    return out;
}

void SkirtCloth::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("get_stats"), &SkirtCloth::get_stats);
    godot::ClassDB::bind_method(godot::D_METHOD("configure", "cols", "rows", "rest", "links", "lengths", "shape",
                                                "iterations", "passes", "damp"),
                                &SkirtCloth::configure);
    godot::ClassDB::bind_method(godot::D_METHOD("step", "pelvis", "gravity", "dt", "caps", "radii", "jump_m"),
                                &SkirtCloth::step);
    godot::ClassDB::bind_method(godot::D_METHOD("reset"), &SkirtCloth::reset);
}

}  // namespace scraperx::bridge
