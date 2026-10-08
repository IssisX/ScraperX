#include "sim/vertical/vertical_machine.hpp"
#include <cmath>

// The owner's drawing (2026-10-08): a concrete slab falls in a timber tower;
// its rope over the tower's head sheave hauls a trolley 30 m up a 60-degree
// incline. Local frame: the incline rises toward +x; the slab tower stands at
// its head. The trolley's deck stays level on a raked chassis that rides the
// rail 2.9 m below it. The rider boards from the -z side at the foot and
// steps off to the -z side at the head.
namespace scraperx::sim::vertical {
std::unique_ptr<Machine> slab_incline(BuildContext c) {
    constexpr float kRise = 30.0f;
    constexpr float kAngle = DegreesToRadians(60.0f);
    const float travel = kRise / std::sin(kAngle);               // 34.64 m along the rail
    const Vec3 along(std::cos(kAngle), std::sin(kAngle), 0);     // up the incline
    constexpr float kDeckHalfX = 1.4f, kDeckHalfZ = 1.6f, kDeckY = 3.05f;
    // The rail's centre line passes this far under the deck's centre, so the
    // deck's uphill underside corner clears it.
    const float drop = .25f + std::tan(kAngle) * kDeckHalfX + .15f + .1f;
    const Vec3 deck_start(0, kDeckY, 0);
    const Vec3 deck_end = deck_start + along * travel;
    const Vec3 rail_start = deck_start - Vec3(0, drop, 0) - along * 1.5f;
    const Vec3 rail_end = deck_end - Vec3(0, drop, 0) + along * 1.0f;
    const float rail_length = (rail_end - rail_start).Length();
    const Vec3 rail_mid = .5f * (rail_start + rail_end);

    // The slab's tower at the incline's head, its sheave over the slab.
    const float slab_x = deck_end.GetX() + 6.2f;
    const Vec3 attach_local(kDeckHalfX, -2.0f, 0);   // the chassis' nose, on the deck
    const Vec3 attach_end = deck_end + attach_local;
    const Vec3 head_sheave = attach_end + along * 2.5f;
    const Vec3 slab_sheave(slab_x, kDeckY + kRise + 7.0f, 0);
    constexpr float kSlabHalfY = 1.5f;
    const Vec3 slab_start(slab_x, slab_sheave.GetY() - 2.0f - kSlabHalfY, 0);

    auto m = std::make_unique<Machine>(c, "sx.slab_incline.v1", 3, 2);

    std::vector<Part> frame;
    // Two rails, either side of the chassis' keel.
    for (const float z : {-.45f, .45f}) {
        auto rail = box(Vec3(.5f * rail_length, .15f, .12f), Material::Timber, rail_mid + Vec3(0, 0, z));
        rail.rotation = Quat::sRotation(Vec3::sAxisZ(), kAngle);
        frame.push_back(rail);
    }
    // Trestles from the ground (local y 0) up under the rails.
    for (float s = 4.0f; s < travel; s += 6.0f) {
        const Vec3 p = rail_start + along * s;
        const float h = p.GetY() - .15f;
        if (h > .5f) {
            frame.push_back(box(Vec3(.18f, .5f * h, .18f), Material::Timber, Vec3(p.GetX(), .5f * h, -.8f)));
            frame.push_back(box(Vec3(.18f, .5f * h, .18f), Material::Timber, Vec3(p.GetX(), .5f * h, .8f)));
            frame.push_back(box(Vec3(.12f, .12f, .9f), Material::Timber, Vec3(p.GetX(), h - .1f, 0)));
        }
    }
    // The tower: four posts round the slab, a head beam, the sheaves.
    const float tower_top = slab_sheave.GetY() + .9f;
    for (const float dx : {-1.6f, 1.6f}) {
        for (const float dz : {-1.6f, 1.6f}) {
            frame.push_back(box(Vec3(.25f, .5f * tower_top, .25f), Material::Steel,
                                Vec3(slab_x + dx, .5f * tower_top, dz)));
        }
    }
    frame.push_back(box(Vec3(1.9f, .3f, 1.9f), Material::Steel, Vec3(slab_x, tower_top, 0)));
    frame.push_back(box(Vec3(.5f * (slab_x - head_sheave.GetX()) + .5f, .25f, .3f), Material::Steel,
                        Vec3(.5f * (slab_x + head_sheave.GetX()), head_sheave.GetY() + .9f, 0)));
    frame.push_back(box(Vec3(.25f, .5f * (head_sheave.GetY() + .9f), .25f), Material::Steel,
                        Vec3(head_sheave.GetX(), .5f * (head_sheave.GetY() + .9f), -.9f)));
    frame.push_back(wheel(.8f, .2f, head_sheave));
    frame.push_back(wheel(.8f, .2f, slab_sheave));
    auto structure = m->body("frame", frame, Vec3::sZero(), 0);

    auto entry = m->body("entry", {box(Vec3(1.5f, .3f, 1.5f), Material::Concrete)},
                         Vec3(0, kDeckY - .05f, -(kDeckHalfZ + .3f + 1.5f)), 0);
    auto exit = m->body("exit", {box(Vec3(1.5f, .3f, 1.5f), Material::Concrete)},
                        Vec3(deck_end.GetX(), deck_end.GetY() - .05f, -(kDeckHalfZ + .3f + 1.5f)), 0);

    // The trolley: a level timber deck on a raked steel chassis.
    std::vector<Part> trolley = {box(Vec3(kDeckHalfX, .25f, kDeckHalfZ), Material::Timber)};
    {
        auto keel = box(Vec3(.5f * (2.0f * kDeckHalfX / std::cos(kAngle)), .15f, .3f), Material::Steel,
                        Vec3(0, -drop + .3f, 0));
        keel.rotation = Quat::sRotation(Vec3::sAxisZ(), kAngle);
        trolley.push_back(keel);
        const float front = drop - .25f;
        trolley.push_back(box(Vec3(.15f, .5f * front, .3f), Material::Steel, Vec3(kDeckHalfX - .2f, -.25f - .5f * front, 0)));
        trolley.push_back(box(Vec3(.15f, .3f, .3f), Material::Steel, Vec3(-kDeckHalfX + .2f, -.55f, 0)));
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
