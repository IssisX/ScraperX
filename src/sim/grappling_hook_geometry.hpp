#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>
#include <cmath>
#include <utility>
#include <vector>

namespace scraperx::sim {

struct GrapplingHookPrism {
    // Counterclockwise convex XY polygon at Z = 0, extruded over [z_low, z_high].
    std::vector<JPH::Vec3> polygon;
    float z_low = 0.0F;
    float z_high = 0.0F;
};

struct GrapplingHookLayout {
    std::vector<GrapplingHookPrism> parts;
    JPH::Vec3 eye = JPH::Vec3::sZero();
    float stem_length_m = 0.0F;
    float steel_mass_kg = 0.0F;
};

// Pure geometric data: no Jolt allocation, shape lifetime, or imposed mass properties.
inline const GrapplingHookLayout &grappling_hook_layout() {
    static const GrapplingHookLayout layout = [] {
        constexpr double density = 7850.0;
        constexpr double pi = 3.14159265358979323846;
        GrapplingHookLayout out;
        out.parts.reserve(66);
        auto box = [&](float xl, float xh, float yl, float yh, float zl, float zh) {
            out.parts.push_back({{{xl, yl, 0}, {xh, yl, 0}, {xh, yh, 0}, {xl, yh, 0}}, zl, zh});
        };
        auto volume = [&] {
            double result = 0.0;
            for (const auto &part : out.parts) {
                double twice_area = 0.0;
                for (std::size_t i = 0; i < part.polygon.size(); ++i) {
                    const auto &a = part.polygon[i];
                    const auto &b = part.polygon[(i + 1) % part.polygon.size()];
                    twice_area += double(a.GetX()) * b.GetY() - double(b.GetX()) * a.GetY();
                }
                result += 0.5 * twice_area * (double(part.z_high) - part.z_low);
            }
            return result;
        };
        for (float z : {-0.09F, -0.03F, 0.03F, 0.09F}) {
            for (int segment = 0; segment < 12; ++segment) {
                const double a = (-45.0 + 22.5 * segment) * pi / 180.0;
                const double b = a + pi / 8.0;
                auto point = [](double radius, double angle) {
                    return JPH::Vec3(float(radius * std::cos(angle)), float(0.10 + radius * std::sin(angle)), 0);
                };
                std::vector<JPH::Vec3> polygon{point(0.05, a), point(0.09, a), point(0.09, b), point(0.05, b)};
                // Mirror the two flat weld boundaries without filling the mouth.
                for (float face : {-0.085F, 0.085F}) {
                    std::vector<JPH::Vec3> clipped;
                    auto inside = [&](const JPH::Vec3 &p) {
                        return face < 0 ? p.GetX() >= face : p.GetX() <= face;
                    };
                    for (std::size_t i = 0; i < polygon.size(); ++i) {
                        const auto &p = polygon[i];
                        const auto &q = polygon[(i + 1) % polygon.size()];
                        const bool p_inside = inside(p), q_inside = inside(q);
                        if (p_inside) clipped.push_back(p);
                        if (p_inside != q_inside) {
                            const double t = (double(face) - p.GetX()) / (double(q.GetX()) - p.GetX());
                            clipped.emplace_back(face, float(p.GetY() + t * (double(q.GetY()) - p.GetY())), 0);
                        }
                    }
                    polygon = std::move(clipped);
                }
                out.parts.push_back({std::move(polygon), z - 0.0015F, z + 0.0015F});
            }
        }
        const auto finger_count=out.parts.size();
        box(-0.097F, -0.085F, 0.092F, 0.108F, -0.0915F, 0.0915F);
        box(0.085F, 0.097F, 0.092F, 0.108F, -0.0915F, 0.0915F);
        // CHOSEN175mm real hollow stems put the steel COM below the crook
        // cavity. The previous short stems left it8.6mm above the cavity:
        // a slack catch rolled toward the mouth before the line loaded it.
        // Redistribute the same2kg of steel, rather than overriding inertia
        // or supplying an orientation motor. Tube and eye bars partition
        // their volume without overlapping corners.
        out.stem_length_m = .175F;
        const float bottom = 0.100F - out.stem_length_m;
        auto mirrored_box = [&](float xl, float xh, float yl, float yh, float zl, float zh) {
            box(xl, xh, yl, yh, zl, zh);
            box(-xh, -xl, yl, yh, zl, zh);
        };
        mirrored_box(-0.123F, -0.120F, bottom, 0.100F, -0.013F, 0.013F);
        mirrored_box(-0.100F, -0.097F, bottom, 0.100F, -0.013F, 0.013F);
        mirrored_box(-0.120F, -0.100F, bottom, 0.100F, -0.013F, -0.010F);
        mirrored_box(-0.120F, -0.100F, bottom, 0.100F, 0.010F, 0.013F);
        const float eye_y = bottom - 0.020F;
        mirrored_box(-0.130F, -0.120F, eye_y - 0.020F, bottom, -0.0015F, 0.0015F);
        mirrored_box(-0.100F, -0.090F, eye_y - 0.020F, bottom, -0.0015F, 0.0015F);
        mirrored_box(-0.120F, -0.100F, eye_y - 0.020F, eye_y - 0.010F, -0.0015F, 0.0015F);
        mirrored_box(-0.120F, -0.100F, eye_y + 0.010F, bottom, -0.0015F, 0.0015F);
        out.eye = JPH::Vec3(-0.110F, eye_y, 0);
        // Only the48 finger prisms change thickness. Fixed bridges, hollow
        // tubes and eyes keep their actual sections. The remaining mass
        // determines finger thickness (~1.60043mm); XY mouth stays open.
        double finger_volume = 0.0;
        for (std::size_t i=0;i<finger_count;++i) {
            const auto &part=out.parts[i];
            double twice_area=0.0;
            for(std::size_t j=0;j<part.polygon.size();++j) {
                const auto &a=part.polygon[j];
                const auto &b=part.polygon[(j+1)%part.polygon.size()];
                twice_area+=double(a.GetX())*b.GetY()-double(b.GetX())*a.GetY();
            }
            finger_volume+=.5*twice_area*(double(part.z_high)-part.z_low);
        }
        const double finger_scale=(2.0/density-(volume()-finger_volume))/finger_volume;
        for(std::size_t i=0;i<finger_count;++i) {
            auto &part=out.parts[i];
            const float center=(part.z_low+part.z_high)*.5F;
            const float half=float((double(part.z_high)-part.z_low)*finger_scale*.5);
            part.z_low=center-half;part.z_high=center+half;
        }
        out.steel_mass_kg = float(density * volume());
        return out;
    }();
    return layout;
}

} // namespace scraperx::sim
