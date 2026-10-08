#include "vertical_machine.hpp"

namespace scraperx::sim::vertical {
std::unique_ptr<Machine> cascade_mast(BuildContext c) {
    constexpr float kStage1Travel = 11.0f;
    constexpr float kStage2Travel = 22.0f;
    constexpr float kDeckTravel = 33.0f;
    constexpr float kBackZ = -2.8f;

    auto m = std::make_unique<Machine>(c, "sx.cascade_mast.v1", 3, 3);
    m->body("frame", {
        box(Vec3(.45f, 21.5f, .45f), Material::Concrete, Vec3(-5.2f, 20.5f, kBackZ)),
        box(Vec3(.45f, 21.5f, .45f), Material::Concrete, Vec3( 5.2f, 20.5f, kBackZ)),
        box(Vec3(5.7f, .35f, .45f), Material::Steel, Vec3(0, 42.0f, kBackZ)),
        box(Vec3(5.7f, .35f, .45f), Material::Steel, Vec3(0, .5f, kBackZ)),
        box(Vec3(.2f,1.1f,.25f),Material::Steel,Vec3(-3.65f,40.9f,kBackZ)),
        box(Vec3(.2f,1.1f,.25f),Material::Steel,Vec3(3.65f,40.9f,kBackZ)),
        wheel(.35f,.2f,Vec3(-3.65f,40,kBackZ)),wheel(.35f,.2f,Vec3(3.65f,40,kBackZ)),
        wheel(.35f,.2f,Vec3(-3.65f,0,kBackZ)),wheel(.35f,.2f,Vec3(3.65f,0,kBackZ)),
        box(Vec3(.2f,.2f,.25f),Material::Steel,Vec3(-3.65f,.1f,kBackZ)),
        box(Vec3(.2f,.2f,.25f),Material::Steel,Vec3(3.65f,.1f,kBackZ)),
        box(Vec3(1.5f,.3f,.4f),Material::Steel,Vec3(6,3,kBackZ)),
        box(Vec3(1.5f,.3f,.4f),Material::Steel,Vec3(6,36,kBackZ))
    }, Vec3::sZero(), 0);
    auto entry = m->body("entry", {box(Vec3(1.5f, .3f, 2.0f), Material::Concrete),box(Vec3(.12f,.55f,.12f),Material::Yellow,Vec3(0,.85f,0)),box(Vec3(.2f,.12f,.12f),Material::Hazard,Vec3(0,1.1f,0))},
                         Vec3(5.3f, 3.0f, 0), 0);
    auto exit = m->body("exit", {box(Vec3(1.5f, .3f, 2.0f), Material::Concrete),box(Vec3(.12f,.55f,.12f),Material::Yellow,Vec3(0,.85f,0)),box(Vec3(.2f,.12f,.12f),Material::Hazard,Vec3(0,1.1f,0))},
                        Vec3(5.3f, 36.0f, 0), 0);

    auto stage1 = m->body("outer_stage", {
        box(Vec3(.25f, 9.0f, .3f), Material::Steel, Vec3(-3.0f, 0, 0)),
        box(Vec3(.25f, 9.0f, .3f), Material::Steel, Vec3( 3.0f, 0, 0)),
        box(Vec3(3.25f, .25f, .35f), Material::Yellow, Vec3(0, 8.75f, 0)),
        box(Vec3(3.25f, .25f, .35f), Material::Steel, Vec3(0, -8.75f, 0)),
        box(Vec3(.4f,.12f,.3f),Material::Steel,Vec3(-3.4f,0,0))
    }, Vec3(0, 10.0f, kBackZ), 900);
    auto stage2 = m->body("inner_stage", {
        box(Vec3(.22f, 9.0f, .26f), Material::Galvanised, Vec3(-2.15f, 0, 0)),
        box(Vec3(.22f, 9.0f, .26f), Material::Galvanised, Vec3( 2.15f, 0, 0)),
        box(Vec3(2.4f, .22f, .3f), Material::Yellow, Vec3(0, 8.75f, 0)),
        box(Vec3(2.4f, .22f, .3f), Material::Steel, Vec3(0, -8.75f, 0)),
        box(Vec3(1.1f,.12f,.3f),Material::Steel,Vec3(-2.6f,0,0)),
        box(Vec3(1.1f,.12f,.3f),Material::Steel,Vec3(2.6f,0,0))
    }, Vec3(0, 10.0f, kBackZ), 750);
    auto deck = m->body("deck", {
        box(Vec3(3.5f, .3f, 2.0f), Material::Timber),
        box(Vec3(.22f, 2.0f, .22f), Material::Steel, Vec3(-2.0f, 2.0f, kBackZ)),
        box(Vec3(.22f, 2.0f, .22f), Material::Steel, Vec3( 2.0f, 2.0f, kBackZ)),
        box(Vec3(2.2f, .2f, .25f), Material::Yellow, Vec3(0, 4.0f, kBackZ)),
        box(Vec3(3.9f,.15f,.25f),Material::Steel,Vec3(0,0,kBackZ)),
        box(Vec3(.12f,.55f,.12f),Material::Yellow,Vec3(0,.85f,-1.2f)),
        box(Vec3(.2f,.12f,.12f),Material::Hazard,Vec3(0,1.15f,-1.2f))
    }, Vec3(0, 3.0f, 0), 1200);

    auto drive_slider = m->slider({}, stage1, Vec3::sAxisY(), 0, kStage1Travel);
    // Equivalent nominal input load is about 61 kN after the 1:2:3
    // displacement cascade; 3,000 kg on the deck exceeds this finite rating.
    drive_slider->GetMotorSettings().SetForceLimit(110000);
    m->slider({}, stage2, Vec3::sAxisY(), 0, kStage2Travel);
    m->slider({}, deck, Vec3::sAxisY(), 0, kDeckTravel);

    // A driven lower rope leg lengthens as stage 1 rises. The tension-only
    // upper leg must shorten, so stage 2 rises at twice stage-1 travel.
    auto first = c.kit.add_rope(
        stage1, Vec3(-3.65f, 0, 0), m->point(Vec3(-3.65f, 0, kBackZ)),
        stage2, Vec3(-3.65f, 0, 0), m->point(Vec3(-3.65f, 40.0f, kBackZ)),
        .5f, 25.0f, 180000);
    // Stage 2 repeats the arrangement at a 2:3 ratio, producing 33 m of
    // deck travel from its 22 m rise. No deck-position motor exists.
    auto second = c.kit.add_rope(
        stage2, Vec3(3.65f, 0, 0), m->point(Vec3(3.65f, 0, kBackZ)),
        deck, Vec3(3.65f, 0, kBackZ), m->point(Vec3(3.65f, 40.0f, kBackZ)),
        2.0f / 3.0f, 34.6666667f, 180000);
    m->ropes.push_back(first);
    m->ropes.push_back(second);

    m->no_collision(stage1, stage2);
    m->no_collision(stage1, deck);
    m->no_collision(stage2, deck);

    Drive drive;
    drive.slider = drive_slider;
    drive.extent = kStage1Travel;
    drive.speed = .40f;drive.acceleration=.12f;
    m->drives.push_back(drive);

    m->walkable("deck", deck, Vec3(0, .3f, 0));
    m->walkable("entry", entry, Vec3(0, .3f, 0));
    m->walkable("exit", exit, Vec3(0, .3f, 0));
    return m;
}
} // namespace scraperx::sim::vertical
