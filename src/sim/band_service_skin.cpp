#include "sim/bands.hpp"
#include "sim/simulation.hpp"

#include <cmath>
#include <vector>

// B06 west service skin: one athletic climb off the north-west corner of
// TP-640, up to a fixed service deck at 672.25 m. It is not a machine and
// it does not touch the crane jib (that pendulum stays on the east and
// south of the plate, below it). Every part is a real member of one
// service run: a manifold, a duct, an inspection beam, a maintenance
// lattice, a cable tray, an air handler, a louver and a brace.
//
// The route stays in x [-16, -9], z [-150, -139] except the two canopies
// that only catch a fall. Nothing of it is a stair, a ramp, or a ladder
// that reaches the deck on its own.
//
// Air-grab only takes a hold while vertical speed is under 0.2 m/s, which
// on a jump is the apex. Hands there are about 2.4 m above the takeoff, so
// the first grippable rung of each leap sits there. A chord 1.0 m above
// the takeoff is fatter than a hand can close on (section 0.24 m): it is
// the structure, not a hold, and it is too low to mantle.

namespace scraperx::sim::bands {

namespace {

using kit::Material;
using kit::Part;
using Sim = Simulation;

Part box(const JPH::Vec3 half, const JPH::Vec3 center, const Material material) {
    return {half, center, JPH::Quat::sIdentity(), material};
}

Part span(const JPH::Vec3 low, const JPH::Vec3 high, const Material material) {
    return box(0.5F * (high - low), 0.5F * (high + low), material);
}

// A bar a hand can close round: both thin dimensions under 0.18 m.
void grip_bar(std::vector<Part> &parts, const JPH::Vec3 low, const JPH::Vec3 high) {
    parts.push_back(span(low, high, Material::Yellow));
}

// The 640 m floor. This climb sits on it and then goes west, off the
// building. That is what this climb does. It is not the pattern. The next
// machine goes in an opening that is already part of the building.
constexpr float kPlate = 640.25F;

// The manifold the vault crosses, facing north. The chest ray is 0.90 m
// above the feet, so a crown at the asked 0.80 m passes under it and is
// never a vault. 0.93 m is the lowest solid face that ray meets, and it
// is still inside the vault band (0.35–1.15). A lip stood above that
// crown would be the thing the arc hits: the vault only clears its own
// ledge by 0.10 m.
constexpr float kManifoldX0 = -11.70F;
constexpr float kManifoldX1 = -8.90F;
constexpr float kManifoldZ0 = -146.22F;
constexpr float kManifoldZ1 = -145.78F;
constexpr float kManifoldTop = kPlate + 0.93F;

// The duct west of the vault landing. Top is 1.45 m over the plate.
constexpr float kDuctX0 = -14.40F;
constexpr float kDuctX1 = -12.10F;
constexpr float kDuctZ0 = -145.70F;
constexpr float kDuctZ1 = -143.05F;
constexpr float kDuctTop = kPlate + 1.45F;

// Inspection beam, 0.32 m wide, from the duct's top out over the void.
// Local +X of the box runs from `kBeamA` to `kBeamB`.
const JPH::Vec3 kBeamA(-13.15F, kDuctTop, -144.85F);
const JPH::Vec3 kBeamB(-15.55F, kDuctTop, -142.15F);

// Maintenance lattice north of the beam's end. First grip is at apex-hand
// height; the chord under it is not a hold.
constexpr float kLatticeX = -15.55F;
constexpr float kLatticeZ = -139.35F;
constexpr float kLatticeChordY = kDuctTop + 1.00F;
constexpr float kLatticeRung0 = kDuctTop + 2.40F;
constexpr float kLatticeRung1 = 650.35F;

// Balcony the lateral traverse mantles onto. East of the lattice.
constexpr float kBalconyTop = 650.30F;
constexpr float kBalconyX0 = -13.15F;
constexpr float kBalconyX1 = -11.15F;
constexpr float kBalconyZ0 = -141.05F;
constexpr float kBalconyZ1 = -139.05F;

// Cable tray over the run south of the balcony. Underside 1.45 m: under a
// standing body (1.8 m), over a crouched one (1.2 m). Its north edge stays
// clear of a mantle landing on the balcony (the capsule there reaches
// about z -140.5). Three metres, then a standing apron.
constexpr float kTrayX0 = -12.85F;
constexpr float kTrayX1 = -11.35F;
constexpr float kTrayZ0 = -144.05F;
constexpr float kTrayZ1 = -141.05F;
constexpr float kTrayUnderside = kBalconyTop + 1.45F;

// Air-handler casing south of the tray, clear of it. Top is 1.55 m over
// the crouch floor. The east face is the mantle.
constexpr float kHandlerX0 = -14.85F;
constexpr float kHandlerX1 = -12.95F;
constexpr float kHandlerZ0 = -145.45F;
constexpr float kHandlerZ1 = -144.25F;
constexpr float kHandlerTop = kBalconyTop + 1.55F;

// Louver south of the handler, 2.8 m of open air off the casing's south face.
constexpr float kLouverX = -13.90F;
constexpr float kLouverZ = -148.25F;
constexpr float kLouverRung0 = kHandlerTop + 2.40F;
constexpr float kLouverRung1 = 658.55F;

// Where a jump back off the louver lands. The throw is 2.5 m/s off the
// face; a full stick air-controls that out to about 4 m, a lighter hold
// less. The pad covers both, and it stops short of the louver so it is
// not a ceiling on that climb. The ladder stands at its north edge.
constexpr float kBackTop = 656.20F;
constexpr float kBackX0 = -15.05F;
constexpr float kBackX1 = -12.05F;
constexpr float kBackZ0 = -146.50F;
constexpr float kBackZ1 = -140.20F;

// Offset ladder just south of the service deck's fascia. The mantle is
// 1.6 m, so the head of the ladder has to put a chest at that rise: the
// deck is 672.25 and the chest ray is 0.90 over the feet.
constexpr float kBraceX = -12.70F;
constexpr float kBraceZ = -140.75F;
constexpr float kBraceTop = 671.70F;

// The small deck. Its south edge is the step up from the ladder.
// The hinged sheet at the west edge falls into open air, beside the
// building. It is not this climb. Do not keep building past it.
constexpr float kDeckTop = 672.25F;
constexpr float kDeckX0 = -14.35F;
constexpr float kDeckX1 = -11.15F;
constexpr float kDeckZ0 = -140.30F;
constexpr float kDeckZ1 = -138.90F;

void build_manifold(std::vector<Part> &parts) {
    parts.push_back(span({kManifoldX0, kPlate + 0.02F, kManifoldZ0},
                         {kManifoldX1, kManifoldTop, kManifoldZ1}, Material::Galvanised));
    // End cheeks, too tall to mantle and too thick to grip, so the vault
    // is the way through the lane.
    for (const float x0 : {kManifoldX0 - 0.42F, kManifoldX1}) {
        parts.push_back(span({x0, kPlate + 0.02F, kManifoldZ0 - 0.28F},
                             {x0 + 0.38F, kPlate + 2.45F, kManifoldZ1 + 0.18F}, Material::Steel));
    }
    // Hazard band on the crown, so the bar reads as the thing to clear.
    parts.push_back(span({kManifoldX0 + 0.15F, kManifoldTop - 0.06F, kManifoldZ0 - 0.02F},
                         {kManifoldX1 - 0.15F, kManifoldTop, kManifoldZ1 + 0.02F}, Material::Hazard));
    // East plenum. Taller than a vault, a mantle or a standing hang, so the
    // lane through the manifold is the way onto the duct. 0.40 m thick: not
    // a hold.
    parts.push_back(span({kManifoldX1 + 0.30F, kPlate + 0.02F, -147.40F},
                         {-6.40F, kPlate + 2.60F, -143.40F}, Material::Steel));
}

void build_duct(std::vector<Part> &parts) {
    parts.push_back(span({kDuctX0, kPlate + 0.04F, kDuctZ0}, {kDuctX1, kDuctTop, kDuctZ1},
                         Material::Galvanised));
    // A north return, too tall to mantle, so the east face is the mantle.
    parts.push_back(span({kDuctX0, kDuctTop, kDuctZ1 - 0.08F},
                         {kDuctX1 + 0.04F, kDuctTop + 2.20F, kDuctZ1 + 0.28F}, Material::Steel));
}

void build_beam(std::vector<Part> &parts) {
    const JPH::Vec3 delta = kBeamB - kBeamA;
    const float length = std::sqrt(delta.GetX() * delta.GetX() + delta.GetZ() * delta.GetZ());
    const JPH::Vec3 mid = 0.5F * (kBeamA + kBeamB);
    // Jolt's rotation of local +X about Y by yaw lands on (cos yaw, 0, -sin yaw).
    const float yaw = std::atan2(-delta.GetZ(), delta.GetX());
    const JPH::Quat rot = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), yaw);
    parts.push_back({JPH::Vec3(0.5F * length, 0.08F, 0.16F), mid - JPH::Vec3(0.0F, 0.08F, 0.0F), rot,
                     Material::Yellow});
}

