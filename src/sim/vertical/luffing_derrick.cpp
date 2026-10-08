#include "sim/vertical/vertical_machine.hpp"
#include <cmath>

namespace scraperx::sim::vertical {
std::unique_ptr<Machine> luffing_derrick(BuildContext c) {
    constexpr float kLength = 32.0f;
    constexpr float kStart = DegreesToRadians(8.0f);
    constexpr float kEnd = DegreesToRadians(65.0f);
    constexpr float kBackZ = -2.5f;
    const float delta = kEnd - kStart;
    const Vec3 pivot(0, 4.0f, kBackZ);
    const Vec3 initial_tip(pivot.GetX() + kLength * std::cos(kStart),
                           pivot.GetY() + kLength * std::sin(kStart), kBackZ);
    const Vec3 final_tip(pivot.GetX() + kLength * std::cos(kEnd),
                         pivot.GetY() + kLength * std::sin(kEnd), kBackZ);
    const Vec3 head_sheave(0, 40.0f, kBackZ);
    const Vec3 ballast_sheave(-6.0f, 40.0f, kBackZ);
    const float first_leg_start = (initial_tip - head_sheave).Length();
    const float first_leg_end = (final_tip - head_sheave).Length();
    const float ballast_travel = first_leg_start - first_leg_end;

    auto m = std::make_unique<Machine>(c, "sx.luffing_derrick.v1", 3, 3);
    auto frame = m->body("frame", {
        box(Vec3(.55f, 20.0f, .55f), Material::Concrete, Vec3(0, 20.0f, kBackZ)),
        box(Vec3(3.8f, .4f, .5f), Material::Steel, Vec3(-3.0f, 40.0f, kBackZ)),
        wheel(.9f, .35f, Vec3(0, 40.0f, kBackZ)),
        wheel(.9f, .35f, Vec3(-6.0f, 40.0f, kBackZ)),
        box(Vec3(.35f, 18.0f, .35f), Material::Galvanised, Vec3(-6.0f, 20.0f, kBackZ))
    }, Vec3::sZero(), 0);
    const float initial_deck_y = initial_tip.GetY() - 2.0f;
    const float final_deck_y = final_tip.GetY() - 2.0f;
    auto entry = m->body("entry", {box(Vec3(1.5f, .3f, 2.0f), Material::Concrete)},
                         Vec3(initial_tip.GetX() + 4.8f, initial_deck_y - .05f, 0), 0);
    auto exit = m->body("exit", {box(Vec3(1.5f, .3f, 2.0f), Material::Concrete)},
                        Vec3(final_tip.GetX() - 4.8f, final_deck_y - .05f, 0), 0);

    const Vec3 boom_center = pivot + Vec3(.5f * kLength * std::cos(kStart),
                                          .5f * kLength * std::sin(kStart), 0);
    auto boom = m->body("boom", {
        box(Vec3(kLength * .5f, .38f, .42f), Material::Steel),
        box(Vec3(kLength * .5f, .16f, .28f), Material::Yellow, Vec3(0, .85f, 0)),
        box(Vec3(.35f, 1.2f, .35f), Material::Steel, Vec3(kLength * .5f - .35f, .55f, 0))
    }, boom_center, 2500, kStart);
    auto boom_hinge = m->hinge({}, boom, pivot, 0, delta);
    boom_hinge->SetMaxFrictionTorque(3000);
    // The boom shares its pivot volume with the mast; the hinge, not
    // self-collision against the bearing housing, owns that relationship.
    m->no_collision(frame, boom);

    auto deck = m->body("deck", {
        box(Vec3(3.0f, .25f, 2.0f), Material::Timber),
        box(Vec3(.18f, 1.0f, .18f), Material::Steel, Vec3(-2.8f, 1.0f, -1.8f)),
        box(Vec3(.18f, 1.0f, .18f), Material::Steel, Vec3( 2.8f, 1.0f, -1.8f)),
        box(Vec3(2.8f, .15f, .15f), Material::Steel, Vec3(0, 2.0f, -1.8f)),
        box(Vec3(.25f, 1.1f, 1.25f), Material::Steel, Vec3(0, 1.0f, -1.25f))
    }, Vec3(initial_tip.GetX(), initial_deck_y, 0), 1200);
    auto hanger = m->hinge(boom, deck, initial_tip);
    hanger->SetMaxFrictionTorque(120);
    c.kit.set_damping(deck, 0, .55f);
    c.kit.set_damping(boom, 0, .03f);
    m->no_collision(boom, deck);

    auto ballast = m->body("ballast", {
        box(Vec3(1.2f, 1.5f, 1.2f), Material::Concrete),
        box(Vec3(1.45f, .18f, 1.45f), Material::Yellow, Vec3(0, 1.5f, 0))
    }, Vec3(-6.0f, 36.5f, kBackZ), 1200);
    auto ballast_slide = m->slider({}, ballast, Vec3::sAxisY(), -ballast_travel, 0);
    // The slider is the authoritative guide. The visible guide rail is not a
    // second, competing contact constraint.
    m->no_collision(frame, ballast);
    // 50 kN plus ballast gravity can luff the occupied cradle, while the
    // declared 3,000 kg overload cannot break the boom away from its stop.
    ballast_slide->GetMotorSettings().SetForceLimit(50000);

    const float ballast_initial_leg = 2.0f;
    auto rope = c.kit.add_rope(
        boom, Vec3(kLength * .5f, 0, 0), m->point(head_sheave),
        ballast, Vec3(0, 1.5f, 0), m->point(ballast_sheave),
        1.0f, first_leg_start + ballast_initial_leg, 150000);
    m->ropes.push_back(rope);

    Drive drive;
    drive.slider = ballast_slide;
    drive.extent = -ballast_travel;
    drive.speed = .85f;
    m->drives.push_back(drive);

    m->walkable("deck", deck, Vec3(0, .25f, 0));
    m->walkable("entry", entry, Vec3(0, .3f, 0));
    m->walkable("exit", exit, Vec3(0, .3f, 0));
    return m;
}
} // namespace scraperx::sim::vertical
