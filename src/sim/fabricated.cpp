// A fabricated object's hull file, read and bounded, and its body's shape
// (see fabricated.hpp).
#include "sim/fabricated.hpp"

#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace scraperx::sim::fabricated {

namespace {

// Steel, timber, rubber, concrete: the density each weighs by (as the tool,
// tools/fab/hullfile.py), the friction and restitution it meets things with,
// and what it sounds like struck. Rubber bounces; it is heard as a dull
// thud, as timber is; concrete as heavy and hard, as solid steel is.
constexpr std::array<MaterialSpec, kMaterials> kMaterialSpecs{{
    {7850.0F, 0.5F, 0.0F, ImpactSound::HeavySteel},
    {600.0F, 0.7F, 0.0F, ImpactSound::Timber},
    {1100.0F, 0.9F, 0.4F, ImpactSound::Timber},
    {2400.0F, 0.8F, 0.0F, ImpactSound::HeavySteel},
}};

constexpr char kMagic[8] = {'S', 'X', 'F', 'A', 'B', 'H', 'U', 'L'};

class Bytes final {
public:
    Bytes(const std::uint8_t *data, const std::size_t size) : data_(data), size_(size) {}
    template <typename T>
    bool read(T &value) {
        if (size_ - at_ < sizeof(T)) {
            return false;
        }
        std::memcpy(&value, data_ + at_, sizeof(T));
        at_ += sizeof(T);
        return true;
    }
    [[nodiscard]] bool done() const noexcept { return at_ == size_; }

private:
    const std::uint8_t *data_;
    std::size_t size_;
    std::size_t at_ = 0;
};

} // namespace

const MaterialSpec &material_spec(const Material material) noexcept {
    return kMaterialSpecs[std::min<std::size_t>(static_cast<std::size_t>(material), kMaterials - 1)];
}

FabricatedRefusal parse(const std::uint8_t *data, const std::size_t size, Spec &out) {
    out = Spec{};
    if (data == nullptr || size < kMinFileBytes || size > kMaxFileBytes) {
        return FabricatedRefusal::Size;
    }
    Bytes in(data, size);
    char magic[8] = {};
    std::uint32_t version = 0, material = 0, hulls = 0;
    float mass = 0.0F, volume = 0.0F;
    std::array<float, 3> half{};
    if (!in.read(magic) || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) {
        return FabricatedRefusal::Format;
    }
    if (!in.read(version) || version != kVersion) {
        return FabricatedRefusal::Version;
    }
    if (!in.read(material) || material >= kMaterials) {
        return FabricatedRefusal::Material;
    }
    if (!in.read(hulls) || hulls < 1 || hulls > kMaxHulls) {
        return FabricatedRefusal::HullCount;
    }
    if (!in.read(mass) || !in.read(volume) || !in.read(half) || !in.read(out.glb_sha512)) {
        return FabricatedRefusal::Format;
    }
    if (!std::isfinite(mass) || !std::isfinite(volume) ||
        !std::all_of(half.begin(), half.end(), [](const float h) { return std::isfinite(h); })) {
        return FabricatedRefusal::NotFinite;
    }
    for (const float h : half) {
        if (2.0F * h > kCarrierBoundM + kBoxSlackM) {
            return FabricatedRefusal::Oversized;
        }
        if (2.0F * h < kMinExtentM) {
            return FabricatedRefusal::Undersized;
        }
    }
    out.hulls.reserve(hulls);
    for (std::uint32_t h = 0; h < hulls; ++h) {
        std::uint32_t count = 0;
        if (!in.read(count)) {
            return FabricatedRefusal::Format;
        }
        if (count < kMinHullPoints || count > kMaxHullPoints) {
            return FabricatedRefusal::PointCount;
        }
        std::vector<JPH::Vec3> points;
        points.reserve(count);
        for (std::uint32_t p = 0; p < count; ++p) {
            std::array<float, 3> xyz{};
            if (!in.read(xyz)) {
                return FabricatedRefusal::Format;
            }
            for (std::size_t axis = 0; axis < 3; ++axis) {
                if (!std::isfinite(xyz[axis])) {
                    return FabricatedRefusal::NotFinite;
                }
                if (std::abs(xyz[axis]) > half[axis] + kBoxSlackM) {
                    return FabricatedRefusal::Oversized;
                }
            }
            points.emplace_back(xyz[0], xyz[1], xyz[2]);
        }
        out.hulls.push_back(std::move(points));
    }
    if (!in.done()) {
        return FabricatedRefusal::Format;
    }
    if (mass > kMaxMassKg) {
        return FabricatedRefusal::TooHeavy;
    }
    if (mass < kMinMassKg) {
        return FabricatedRefusal::TooLight;
    }
    out.material = static_cast<Material>(material);
    out.mass_kg = mass;
    out.volume_m3 = volume;
    out.half_extent = half;
    return FabricatedRefusal::None;
}