void add_ladder(std::vector<Part> &parts, const float x, const float z, const float y0, const float y1,
                const bool across_x) {
    const float half = 0.28F;
    for (const float side : {-1.0F, 1.0F}) {
        const JPH::Vec3 c = across_x ? JPH::Vec3(x + side * half, 0.0F, z) : JPH::Vec3(x, 0.0F, z + side * half);
        parts.push_back(span({c.GetX() - 0.03F, y0, c.GetZ() - 0.03F}, {c.GetX() + 0.03F, y1, c.GetZ() + 0.03F},
                             Material::Yellow));
    }
    const JPH::Vec3 rung = across_x ? JPH::Vec3(half, 0.02F, 0.02F) : JPH::Vec3(0.02F, 0.02F, half);
    for (float y = y0 + 0.30F; y <= y1 - 0.04F; y += 0.30F) {
        parts.push_back(box(rung, JPH::Vec3(x, y, z), Material::Steel));
    }
}

void build_lattice(std::vector<Part> &parts) {
    // The low chord: 1.0 m over the beam, section 0.24 m, not a hold.
    parts.push_back(span({kLatticeX - 0.55F, kLatticeChordY - 0.12F, kLatticeZ - 0.12F},
                         {kLatticeX + 0.55F, kLatticeChordY + 0.12F, kLatticeZ + 0.12F}, Material::Rust));
    add_ladder(parts, kLatticeX, kLatticeZ, kLatticeRung0 - 0.20F, kLatticeRung1, true);
    // Soffit over the ladder. Its underside stops a climber whose head meets
    // it, and it has no face at chest height, so looking up is not a mantle.
    // It stays west of the balcony.
    parts.push_back(span({kLatticeX - 0.70F, 650.72F, kLatticeZ - 0.35F},
                         {kLatticeX + 0.50F, 651.02F, kLatticeZ + 0.85F}, Material::Steel));
    // Horizontal holds running east, at the height a mantle onto the balcony
    // is still inside 1.85 m. They stop short of the fascia.
    for (const float y : {649.90F, 650.25F}) {
        grip_bar(parts, {kLatticeX - 0.05F, y - 0.03F, kLatticeZ - 0.03F},
                 {-13.40F, y + 0.03F, kLatticeZ + 0.03F});
    }
    // Catch under the leap. Its ladder climbs back to the beam only, and
    // stands south of the takeoff so it is not a rung of the leap.
    parts.push_back(span({kLatticeX - 0.90F, 638.55F, -142.35F}, {kLatticeX + 0.70F, 638.75F, -140.55F},
                         Material::Rust));
    add_ladder(parts, kBeamB.GetX(), kBeamB.GetZ() - 0.55F, 638.75F, kDuctTop - 0.15F, true);
}

