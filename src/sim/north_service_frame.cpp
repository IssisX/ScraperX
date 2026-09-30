#include "sim/north_service_frame.hpp"

#include <vector>

namespace scraperx::sim {
namespace {
using kit::Material;
using kit::Part;

Part span(const JPH::Vec3 low, const JPH::Vec3 high, const Material material) {
    return {0.5F * (high - low), 0.5F * (high + low),
            JPH::Quat::sIdentity(), material};
}
}

void build_north_service_frame(kit::Kit &kit) {
    std::vector<Part> parts;
    // The old north ring reaches Z=-176. This entry overlaps its outer edge
    // beam and projects to a real first-rung stance at Z=-179.5.
    parts.push_back(span({14.0F, 87.72F, -180.0F},
                         {18.0F, 88.0F, -175.75F}, Material::Galvanised));
    for (const float x : {14.2F, 17.8F}) {
        parts.push_back(span({x - 0.175F, 77.0F, -179.15F},
                             {x + 0.175F, 87.72F, -178.8F}, Material::Steel));
    }
    parts.push_back(span({14.0F, 76.25F, -179.15F},
                         {18.0F, 76.72F, -175.8F}, Material::Steel));
    parts.push_back(span({14.0F, 86.8F, -179.15F},
                         {18.0F, 87.4F, -178.8F}, Material::Rust));

    // A short climb leaves the platform for a broad first rest. The rung
    // dimensions match the existing native grip classifier.
    for (const float x : {15.45F, 16.55F}) {
        parts.push_back(span({x - 0.05F, 88.55F, -180.30F},
                             {x + 0.05F, 93.80F, -180.20F}, Material::Yellow));
    }
    for (float y = 89.25F; y <= 93.75F; y += 0.30F) {
        const float z = y > 93.6F ? -179.96F : -180.25F;
        parts.push_back(span({15.45F, y - 0.04F, z - 0.04F},
                             {16.55F, y + 0.04F, z + 0.04F}, Material::Steel));
    }
    parts.push_back(span({14.3F, 93.72F, -183.0F},
                         {17.7F, 94.0F, -180.0F}, Material::Galvanised));
    // A continuous lip face meets the climb probe at hand height. The deck
    // slab alone is above the probe ray while the climber is below its top.
    parts.push_back(span({15.3F, 92.6F, -180.30F},
                         {16.7F, 94.0F, -179.99F}, Material::Rust));
    for (const float x : {14.5F, 17.5F}) {
        parts.push_back(span({x - 0.175F, 77.0F, -181.8F},
                             {x + 0.175F, 93.72F, -181.45F}, Material::Steel));
    }
    parts.push_back(span({14.15F, 93.25F, -181.75F},
                         {17.85F, 93.72F, -181.35F}, Material::Rust));
    // Turn west on a broad service catwalk. Its next landing and tie into
    // the existing +77 m north ring make the load path legible.
    parts.push_back(span({10.0F, 93.72F, -181.9F},
                         {14.6F, 94.0F, -180.5F}, Material::Galvanised));
    parts.push_back(span({8.5F, 93.72F, -183.0F},
                         {11.5F, 94.0F, -179.25F}, Material::Galvanised));
    parts.push_back(span({8.5F, 76.25F, -182.0F},
                         {11.5F, 76.72F, -175.8F}, Material::Steel));
    for (const float x : {8.8F, 11.2F}) {
        parts.push_back(span({x - 0.175F, 76.72F, -181.8F},
                             {x + 0.175F, 93.72F, -181.45F}, Material::Steel));
    }
    for (const float x : {9.45F, 10.55F}) {
        parts.push_back(span({x - 0.05F, 94.55F, -178.90F},
                             {x + 0.05F, 97.30F, -178.80F}, Material::Yellow));
    }
    for (float y = 95.25F; y <= 97.05F; y += 0.30F) {
        const float z = y > 96.9F ? -179.14F : -178.85F;
        parts.push_back(span({9.45F, y - 0.04F, z - 0.04F},
                             {10.55F, y + 0.04F, z + 0.04F}, Material::Steel));
    }
    parts.push_back(span({8.5F, 97.22F, -179.1F},
                         {11.5F, 97.5F, -176.8F}, Material::Galvanised));
    parts.push_back(span({9.3F, 96.1F, -179.11F},
                         {10.7F, 97.5F, -178.80F}, Material::Rust));
    // A mantle-height bridge lip bears on the existing outer edge beam.
    // Its top stops just short of the tower slab rather than doubling it.
    parts.push_back(span({8.8F, 98.72F, -176.8F},
                         {11.2F, 99.0F, -176.05F}, Material::Galvanised));
    parts.push_back(span({9.3F, 97.4F, -176.85F},
                         {10.7F, 99.0F, -176.55F}, Material::Rust));

    // The +99 m ring leads to the next frame station on the same north face.
    parts.push_back(span({14.0F, 98.72F, -180.0F},
                         {18.0F, 99.0F, -175.75F}, Material::Galvanised));
    for (const float x : {14.2F, 17.8F}) {
        parts.push_back(span({x - 0.175F, 88.0F, -179.15F},
                             {x + 0.175F, 98.72F, -178.8F}, Material::Steel));
    }
    for (const float x : {15.45F, 16.55F}) {
        parts.push_back(span({x - 0.05F, 99.55F, -180.30F},
                             {x + 0.05F, 103.30F, -180.20F}, Material::Yellow));
    }
    for (float y = 100.25F; y <= 103.25F; y += 0.30F) {
        const float z = y > 103.1F ? -179.96F : -180.25F;
        parts.push_back(span({15.45F, y - 0.04F, z - 0.04F},
                             {16.55F, y + 0.04F, z + 0.04F}, Material::Steel));
    }
    parts.push_back(span({14.3F, 103.22F, -183.0F},
                         {17.7F, 103.5F, -180.0F}, Material::Galvanised));
    parts.push_back(span({15.3F, 102.1F, -180.30F},
                         {16.7F, 103.5F, -179.99F}, Material::Rust));
    for (const float x : {14.5F, 17.5F}) {
        parts.push_back(span({x - 0.175F, 94.0F, -181.8F},
                             {x + 0.175F, 103.22F, -181.45F}, Material::Steel));
    }

    // The high west shelf is too high to step or mantle from +103.5.
    // A jump reaches its lip. The canopy hides the first top-out pocket;
    // moving hand-over-hand north reveals a clear one.
    parts.push_back(span({12.2F, 105.72F, -183.0F},
                         {14.0F, 106.0F, -180.0F}, Material::Galvanised));
    parts.push_back(span({13.94F, 104.6F, -182.95F},
                         {14.03F, 106.0F, -180.05F}, Material::Rust));
    parts.push_back(span({12.2F, 107.2F, -181.25F},
                         {14.2F, 107.6F, -180.0F}, Material::Steel));
    parts.push_back(span({12.15F, 106.0F, -180.22F},
                         {12.4F, 107.2F, -179.98F}, Material::Steel));
    parts.push_back(span({12.3F, 94.0F, -181.8F},
                         {12.65F, 105.72F, -181.45F}, Material::Steel));
    parts.push_back(span({12.2F, 102.8F, -181.8F},
                         {14.6F, 103.22F, -181.45F}, Material::Rust));

    // A final compact ladder reaches the high rest, with a real fascia for
    // its top-out. The southbound catwalk leaves the ladder sweep at X=15.5.
    for (const float x : {12.45F, 13.55F}) {
        parts.push_back(span({x - 0.05F, 106.55F, -183.45F},
                             {x + 0.05F, 108.30F, -183.35F}, Material::Yellow));
    }
    for (float y = 107.25F; y <= 108.15F; y += 0.30F) {
        const float z = y > 108.0F ? -183.12F : -183.40F;
        parts.push_back(span({12.45F, y - 0.04F, z - 0.04F},
                             {13.55F, y + 0.04F, z + 0.04F}, Material::Steel));
    }
    parts.push_back(span({11.4F, 108.22F, -185.0F},
                         {14.6F, 108.5F, -183.1F}, Material::Galvanised));
    parts.push_back(span({12.3F, 107.1F, -183.40F},
                         {13.7F, 108.5F, -183.09F}, Material::Rust));
    parts.push_back(span({12.1F, 106.0F, -183.25F},
                         {12.35F, 108.22F, -182.9F}, Material::Steel));
    parts.push_back(span({13.0F, 108.22F, -184.4F},
                         {16.2F, 108.5F, -183.2F}, Material::Galvanised));
    parts.push_back(span({15.0F, 108.22F, -184.4F},
                         {16.2F, 108.5F, -176.8F}, Material::Galvanised));
    parts.push_back(span({15.45F, 103.5F, -184.0F},
                         {15.80F, 108.22F, -183.65F}, Material::Steel));
    parts.push_back(span({15.45F, 102.95F, -184.0F},
                         {15.80F, 103.22F, -182.8F}, Material::Rust));
    parts.push_back(span({14.3F, 109.72F, -176.8F},
                         {16.7F, 110.0F, -176.05F}, Material::Galvanised));
    parts.push_back(span({14.8F, 108.4F, -176.85F},
                         {16.2F, 110.0F, -176.55F}, Material::Rust));
    (void)kit.add_body(1901, parts, JPH::RVec3::sZero(),
                       JPH::Quat::sIdentity(), 0.0F, 0.85F);
}
} // namespace scraperx::sim