FabricatedRefusal build_shape(const Spec &spec, JPH::RefConst<JPH::Shape> &shape, float &mass_kg) {
    shape = nullptr;
    mass_kg = 0.0F;
    const MaterialSpec &material = material_spec(spec.material);
    std::vector<JPH::RefConst<JPH::Shape>> hulls;
    float volume = 0.0F;
    for (const std::vector<JPH::Vec3> &points : spec.hulls) {
        JPH::ConvexHullShapeSettings settings(points.data(), static_cast<int>(points.size()), JPH::cDefaultConvexRadius);
        settings.SetDensity(material.density_kg_m3);
        const JPH::ShapeSettings::ShapeResult result = settings.Create();
        if (result.HasError()) {
            return FabricatedRefusal::Degenerate;
        }
        const JPH::RefConst<JPH::Shape> hull = result.Get();
        if (!(hull->GetVolume() >= kMinHullVolumeM3)) {
            return FabricatedRefusal::Degenerate;
        }
        volume += hull->GetVolume();
        hulls.push_back(hull);
    }
    if (hulls.size() == 1) {
        shape = hulls.front();
    } else {
        JPH::StaticCompoundShapeSettings compound;
        for (const JPH::RefConst<JPH::Shape> &hull : hulls) {
            compound.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(), hull.GetPtr());
        }
        const JPH::ShapeSettings::ShapeResult result = compound.Create();
        if (result.HasError()) {
            return FabricatedRefusal::Degenerate;
        }
        shape = result.Get();
    }
    const float mass = shape->GetMassProperties().mMass;
    // The physics' own measure of the hulls against the file's: its density
    // times its volume is its mass, within kMassTolerance.
    const float expected = material.density_kg_m3 * volume;
    if (!std::isfinite(mass) || std::abs(mass - expected) > kMassTolerance * expected ||
        std::abs(spec.mass_kg - mass) > kMassTolerance * mass ||
        std::abs(spec.volume_m3 - volume) > kMassTolerance * volume) {
        return FabricatedRefusal::MassMismatch;
    }
    if (mass > kMaxMassKg) {
        return FabricatedRefusal::TooHeavy;
    }
    if (mass < kMinMassKg) {
        return FabricatedRefusal::TooLight;
    }
    mass_kg = mass;
    return FabricatedRefusal::None;
}

const char *refusal_name(const FabricatedRefusal refusal) noexcept {
    switch (refusal) {
    case FabricatedRefusal::None: return "none";
    case FabricatedRefusal::Size: return "size";
    case FabricatedRefusal::Format: return "format";
    case FabricatedRefusal::Version: return "version";
    case FabricatedRefusal::Material: return "material";
    case FabricatedRefusal::HullCount: return "hull count";
    case FabricatedRefusal::PointCount: return "point count";
    case FabricatedRefusal::NotFinite: return "not finite";
    case FabricatedRefusal::Oversized: return "oversized";
    case FabricatedRefusal::Undersized: return "undersized";
    case FabricatedRefusal::Degenerate: return "degenerate";
    case FabricatedRefusal::TooHeavy: return "too heavy";
    case FabricatedRefusal::TooLight: return "too light";
    case FabricatedRefusal::MassMismatch: return "mass mismatch";
    case FabricatedRefusal::Full: return "full";
    case FabricatedRefusal::Placement: return "placement";
    }
    return "unknown";
}

} // namespace scraperx::sim::fabricated
