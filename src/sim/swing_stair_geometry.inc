constexpr float kStairApproachTop = 33.00F;
const JPH::RVec3 kS2Hinge(-15.00, 32.70, -122.10);
constexpr float kS2Pitch = 0.70860367F;   // 40.6 deg, seated: the treads level
constexpr float kS2Stored = 1.43116999F;  // 82 deg, upright on its catch
constexpr float kS2Lift = kS2Stored - kS2Pitch;
constexpr float kS2Rise = 0.25F;
constexpr int kS2Treads = 43;             // then the top landing, the 44th rise
constexpr float kS2FirstNosing = 0.25F;   // tread 1's nosing, east of the hinge
constexpr float kS2LandingLength = 1.00F;
constexpr float kS2StringerZ = 0.95F;
constexpr float kS2StringerHalfZ = 0.06F;
constexpr float kS2StringerTop = 0.08F;   // its faces about the pitch line
constexpr float kS2StringerBottom = -0.42F;
constexpr float kS2StringerFoot = -0.20F; // from the pitch line's point nearest the hinge
constexpr float kS2TreadHalfZ = 0.89F;
constexpr float kS2RailHeight = 1.00F;
constexpr float kS2FlightMassKg = 4000.0F;
constexpr float kS2CounterweightKg = 13500.0F;
constexpr float kS2CounterweightArm = 2.50F;
constexpr float kS2CounterweightRound = 0.03490659F;  // 2 deg past opposite the stair
const JPH::Vec3 kS2CounterweightHalf(0.55F, 0.55F, 0.89F);
// The catch: a trip lever on a post at the landing's south edge. Its hook
// (east) rests down over the lug under a cast weight on a mast over the pivot,
// 11.5 deg east of upright; the chain hangs from its west arm's end over a
// guide, the handle at hand height over the landing. Pulled 0.20 rad, the
// weight passes over the pivot and throws the lever onto its far stop, where
// it stays with the hook up: let go early or late, the hook cannot fall back
// across the lug's path. The catch lets go only once the lever is past that
// dead point.
const JPH::RVec3 kS2CatchPivot(-16.00, 35.90, -120.90);
constexpr float kS2CatchMassKg = 60.0F;
constexpr float kS2CatchWeightKg = 50.0F;
const JPH::Vec3 kS2CatchWeightHalf(0.15F, 0.15F, 0.04F);
constexpr float kS2CatchTravel = 0.45F;
constexpr float kS2CatchRelease = 0.26F;
constexpr float kS2ChainArm = 1.50F;      // the chain's point, west of the pivot
const JPH::RVec3 kS2ChainGuide(-17.50, 35.05, -120.90);
constexpr float kS2ChainTop = kStairApproachTop + 1.95F;