void build_balcony_and_tray(std::vector<Part> &parts) {
    parts.push_back(span({kBalconyX0, kBalconyTop - 0.22F, kBalconyZ0},
                         {kBalconyX1, kBalconyTop, kBalconyZ1}, Material::Galvanised));
    // West fascia, down through chest height of a climber on the upper
    // holds, and south far enough to meet a body that hangs behind the
    // lattice. Thin, but taller and longer than a hand can close on.
    parts.push_back(span({kBalconyX0 - 0.08F, 649.05F, -140.85F},
                         {kBalconyX0 + 0.02F, kBalconyTop, -138.95F}, Material::Steel));
    // The crouch floor, south off the balcony, and a standing apron past
    // the tray so the body can stand before the next mantle.
    parts.push_back(span({kTrayX0, kBalconyTop - 0.22F, kTrayZ0}, {kTrayX1, kBalconyTop, kBalconyZ1},
                         Material::Concrete));
    parts.push_back(span({kTrayX0, kBalconyTop - 0.22F, kTrayZ0 - 0.95F},
                         {kTrayX1, kBalconyTop, kTrayZ0}, Material::Concrete));
    // The tray itself, and two posts too thick to grip. No rail under it.
    parts.push_back(span({kTrayX0, kTrayUnderside, kTrayZ0}, {kTrayX1, kTrayUnderside + 0.28F, kTrayZ1},
                         Material::Steel));
    for (const float z : {kTrayZ0 + 0.25F, kTrayZ1 - 0.25F}) {
        parts.push_back(span({kTrayX0 - 0.08F, kBalconyTop, z - 0.16F},
                             {kTrayX0 + 0.22F, kTrayUnderside, z + 0.16F}, Material::Rust));
        parts.push_back(span({kTrayX1 - 0.22F, kBalconyTop, z - 0.16F},
                             {kTrayX1 + 0.08F, kTrayUnderside, z + 0.16F}, Material::Rust));
    }
}

