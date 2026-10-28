// Her climbs as her own scan of the world found them (matron_traversal.hpp):
// the rung ways off a floor, read straight from the scan, the nearest one,
// and her way up out of her own trap.

#include "sim/matron.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace scraperx::sim {
using namespace JPH;

int Matron::climb_count() const noexcept {
    return scanned_ == nullptr ? 0 : static_cast<int>(scanned_->edges().size());
}

// A way up as her own scan of the world found it: a usable rung edge of her
// scanned map, read straight from the scan (no table of hers), faced toward
// its rungs from the side she starts on.
bool Matron::climb_of(const int edge, Climb &out, const Ways ways) const noexcept {
    if (edge < 0 || edge >= climb_count()) {
        return false;
    }
    const traversal::Edge &e = scanned_->edges()[static_cast<std::size_t>(edge)];
    const bool mantle = e.kind == traversal::MoveKind::Mantle && ways == Ways::Any;
    if (!e.usable || (e.kind != traversal::MoveKind::RungUp && !mantle)) {
        return false;
    }
    const auto &regions = scanned_->regions();
    const traversal::MoveSpec &m = e.spec;
    const Vec3 face = -m.out;
    // A lip from a floor is a way up for her hands as rungs are: the scan's line is her standing spot, so the lip is
    // midway between it and the top she steps onto.
    const float line_x = mantle ? 0.5F * (m.start.GetX() + m.goal.GetX()) : m.line.GetX();
    const float line_z = mantle ? 0.5F * (m.start.GetZ() + m.goal.GetZ()) : m.line.GetZ();
    const float head = regions[static_cast<std::size_t>(e.to)].top;
    // She hangs against the rungs, not where she stood at the foot: her radius and a hand's clearance off the line.
    const float hang = kRadius + 0.05F;
    // The lip is the scan's own head patch, not the region's average: a bridge or fascia can stand above its deck.
    out = {line_x - face.GetX() * hang, line_z - face.GetZ() * hang, m.start.GetY(), head, face.GetX(), face.GetZ(),
           m.goal.GetX(), m.goal.GetZ(), true, hang, m.goal_top};
    return true;
}

Matron::Climb Matron::climb_spec(const int edge) const noexcept {
    Climb c{};
    (void)climb_of(edge, c);
    return c;
}

// Her nearest scanned way off this floor: up from its foot, or down from its
// top. -1 when her scan found none.
int Matron::nearest_climb(const float x, const float z, const float floor_y, const bool down) const noexcept {
    int best = -1;
    float best_d = std::numeric_limits<float>::max();
    for (int i = 0; i < climb_count(); ++i) {
        Climb c{};
        if (!climb_of(i, c)) {
            continue;
        }
        // A climb's top floor can be its deck or the bridge over it.
        const float gap = down ? std::min(std::abs(c.head_y - floor_y), std::abs(c.lip_y - floor_y))
                               : std::abs(c.foot_y - floor_y);
        if (gap > 0.6F) {
            continue;
        }
        const float d = down ? std::hypot(x - c.top_x, z - c.top_z) : std::hypot(x - c.x, z - c.z);
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

// Out of her own trap: the scanned way up nearest her, off the floor she
// stands on.
int Matron::bay_way_out() const noexcept {
    if (!body_in_world_) {
        return -1;
    }
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    return nearest_climb(float(at.GetX()), float(at.GetZ()), float(at.GetY()) - kStandCentre, false);
}

} // namespace scraperx::sim
