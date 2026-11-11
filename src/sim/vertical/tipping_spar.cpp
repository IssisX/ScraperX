#include "sim/vertical/vertical_machine.hpp"
#include <algorithm>
#include <cmath>

// J-082 set piece 1, the tipping spar (built on the crown gondola: a turning
// arm, a cabin hung level on it, a braked hinge). A 144 m steel spar turns on
// a bearing at its middle, its plane east-west. As found it stands almost
// upright, its counterweight at the top and its cabin at the foot, leaning
// kLean toward its fall and held there by the hinge's brake. Let go, it
// topples over sideways through half a turn: the counterweight falls 144 m
// and carries the cabin out over the drop and up and over to the top, where
// the spar comes to rest hanging from its counterweight. The cabin hangs from
// a pin on the spar's end and stays level all the way.
//
// The brake only resists the fall (its torque never drives the spar the way
// it falls), so the counterweight does all the lifting; it holds the fall to
// kSpeed. Called back from the lower gangway, the same motor winds the spar
// back over (lifting the counterweight, lowering the cabin): its powered reset.
//
// Local frame: the origin is the bearing's centre. The cabin end swings east
// (+x), the counterweight west.
namespace scraperx::sim::vertical {
namespace {
constexpr float kArm = 72.0F;                 // bearing to each end
constexpr float kLean = 0.0349066F;           // 2 deg, as found
constexpr float kHang = 2.6F;                 // the cabin's pin over its floor
constexpr float kChordHalfLength = 72.3F;
constexpr float kCabinZ = 3.0F;               // the cabin hangs south of the spar
constexpr float kCabinHalfX = 2.5F;
constexpr float kCabinHalfZ = 1.5F;
constexpr float kSpeed = 0.035F;              // rad/s: the cabin at 2.5 m/s
constexpr float kBrakeTorque = 3.0e6F;        // N m, over the 1.21 MN m peak
constexpr float kSparMassKg = 9000.0F;        // a third of it the counterweight
constexpr float kCabinMassKg = 1200.0F;
} // namespace

std::unique_ptr<Machine> tipping_spar(BuildContext c) {
    auto m = std::make_unique<Machine>(c, "sx.tipping_spar.v1", 3, 2);
    const float extent = JPH_PI - kLean;

    // The cabin's pin, as found and at the top.
    const Vec3 pin_low(kArm * std::sin(kLean), -kArm * std::cos(kLean), 0.0F);
    const Vec3 pin_high(0.0F, kArm, 0.0F);
    const float floor_low = pin_low.GetY() - kHang;
    const float floor_high = pin_high.GetY() - kHang;

    // The bearing on the spar's north side, and the girders and the column
    // that carry it from the tower's south face (z -134.3, 11.3 m north).
    m->body("bearing", {
        box(Vec3(.6F, .6F, .5F), Material::Steel, Vec3(0, 0, -.95F)),
        box(Vec3(.25F, .35F, 5.4F), Material::Rust, Vec3(-.45F, 0, -6.85F)),
        box(Vec3(.25F, .35F, 5.4F), Material::Rust, Vec3(.45F, 0, -6.85F)),
        box(Vec3(.3F, 10.75F, .3F), Material::Rust, Vec3(0, -7.85F, -11.3F)),
    }, Vec3::sZero(), 0);

    // The gangways, west of the cabin's pin (the side the cabin end never
    // sweeps through at the top): the lower one from TP-340's south edge, the
    // upper one to the 484 ring's south band, each to the cabin's north door.
    const float door_x0 = -2.4F, door_x1 = -1.2F;
    const float low_top = -74.7F;   // 340.35 m
    const float north_low = -12.6F; // TP-340's south edge, z -135.6
    const float cabin_north = kCabinZ - kCabinHalfZ;
    auto entry = m->body("entry", {box(Vec3(.6F, .1F, .5F * (cabin_north - .05F - north_low)), Material::Galvanised)},
                         Vec3(pin_low.GetX() + .5F * (door_x0 + door_x1), low_top - .1F,
                              .5F * (cabin_north - .05F + north_low)), 0);
    const float high_top = floor_high - .1F;   // 484.35 m
    const float north_high = -15.0F;           // onto the 484 ring's south band
    auto exit = m->body("exit", {box(Vec3(.6F, .1F, .5F * (cabin_north - .05F - north_high)), Material::Galvanised)},
                        Vec3(.5F * (door_x0 + door_x1), high_top - .1F, .5F * (cabin_north - .05F + north_high)), 0);

    // The spar: two chords, a tie every 6 m, the counterweight on its upper
    // end and the cabin's pin on its lower one. Built upright and set at its
    // lean; the kit spreads its mass by volume, so the counterweight's block
    // is sized to a third of it.
    std::vector<Part> spar;
    for (const float x : {-.25F, .25F}) spar.push_back(box(Vec3(.12F, kChordHalfLength, .3F), Material::Rust, Vec3(x, 0, 0)));
    for (float y = -66.0F; y <= 66.01F; y += 6.0F) spar.push_back(box(Vec3(.37F, .06F, .06F), Material::Yellow, Vec3(0, y, 0)));
    spar.push_back(box(Vec3(.15F, .15F, 1.5F), Material::Steel, Vec3(0, -kArm, 1.4F)));
    float volume = 0.0F;
    for (const Part &p : spar) volume += 8.0F * p.half.GetX() * p.half.GetY() * p.half.GetZ();
    const float block = .5F * std::cbrt(.5F * volume);   // its volume half the rest's
    spar.push_back(box(Vec3(block, block, block), Material::Concrete, Vec3(0, kArm, 0)));
    auto arm = m->body("spar", spar, Vec3::sZero(), kSparMassKg, kLean);
    auto bearing = m->hinge({}, arm, Vec3::sZero(), 0.0F, extent);
    bearing->GetMotorSettings().SetTorqueLimits(-kBrakeTorque, 0.0F);

    // The cabin, hung from the pin: a floor on a ballast block, railed on its
    // east, west and south sides and on its north side east of the door.
    // Each railed side is a toe board, a mid rail and a hand rail, the gaps
    // under a body's width, so the rider sees out over the drop.
    std::vector<Part> cabin_parts{
        box(Vec3(kCabinHalfX, .15F, kCabinHalfZ), Material::Timber),
        box(Vec3(1.0F, .3F, 1.0F), Material::Concrete, Vec3(0, -.45F, 0)),
        // The hanger: a post from the floor's north edge up to the pin.
        box(Vec3(.08F, .5F * kHang, .08F), Material::Steel, Vec3(0, .5F * kHang, -kCabinHalfZ + .1F)),
    };
    const auto railed = [&cabin_parts](const float x0, const float z0, const float x1, const float z1) {
        const float hx = std::max(.04F, .5F * (x1 - x0)), hz = std::max(.04F, .5F * (z1 - z0));
        const float mx = .5F * (x0 + x1), mz = .5F * (z0 + z1);
        cabin_parts.push_back(box(Vec3(hx, .075F, hz), Material::Yellow, Vec3(mx, .225F, mz)));
        cabin_parts.push_back(box(Vec3(hx, .03F, hz), Material::Yellow, Vec3(mx, .7F, mz)));
        cabin_parts.push_back(box(Vec3(hx, .03F, hz), Material::Yellow, Vec3(mx, 1.15F, mz)));
        cabin_parts.push_back(box(Vec3(.04F, .5F, .04F), Material::Yellow, Vec3(x0, .65F, z0)));
        cabin_parts.push_back(box(Vec3(.04F, .5F, .04F), Material::Yellow, Vec3(x1, .65F, z1)));
    };
    const float ex = kCabinHalfX - .04F, ez = kCabinHalfZ - .04F;
    railed(-ex, -ez, -ex, ez);    // west
    railed(ex, -ez, ex, ez);      // east
    railed(-ex, ez, ex, ez);      // south
    railed(-.9F, -ez, ex, -ez);   // north, east of the door
    auto cabin = m->body("deck", cabin_parts, Vec3(pin_low.GetX(), floor_low - .15F, kCabinZ), kCabinMassKg);
    auto hanger = m->hinge(arm, cabin, Vec3(pin_low.GetX(), pin_low.GetY(), kCabinZ - kCabinHalfZ + .1F));
    hanger->SetMaxFrictionTorque(100);
    c.kit.set_damping(cabin, 0, .3F);
    m->no_collision(arm, cabin);

    Drive drive;
    drive.hinge = bearing;
    drive.extent = extent;
    drive.speed = kSpeed;
    drive.acceleration = .01F;
    m->drives.push_back(drive);

    m->walkable("deck", cabin, Vec3(0, .15F, 0));
    m->walkable("entry", entry, Vec3(0, floor_low - (low_top - .1F), 0));
    m->walkable("exit", exit, Vec3(0, floor_high - (high_top - .1F), 0));
    return m;
}
} // namespace scraperx::sim::vertical