void build_handler_and_louver(std::vector<Part> &parts) {
    parts.push_back(span({kHandlerX0, kBalconyTop, kHandlerZ0}, {kHandlerX1, kHandlerTop, kHandlerZ1},
                         Material::Galvanised));
    parts.push_back(span({kHandlerX0 + 0.08F, kHandlerTop - 0.05F, kHandlerZ0 + 0.08F},
                         {kHandlerX1 - 0.08F, kHandlerTop, kHandlerZ1 - 0.08F}, Material::Hazard));
    // Low chord of the louver, not a hold. The climb starts at the apex rung.
    parts.push_back(span({kLouverX - 0.50F, kHandlerTop + 0.88F, kLouverZ - 0.12F},
                         {kLouverX + 0.50F, kHandlerTop + 1.12F, kLouverZ + 0.12F}, Material::Rust));
    add_ladder(parts, kLouverX, kLouverZ, kLouverRung0 - 0.20F, kLouverRung1, false);
    // Canopy over the louver's south lip.
    parts.push_back(span({kLouverX - 0.70F, 659.05F, kLouverZ - 0.75F},
                         {kLouverX + 0.70F, 659.38F, kLouverZ + 0.20F}, Material::Steel));
    // Catch under the gap, with a climb back onto the handler only.
    // low z is the louver side; high z is the handler side.
    parts.push_back(span({kLouverX - 0.80F, 649.40F, kLouverZ - 0.40F},
                         {kLouverX + 0.80F, 649.62F, kHandlerZ1 - 0.10F}, Material::Rust));
    add_ladder(parts, -13.55F, kHandlerZ1 - 0.15F, 649.62F, kHandlerTop - 0.10F, false);
}

void build_return_and_deck(std::vector<Part> &parts) {
    // The jump-back landing. 2.2 m by 2.2 m, level with the upper louver.
    parts.push_back(span({kBackX0, kBackTop - 0.18F, kBackZ0}, {kBackX1, kBackTop, kBackZ1},
                         Material::Galvanised));
    // Diagonal brace beside the ladder, section 0.24 m so it is not a hold.
    // A rotated bar, not the box between its ends: that box would be a wall.
    const JPH::Vec3 brace_foot(kBraceX + 0.55F, kBackTop + 0.30F, kBraceZ);
    const JPH::Vec3 brace_head(kBraceX + 0.55F, kBraceTop - 0.50F, kBraceZ + 1.15F);
    const JPH::Vec3 brace_delta = brace_head - brace_foot;
    const float brace_len = brace_delta.Length();
    const JPH::Vec3 brace_mid = 0.5F * (brace_foot + brace_head);
    const JPH::Vec3 brace_dir = brace_delta / brace_len;
    const JPH::Quat brace_rot = JPH::Quat::sFromTo(JPH::Vec3::sAxisX(), brace_dir);
    parts.push_back({JPH::Vec3(0.5F * brace_len, 0.12F, 0.12F), brace_mid, brace_rot, Material::Rust});
    add_ladder(parts, kBraceX, kBraceZ, kBackTop, kBraceTop, true);
    // Service deck, the receiver. South fascia is one lip from the ladder
    // head to the deck top, so the mantle has a face to take.
    parts.push_back(span({kDeckX0, kDeckTop - 0.24F, kDeckZ0}, {kDeckX1, kDeckTop, kDeckZ1},
                         Material::Concrete));
    parts.push_back(span({kBraceX - 0.70F, 670.85F, kDeckZ0 - 0.05F},
                         {kBraceX + 0.70F, kDeckTop, kDeckZ0 + 0.06F}, Material::Steel));
    // Two posts under the deck, 0.28 m square, clear of the ladder.
    for (const float x : {kDeckX0 + 0.35F, kDeckX1 - 0.35F}) {
        parts.push_back(span({x - 0.14F, kBackTop, kDeckZ0 + 0.20F},
                             {x + 0.14F, kDeckTop - 0.24F, kDeckZ0 + 0.48F}, Material::Rust));
    }
}

// A tall sheet on a hinge at the west edge, held by a pin. Lift the pin
// and the sheet falls west onto a platform in open air. That platform is
// beside the building. Do not treat it as the next step up, and do not
// build the next piece past it.
constexpr float kHingeX = -14.35F;
constexpr float kHingeY = 672.40F;
constexpr float kHingeZ = -139.60F;
constexpr float kLeafHalf = 5.50F;
constexpr float kLean = 0.14F;
constexpr float kFall = 0.80F;
// Clevis, metres up the plate from the hinge, so the pin is on the deck.
constexpr float kClevisS = 0.12F;