// The stair about its hinge, seated.
[[nodiscard]] std::vector<Part> s2_stair_parts() {
    const float going = kS2Rise / std::tan(kS2Pitch);
    const float foot_top = kStairApproachTop - static_cast<float>(kS2Hinge.GetY());
    const float top = foot_top + kS2Rise * static_cast<float>(kS2Treads + 1);
    const float landing_x = kS2FirstNosing + static_cast<float>(kS2Treads) * going;
    const JPH::Vec3 along(std::cos(kS2Pitch), std::sin(kS2Pitch), 0.0F);
    const JPH::Vec3 across(-std::sin(kS2Pitch), std::cos(kS2Pitch), 0.0F);
    const JPH::Quat pitched = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), kS2Pitch);
    const auto pitch_y = [&](const float x) {
        return foot_top + kS2Rise + (x - kS2FirstNosing) * std::tan(kS2Pitch);
    };
    const auto stringer_top_y = [&](const float x) {
        return pitch_y(x) + kS2StringerTop / std::cos(kS2Pitch);
    };
    const JPH::Vec3 nosing1(kS2FirstNosing, pitch_y(kS2FirstNosing), 0.0F);
    const JPH::Vec3 foot = nosing1 - nosing1.Dot(along) * along;
    std::vector<Part> parts;
    // Two stringers, from just behind the hinge to under the top landing.
    const float stringer_end =
        (landing_x - 0.15F - foot.GetX() - kS2StringerTop * across.GetX()) / along.GetX();
    const float stringer_mid = 0.5F * (kS2StringerFoot + stringer_end);
    const float stringer_t = 0.5F * (kS2StringerTop + kS2StringerBottom);
    for (const float side : {-1.0F, 1.0F}) {
        parts.push_back({JPH::Vec3(0.5F * (stringer_end - kS2StringerFoot),
                                   0.5F * (kS2StringerTop - kS2StringerBottom), kS2StringerHalfZ),
                         foot + stringer_mid * along + stringer_t * across +
                             JPH::Vec3(0.0F, 0.0F, side * kS2StringerZ),
                         pitched, Material::Yellow});
    }
    // The lug on the south stringer's outer face that the catch's hook holds.
    parts.push_back({JPH::Vec3(0.10F, 0.10F, 0.135F),
                     foot + 3.0F * along - 0.17F * across + JPH::Vec3(0.0F, 0.0F, 1.145F), pitched,
                     Material::Hazard});
    // The foot plate over the hinge, level with the +33 m ring, then the treads.
    parts.push_back(span({-0.30F, foot_top - 0.10F, -kS2TreadHalfZ},
                         {kS2FirstNosing, foot_top, kS2TreadHalfZ}, Material::Galvanised));
    for (int k = 1; k <= kS2Treads; ++k) {
        const float x = kS2FirstNosing + static_cast<float>(k - 1) * going;
        const float y = foot_top + kS2Rise * static_cast<float>(k);
        parts.push_back(span({x, y - 0.05F, -kS2TreadHalfZ}, {x + going, y, kS2TreadHalfZ},
                             Material::Galvanised));
    }
    // Risers close every step, the top landing's included. Stood upright,
    // open treads would lie at 41 deg one above another, a slope a body
    // pushing into them rides up; closed, their face leans back only 8 deg.
    for (int k = 1; k <= kS2Treads + 1; ++k) {
        const float x = kS2FirstNosing + static_cast<float>(k - 1) * going;
        const float y = foot_top + kS2Rise * static_cast<float>(k);
        parts.push_back(span({x - 0.03F, y - kS2Rise - 0.05F, -kS2TreadHalfZ}, {x, y, kS2TreadHalfZ},
                             Material::Steel));
    }
    // The top landing over the full width, its beams, and its retained central
    // blade. Two visible receiver stacks straddle the blade.
    parts.push_back(span({landing_x, top - 0.12F, -1.01F},
                         {landing_x + kS2LandingLength, top, 1.01F}, Material::Galvanised));
    for (const float side : {-1.0F, 1.0F}) {
        parts.push_back(span({landing_x - 0.30F, top - 0.50F, side * kS2StringerZ - 0.06F},
                             {landing_x + kS2LandingLength, top - 0.12F, side * kS2StringerZ + 0.06F},
                             Material::Yellow));
    }
    parts.push_back(span({landing_x + 0.30F, top - 1.10F, -0.03F},
                         {landing_x + 0.80F, top - 0.12F, 0.03F}, Material::Steel));
    // Handrails: flat posts and flat rails, 0.20 by 0.06, too broad to grip,
    // so the upright stair is not a ladder. The north rail stops at the last
    // tread: the top landing's north side is where the deck is.
    for (const float side : {-1.0F, 1.0F}) {
        const float z = side * 0.98F;
        const float last = side < 0.0F ? landing_x - going : landing_x - 0.10F;
        std::vector<float> xs;
        for (int i = 0; i < 6; ++i) {
            const float x = 1.2F + 2.3F * static_cast<float>(i);
            if (x < last - 0.3F) {
                xs.push_back(x);
            }
        }
        xs.push_back(last);
        for (const float x : xs) {
            const float y = stringer_top_y(x);
            parts.push_back(span({x - 0.10F, y, z - 0.03F}, {x + 0.10F, y + kS2RailHeight, z + 0.03F},
                                 Material::Yellow));
        }
        const JPH::Vec3 a(xs.front(), stringer_top_y(xs.front()) + kS2RailHeight, z);
        const JPH::Vec3 b(xs.back(), stringer_top_y(xs.back()) + kS2RailHeight, z);
        parts.push_back({JPH::Vec3(0.5F * (b - a).Length() + 0.10F, 0.10F, 0.03F),
                         0.5F * (a + b) - JPH::Vec3(0.0F, 0.10F, 0.0F), pitched, Material::Yellow});
    }
    const float landing_end = landing_x + kS2LandingLength;
    for (const float x : {landing_x + 0.45F, landing_end - 0.03F}) {
        parts.push_back(span({x - 0.10F, top, 0.95F}, {x + 0.10F, top + kS2RailHeight, 1.01F},
                             Material::Yellow));
    }
    parts.push_back(span({landing_x - 0.10F, top + kS2RailHeight - 0.20F, 0.95F},
                         {landing_end, top + kS2RailHeight, 1.01F}, Material::Yellow));
    parts.push_back(span({landing_end - 0.06F, top, -0.95F}, {landing_end, top + kS2RailHeight, -0.75F},
                         Material::Yellow));
    parts.push_back(span({landing_end - 0.06F, top + kS2RailHeight - 0.20F, -0.95F},
                         {landing_end, top + kS2RailHeight, 1.01F}, Material::Yellow));
    // The hinge's shaft, its ends in the bearings.
    parts.push_back(box(JPH::Vec3(0.15F, 0.15F, 1.15F), JPH::Vec3::sZero(), Material::Steel));

    // The counterweight hangs 2.5 m behind the hinge, a little past opposite
    // the stair's centre, on two arms in line with the stringers.
    JPH::Vec3 centre = JPH::Vec3::sZero();   // volume-weighted: its direction is all that is used
    for (const Part &part : parts) {
        centre += 8.0F * part.half.GetX() * part.half.GetY() * part.half.GetZ() * part.offset;
    }
    const float heading = std::atan2(centre.GetY(), centre.GetX()) + JPH::JPH_PI + kS2CounterweightRound;
    const JPH::Vec3 out(std::cos(heading), std::sin(heading), 0.0F);
    const JPH::Quat turned = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), heading);
    const float arm = kS2CounterweightArm + 0.30F;
    for (const float side : {-1.0F, 1.0F}) {
        parts.push_back({JPH::Vec3(0.5F * (arm + 0.15F), 0.20F, 0.06F),
                         0.5F * (arm - 0.15F) * out + JPH::Vec3(0.0F, 0.0F, side * kS2StringerZ), turned,
                         Material::Yellow});
    }
    const JPH::Vec3 &h = kS2CounterweightHalf;
    parts.push_back({h, kS2CounterweightArm * out, turned, Material::Rust,
                     kit::Part::Shape::Box, kS2CounterweightKg});
    double volume = 0;
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        const auto h = parts[i].half;
        volume += 8. * h.GetX() * h.GetY() * h.GetZ();
    }
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        const auto h = parts[i].half;
        parts[i].mass_kg = float(kS2FlightMassKg * 8. * h.GetX() * h.GetY() * h.GetZ() / volume);
    }
    return parts;
}
