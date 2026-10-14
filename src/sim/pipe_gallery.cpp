// The pipe gallery in the yard (see pipe_gallery.hpp).
#include "sim/pipe_gallery.hpp"

#include <cmath>
#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::Material;
using kit::Part;

const RVec3 kOrigin(31.0, 0.0, PipeGallery::kLaneZ);
constexpr float kHalfWidth = 1.2F;   // the stands' and the platform's half width across the lane
constexpr float kBeamZ = 0.74F;      // the beams either side of the lane
constexpr float kPipeHalf = 0.035F;  // a pipe's half section: a hold (under kGripMaxSection)

Vec3 local(const float x, const float y, const float z) {
    return Vec3(x - float(kOrigin.GetX()), y - float(kOrigin.GetY()), z - float(kOrigin.GetZ()));
}

Part span(const Vec3 a, const Vec3 b, const Material material) {
    Part part;
    part.half = (b - a).Abs() * 0.5F;
    part.offset = local((a.GetX() + b.GetX()) * 0.5F, (a.GetY() + b.GetY()) * 0.5F, (a.GetZ() + b.GetZ()) * 0.5F);
    part.material = material;
    return part;
}

// A deck from x0 to x1 at top on four legs to the yard.
void stand(std::vector<Part> &parts, const float x0, const float x1, const float top) {
    const float z = PipeGallery::kLaneZ;
    parts.push_back(span(Vec3(x0, top - 0.2F, z - kHalfWidth), Vec3(x1, top, z + kHalfWidth), Material::Timber));
    for (const float x : {x0 + 0.15F, x1 - 0.15F})
        for (const float dz : {-kHalfWidth + 0.15F, kHalfWidth - 0.15F})
            parts.push_back(span(Vec3(x - 0.12F, 0.0F, z + dz - 0.12F), Vec3(x + 0.12F, top - 0.2F, z + dz + 0.12F),
                                 Material::Steel));
}

// A ramp of planks along the lane from (x0, y0) to (x1, y1).
Part ramp(const float x0, const float y0, const float x1, const float y1) {
    const Vec3 a = local(x0, y0, PipeGallery::kLaneZ);
    const Vec3 b = local(x1, y1, PipeGallery::kLaneZ);
    const Vec3 run = b - a;
    Part part;
    part.half = Vec3(0.5F * run.Length(), 0.12F, 0.8F);
    part.offset = (a + b) * 0.5F - Vec3(0.0F, 0.12F, 0.0F);
    part.rotation = Quat::sRotation(Vec3::sAxisZ(), std::atan2(run.GetY(), run.GetX()));
    part.material = Material::Timber;
    return part;
}
} // namespace

void PipeGallery::build(kit::Kit &kit) {
    std::vector<Part> parts;
    const float z = kLaneZ;
    // The start stand and its ramp up from the yard (1 in 5).
    stand(parts, kStandWestX, kStandEastX, kStandTop);
    parts.push_back(ramp(kStandWestX - 7.5F, 0.0F, kStandWestX, kStandTop));
    // Its east edge painted: the take-off (wider than a hand closes round).
    parts.push_back(span(Vec3(kStandEastX - 0.3F, kStandTop, z - kHalfWidth), Vec3(kStandEastX, kStandTop + 0.01F,
                                                                                    z + kHalfWidth),
                         Material::Yellow));
    // The pipes, across the lane between the two beams.
    const float last_x = kFirstPipeX + kPipeSpacing * float(kPipeCount - 1);
    for (int pipe = 0; pipe < kPipeCount; ++pipe) {
        const float x = kFirstPipeX + kPipeSpacing * float(pipe);
        parts.push_back(span(Vec3(x - kPipeHalf, kPipeY - kPipeHalf, z - kBeamZ + 0.12F),
                             Vec3(x + kPipeHalf, kPipeY + kPipeHalf, z + kBeamZ - 0.12F), Material::Galvanised));
    }
    // The beams the pipes hang between, and their posts to the yard.
    const float beam_bottom = kPipeY - kPipeHalf;
    const float beam_x0 = kFirstPipeX - 0.5F;
    const float beam_x1 = last_x + 0.4F;
    for (const float side : {-1.0F, 1.0F}) {
        const float bz = z + side * kBeamZ;
        parts.push_back(span(Vec3(beam_x0, beam_bottom, bz - 0.12F), Vec3(beam_x1, beam_bottom + 0.3F, bz + 0.12F),
                             Material::Rust));
        for (const float x : {beam_x0 + 0.12F, 0.5F * (beam_x0 + beam_x1), beam_x1 - 0.12F})
            parts.push_back(span(Vec3(x - 0.12F, 0.0F, bz - 0.12F), Vec3(x + 0.12F, beam_bottom, bz + 0.12F),
                                 Material::Rust));
    }
    // The platform past the last pipe: a painted fascia down its west face
    // to the lip, a ramp down east to the yard.
    stand(parts, kPlatformWestX, kPlatformEastX, kPlatformTop);
    parts.push_back(span(Vec3(kPlatformWestX, kPlatformTop - 1.6F, z - kHalfWidth),
                         Vec3(kPlatformWestX + 0.1F, kPlatformTop - 0.2F, z + kHalfWidth), Material::Yellow));
    parts.push_back(ramp(kPlatformEastX, kPlatformTop, kPlatformEastX + 20.0F, 0.0F));
    (void)kit.add_body(kFrameEntity, parts, kOrigin, Quat::sIdentity(), 0.0F, 0.8F);
}

} // namespace scraperx::sim
