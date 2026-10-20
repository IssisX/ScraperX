#pragma once
#include "sim/grapple_rope_constraint.hpp"
#include <algorithm>
#include <cmath>

namespace scraperx::sim {
struct FiniteLineReelResult {
    float rest_length_m;
    double work_j = 0;
    double absorbed_j = 0;
    bool drive_limited = false;
};

// Human takeup/payout law shared by carried and installed straight lines.
// This computes a target and command-work receipt only; the reciprocal rope
// row remains the force owner. Receipts do not imply full energy closure.
struct FiniteLineReel {
    static constexpr float reel_power_w = 750.0F;
    static constexpr float reel_speed_m_per_s = 1.0F;
    static constexpr float reel_force_n = GrappleRopeSettings::reel_force_n;
    static constexpr float stiffness_n_per_m = GrappleRopeSettings::stiffness_n_per_m;

    static double potential(float radius, float rest) {
        const double extension=std::max(0.0,double(radius)-double(rest));
        return .5*stiffness_n_per_m*extension*extension;
    }

    // Positive input takes up, negative input pays out. The predictor must use
    // the actual endpoint velocities and effective mass for this collision dt.
    // Invalid boundary inputs leave the target and both receipts unchanged.
    template<class ProspectiveTension>
    static FiniteLineReelResult step(float old, float radius, float solved_tension_n,
            float dt, float reel, float minimum_line_length_m, float maximum_line_length_m,
            const ProspectiveTension &prospective_tension) {
        FiniteLineReelResult result{old};
        if (!std::isfinite(old) || !std::isfinite(radius) || !std::isfinite(solved_tension_n) ||
            !std::isfinite(dt) || !std::isfinite(reel) || !std::isfinite(minimum_line_length_m) ||
            !std::isfinite(maximum_line_length_m) || dt<=0 || radius<0 || solved_tension_n<0 ||
            minimum_line_length_m<0 || maximum_line_length_m<minimum_line_length_m ||
            old<0 || reel==0) return result;
        reel=std::clamp(reel,-1.0F,1.0F);
        const float stroke=reel_speed_m_per_s*std::abs(reel)*dt;
        if (reel<0) {
            const float paid=std::min(maximum_line_length_m,old+stroke);
            result.absorbed_j+=std::max(0.0,potential(radius,old)-potential(radius,paid));
            result.rest_length_m=paid;
        } else if (solved_tension_n<=reel_force_n) {
            const double extension=std::max(0.0,double(radius)-old);
            const double slack=std::max(0.0,double(old)-radius);
            const double free_stroke=std::min(double(stroke),slack);
            const double base_force=std::max({double(solved_tension_n),stiffness_n_per_m*extension,
                double(prospective_tension(old,dt))});
            const double budget=reel_power_w*double(dt)*reel;
            // Solve F*d + .5*k*d^2 <= P*h without cancellation near d=0.
            const double energy_stroke=2*budget/(base_force+
                std::sqrt(base_force*base_force+2*stiffness_n_per_m*budget));
            const double loaded_stroke=std::min({std::max(0.0,double(stroke)-free_stroke),energy_stroke,
                std::max(0.0,double(reel_force_n)/stiffness_n_per_m-extension)});
            float target=std::max(minimum_line_length_m,float(double(old)-free_stroke-loaded_stroke));
            target=std::min(target,old);
            const auto charge=[&](float length) {
                const double actual_loaded=std::max(0.0,double(old)-length-slack);
                const double conservative=base_force*actual_loaded+.5*stiffness_n_per_m*actual_loaded*actual_loaded;
                return std::max(conservative,std::max(0.0,potential(radius,length)-potential(radius,old)));
            };
            // Float rest-length representation must not exceed paid work,
            // speed, or finite actuator force through rounding downward.
            while (target<old && (charge(target)>budget || double(old)-target>stroke ||
                (target<radius && stiffness_n_per_m*(double(radius)-target)>reel_force_n)))
                target=std::nextafter(target,old);
            // Stall on CURRENT prospective damping load, even when the prior
            // solved impulse was zero. No paid target change on a stalled tick.
            const float demand=prospective_tension(target,dt);
            if (!std::isfinite(demand) || demand>reel_force_n) target=old;
            // Other island contacts can change the load after this admission
            // check. While actually shortening, the drive is a finite traction
            // limiter: its row slips above1500N instead of adding extra force.
            // k/c and quadratic storage remain unchanged; no clipped damping
            // dissipation or complete energy closure is inferred from this cap.
            result.drive_limited=target<old;
            result.work_j+=charge(target);
            result.rest_length_m=target;
        }
        return result;
    }
};
} // namespace scraperx::sim
