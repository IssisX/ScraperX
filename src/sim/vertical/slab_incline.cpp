#include "sim/vertical/vertical_machine.hpp"
#include <cmath>

// The owner's drawing (2026-10-08): a concrete slab falls in a timber tower;
// its rope over the tower's head sheave hauls a trolley 30 m up a 60-degree
// incline. Local frame: the incline rises toward +x; the slab's tower stands
// beside its head on the +z side. The trolley's deck stays level on a raked
// chassis that rides the rail 2.9 m below it, so at the foot its deck stands
// 3.4 m over the ground. The rider boards from the -z side at the foot and
// steps off to the -z side at the head. As placed (on the south face, from
// the yard to deck 3) the ground is 0.1 m under the origin: trestles and
// posts stand on it, and the slab stops above it.
namespace scraperx::sim::vertical {
std::unique_ptr<Machine> slab_incline(BuildContext c) {
    constexpr float kRise = 30.0f;
    constexpr float kAngle = DegreesToRadians(60.0f);
    const float travel = kRise / std::sin(kAngle);               // 34.64 m along the rail
    const Vec3 along(std::cos(kAngle), std::sin(kAngle), 0);     // up the incline
    constexpr float kDeckHalfX = 1.4f, kDeckHalfZ = 1.6f, kDeckY = 3.05f;
    constexpr float kGroundY = -.1f;
    constexpr float kTowerZ = 4.5f;
    // The rail's centre line passes this far under the deck's centre, so the
    // deck's uphill underside corner clears it.
    const float drop = .25f + std::tan(kAngle) * kDeckHalfX + .15f + .1f;
    const Vec3 deck_start(0, kDeckY, 0);
    const Vec3 deck_end = deck_start + along * travel;
    const Vec3 rail_start = deck_start - Vec3(0, drop, 0) - along * 1.5f;
    const Vec3 rail_end = deck_end - Vec3(0, drop, 0) + along * 1.0f;
    const float rail_length = (rail_end - rail_start).Length();
    const Vec3 rail_mid = .5f * (rail_start + rail_end);

    // The head sheave on the incline's line past the trolley's end; the
    // slab's tower beside the head, its sheave over the slab.
    const Vec3 attach_local(kDeckHalfX, -drop + std::tan(kAngle) * kDeckHalfX, 0);   // the shoe, on the rail's line
    const Vec3 attach_end = deck_end + attach_local;
    const Vec3 head_sheave = attach_end + along * 1.0f;
    const float slab_x = head_sheave.GetX() - 2.5f;
    const Vec3 slab_sheave(slab_x, kDeckY + kRise + 9.0f, kTowerZ);
    constexpr float kSlabHalfY = 1.5f;
    const Vec3 slab_start(slab_x, slab_sheave.GetY() - 2.0f - kSlabHalfY, kTowerZ);

    auto m = std::make_unique<Machine>(c, "sx.slab_incline.v1", 3, 2);

    std::vector<Part> frame;
    // Two rails, either side of the chassis' keel.
    for (const float z : {-.45f, .45f}) {
        auto rail = box(Vec3(.5f * rail_length, .15f, .12f), Material::Timber, rail_mid + Vec3(0, 0, z));
        rail.rotation = Quat::sRotation(Vec3::sAxisZ(), kAngle);
        frame.push_back(rail);
    }
    // A member between two points, its long axis along them.
    const auto span = [](Vec3 from, Vec3 to, float half_w, Material material) {
        const Vec3 run = to - from;
        auto p = box(Vec3(.5f * run.Length(), half_w, half_w), material, .5f * (from + to));
        p.rotation = Quat::sFromTo(Vec3::sAxisX(), run.Normalized());
        return p;
    };
    // Trestles from the ground up under the rails.
    for (float s = 4.0f; s < travel; s += 6.0f) {
        const Vec3 p = rail_start + along * s;
        const float top = p.GetY() - .15f;
        if (top > kGroundY + .5f) {
            frame.push_back(span(Vec3(p.GetX(), kGroundY, -.8f), Vec3(p.GetX(), top, -.8f), .18f, Material::Timber));
            frame.push_back(span(Vec3(p.GetX(), kGroundY, .8f), Vec3(p.GetX(), top, .8f), .18f, Material::Timber));
            frame.push_back(box(Vec3(.12f, .12f, .9f), Material::Timber, Vec3(p.GetX(), top - .1f, 0)));
        }
    }
    // The tower: four posts round the slab, a head frame, the sheaves; a
    // post under the head sheave and a beam from it to the tower's head.
    const float tower_top = slab_sheave.GetY() + .9f;
    for (const float dx : {-1.6f, 1.6f}) {
        for (const float dz : {-1.6f, 1.6f}) {
            frame.push_back(span(Vec3(slab_x + dx, kGroundY, kTowerZ + dz),
                                 Vec3(slab_x + dx, tower_top, kTowerZ + dz), .25f, Material::Steel));
        }
    }
    frame.push_back(box(Vec3(1.9f, .3f, 1.9f), Material::Steel, Vec3(slab_x, tower_top, kTowerZ)));
    frame.push_back(span(Vec3(head_sheave.GetX(), kGroundY, -.9f), Vec3(head_sheave.GetX(), tower_top, -.9f), .25f,
                         Material::Steel));
    frame.push_back(span(Vec3(head_sheave.GetX(), tower_top, -.9f), Vec3(slab_x, tower_top, kTowerZ), .25f,
                         Material::Steel));
    frame.push_back(span(Vec3(head_sheave.GetX(), head_sheave.GetY() + .9f, -.9f),
                         Vec3(head_sheave.GetX(), head_sheave.GetY() + .9f, .3f), .15f, Material::Steel));
    frame.push_back(wheel(.8f, .2f, head_sheave));
    frame.push_back(wheel(.8f, .2f, slab_sheave));
    auto structure = m->body("frame", frame, Vec3::sZero(), 0);

    auto entry = m->body("entry", {box(Vec3(1.5f, .3f, 1.5f), Material::Concrete)},
                         Vec3(0, kDeckY - .05f, -(kDeckHalfZ + .3f + 1.5f)), 0);
    auto exit = m->body("exit", {box(Vec3(1.5f, .3f, 1.5f), Material::Concrete)},
                        Vec3(deck_end.GetX(), deck_end.GetY() - .05f, -(kDeckHalfZ + .3f + 1.5f)), 0);

    // The trolley: a level timber deck on a steel shoe under its uphill edge,
    // where the rail runs 0.5 m under the deck, and a strut back from the shoe
    // under its downhill edge. Nothing of it reaches down the rail below the
    // deck, so at the foot it stands clear of the ground.
    const float shoe_y = -drop + std::tan(kAngle) * kDeckHalfX;   // the rail under the uphill edge
    std::vector<Part> trolley = {box(Vec3(kDeckHalfX, .25f, kDeckHalfZ), Material::Timber)};
    {
        auto shoe = box(Vec3(.5f, .15f, .3f), Material::Steel, Vec3(kDeckHalfX - .4f, shoe_y + .1f, 0));
        shoe.rotation = Quat::sRotation(Vec3::sAxisZ(), kAngle);
        trolley.push_back(shoe);
        trolley.push_back(span(Vec3(-kDeckHalfX + .2f, -.25f, 0), Vec3(kDeckHalfX - .4f, shoe_y + .1f, 0), .12f,
                               Material::Steel));
        // A rail round the deck's uphill and far sides, open to the receivers.
        trolley.push_back(box(Vec3(.08f, .5f, kDeckHalfZ), Material::Yellow, Vec3(kDeckHalfX - .08f, .75f, 0)));
        trolley.push_back(box(Vec3(kDeckHalfX, .5f, .08f), Material::Yellow, Vec3(0, .75f, kDeckHalfZ - .08f)));
    }
    auto deck = m->body("deck", trolley, deck_start, 900);
    auto slide = m->slider({}, deck, along, 0, travel);
    // Ascending, only a braking force: the falling slab supplies the work.
    // Sent back down, this motor is the powered reset that winds the slab up.
    slide->GetMotorSettings().SetForceLimits(-80000, 0);

    auto slab = m->body("slab", {
        box(Vec3(1.0f, kSlabHalfY, 1.0f), Material::Concrete),
        box(Vec3(1.1f, .12f, 1.1f), Material::Yellow, Vec3(0, kSlabHalfY, 0))
    }, slab_start, 2500);
    m->slider({}, slab, Vec3::sAxisY(), -travel, 0);

    const Vec3 slab_top(0, kSlabHalfY + .12f, 0);
    const float max_length = (attach_end - head_sheave).Length() + travel +
                             (slab_sheave - (slab_start + slab_top)).Length();
    auto rope = c.kit.add_rope(deck, attach_local, m->point(head_sheave), slab, slab_top, m->point(slab_sheave),
                               1.0f, max_length, 150000);
    m->ropes.push_back(rope);
    m->no_collision(structure, deck);
    m->no_collision(structure, slab);

    Drive drive;
    drive.slider = slide;
    drive.extent = travel;
    drive.speed = 1.2f;
    m->drives.push_back(drive);

    m->walkable("deck", deck, Vec3(0, .25f, 0));
    m->walkable("entry", entry, Vec3(0, .3f, 0));
    m->walkable("exit", exit, Vec3(0, .3f, 0));
    return m;
}
}  // namespace scraperx::sim::vertical