void build_leaf(kit::Kit &kit) {
    const JPH::RVec3 hinge(kHingeX, kHingeY, kHingeZ);
    const JPH::Quat lean = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), kLean);
    const JPH::Vec3 up = lean * JPH::Vec3::sAxisY();
    // Local origin is the plate centre. The hinge is local (0, -half, 0).
    // The two lugs leave a gap the pin occupies without touching either.
    const JPH::Vec3 lug(0.36F, -kLeafHalf + kClevisS, 0.0F);
    const kit::BodyIndex plate = kit.add_body(
        Sim::kLeafPlateEntityId,
        {box(JPH::Vec3(0.09F, kLeafHalf, 1.10F), JPH::Vec3::sZero(), Material::Galvanised),
         box(JPH::Vec3(0.02F, kLeafHalf, 0.08F), JPH::Vec3(0.09F, 0.0F, 0.90F), Material::Hazard),
         box(JPH::Vec3(0.22F, 0.035F, 0.03F), lug + JPH::Vec3(0.0F, 0.0F, 0.18F), Material::Steel),
         box(JPH::Vec3(0.22F, 0.035F, 0.03F), lug + JPH::Vec3(0.0F, 0.0F, -0.18F), Material::Steel)},
        hinge + JPH::RVec3(lean * JPH::Vec3(0.0F, kLeafHalf, 0.0F)), lean, 4200.0F, 0.95F);
    (void)kit.add_lever(plate, hinge, JPH::Vec3::sAxisZ(), up, 0.0F, kFall);

    const JPH::RVec3 pin_at = hinge + JPH::RVec3(lean * JPH::Vec3(0.36F, kClevisS, 0.0F));
    const JPH::Vec3 pin_c(static_cast<float>(pin_at.GetX()), static_cast<float>(pin_at.GetY()),
                          static_cast<float>(pin_at.GetZ()));
    std::vector<Part> frame;
    // Low plinth on the deck. The pin sits on it. 0.16 m of step, not a rail.
    frame.push_back(box(JPH::Vec3(0.20F, 0.08F, 0.16F), JPH::Vec3(pin_c.GetX(), 672.36F, pin_c.GetZ()),
                         Material::Steel));
    // Hinge cheeks outside the plate (half-width 1.10 m), not through it.
    frame.push_back(span({kHingeX - 0.10F, kHingeY - 0.45F, kHingeZ - 1.58F},
                         {kHingeX + 0.10F, kHingeY + 0.14F, kHingeZ - 1.28F}, Material::Steel));
    frame.push_back(span({kHingeX - 0.10F, kHingeY - 0.45F, kHingeZ + 1.28F},
                         {kHingeX + 0.10F, kHingeY + 0.14F, kHingeZ + 1.58F}, Material::Steel));
    // Landing the fallen tip clears. East edge stays west of the lower face
    // at rest, top is a step down off the ramp, not a wall the plate hits.
    frame.push_back(span({-26.50F, 678.42F, kHingeZ - 1.60F}, {-23.30F, 678.64F, kHingeZ + 1.60F},
                         Material::Concrete));
    frame.push_back(span({-26.85F, 668.00F, kHingeZ - 0.40F}, {-26.35F, 678.42F, kHingeZ + 0.40F},
                         Material::Rust));
    (void)kit.add_body(Sim::kLeafFrameEntityId, frame, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.9F);

    const kit::BodyIndex pin = kit.add_body(
        Sim::kLeafPinEntityId,
        {box(JPH::Vec3(0.045F, 0.045F, 0.11F), JPH::Vec3::sZero(), Material::Hazard)}, pin_at,
        JPH::Quat::sIdentity(), 8.0F, 1.0F);
    kit.set_carry(pin, kit::CarryKind::Load, JPH::Vec3(0.0F, 0.05F, 0.0F));
    (void)kit.add_pin_catch(plate, pin, 0.15F, 0.08F);
}

} // namespace

void build_service_skin(kit::Kit &kit) {
    std::vector<Part> manifold;
    build_manifold(manifold);
    (void)kit.add_body(Sim::kSkinManifoldEntityId, manifold, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F,
                       0.8F);

    std::vector<Part> skin;
    build_duct(skin);
    build_beam(skin);
    build_lattice(skin);
    build_balcony_and_tray(skin);
    build_handler_and_louver(skin);
    build_return_and_deck(skin);
    (void)kit.add_body(Sim::kSkinWestEntityId, skin, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
    build_leaf(kit);
}

} // namespace scraperx::sim::bands
