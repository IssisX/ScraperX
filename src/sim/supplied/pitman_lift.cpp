#include "vertical_machine.hpp"
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <cmath>

namespace scraperx::sim::vertical {
namespace {
Ref<PointConstraint> point_joint(Machine &m, BodyIndex first, BodyIndex second, Vec3 world_point) {
    PointConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mPoint1 = settings.mPoint2 = m.point(world_point);
    settings.mNumVelocityStepsOverride = 40;
    settings.mNumPositionStepsOverride = 8;
    Ref<PointConstraint> result = static_cast<PointConstraint *>(settings.Create(m.native(first), m.native(second)));
    m.own(result.GetPtr());
    return result;
}
}

std::unique_ptr<Machine> pitman_lift(BuildContext c) {
    constexpr float kRadius = 12.0f;
    constexpr float kRodLength = 18.0f;
    const float kStart = -std::asin(22.f / (2*kRadius));
    const float kEnd = -kStart;
    constexpr float kDeckStartY = 3.0f;
    constexpr float kBackZ = -2.5f;
    const float horizontal = kRadius * std::cos(kStart);
    const float vertical_rod = std::sqrt(kRodLength * kRodLength - horizontal * horizontal);
    const float centre_y = kDeckStartY - kRadius * std::sin(kStart) + vertical_rod;
    constexpr float kCrankZ=-4.2f;
    const Vec3 crank_center(0, centre_y, kCrankZ);
    const Vec3 pin_start(kRadius * std::cos(kStart),
                         centre_y + kRadius * std::sin(kStart), kBackZ);
    const Vec3 deck_joint_start(0, kDeckStartY, kBackZ);
    const float deck_end_y = centre_y + kRadius * std::sin(kEnd) - vertical_rod;
    const float deck_travel = deck_end_y - kDeckStartY;
    const Vec3 rod_vector = deck_joint_start - pin_start;
    const float rod_angle = std::atan2(rod_vector.GetY(), rod_vector.GetX());
    const Vec3 rod_center = .5f * (pin_start + deck_joint_start);
    const float target = kEnd - kStart;

    auto m = std::make_unique<Machine>(c, "sx.pitman_lift.v1", 3, 3);
    m->body("frame", {
        box(Vec3(.55f, 13.5f, .55f), Material::Concrete, Vec3(-14.0f, centre_y, kCrankZ)),
        box(Vec3(.55f, 13.5f, .55f), Material::Concrete, Vec3( 14.0f, centre_y, kCrankZ)),
        box(Vec3(14.5f, .45f, .55f), Material::Steel, Vec3(0, centre_y + 13.0f, kCrankZ)),
        // Rear bearing stays behind both the wheel and connecting rod.
        box(Vec3(6.95f,.3f,.35f),Material::Steel,Vec3(-7.55f,centre_y,-6.3f)),
        box(Vec3(6.95f,.3f,.35f),Material::Steel,Vec3(7.55f,centre_y,-6.3f)),
        box(Vec3(.2f,.7f,.5f),Material::Steel,Vec3(-.5f,centre_y,-6.3f)),
        box(Vec3(.2f,.7f,.5f),Material::Steel,Vec3(.5f,centre_y,-6.3f)),
        box(Vec3(.3f,.2f,.5f),Material::Steel,Vec3(0,centre_y-.5f,-6.3f)),
        box(Vec3(.3f,.2f,.5f),Material::Steel,Vec3(0,centre_y+.5f,-6.3f)),
        box(Vec3(.4f,.3f,1.3f),Material::Steel,Vec3(-14,centre_y,-5.25f)),
        box(Vec3(.4f,.3f,1.3f),Material::Steel,Vec3(14,centre_y,-5.25f)),
        box(Vec3(1.4f,.3f,2.1f),Material::Steel,Vec3(15.3f,deck_end_y,-2.1f)),
        box(Vec3(.2f,13.5f,.2f),Material::Steel,Vec3(-3.8f,14,2.5f)),
        box(Vec3(.2f,13.5f,.2f),Material::Steel,Vec3(3.8f,14,2.5f)),
        box(Vec3(9.1f,.25f,.25f),Material::Steel,Vec3(5.1f,27.7f,2.5f)),
        box(Vec3(.25f,.25f,3.35f),Material::Steel,Vec3(14,27.7f,-.85f))
    }, Vec3::sZero(), 0);
    auto entry = m->body("entry", {box(Vec3(6.45f, .3f, 2.0f), Material::Concrete),box(Vec3(.12f,.55f,.12f),Material::Yellow,Vec3(0,.85f,0)),box(Vec3(.2f,.12f,.12f),Material::Hazard,Vec3(0,1.1f,0))},
                         Vec3(10.25f, kDeckStartY, 0), 0);
    auto exit = m->body("exit", {box(Vec3(6.45f, .3f, 2.0f), Material::Concrete),box(Vec3(.12f,.55f,.12f),Material::Yellow,Vec3(0,.85f,0)),box(Vec3(.2f,.12f,.12f),Material::Hazard,Vec3(0,1.1f,0))},
                        Vec3(10.25f, deck_end_y, 0), 0);

    std::vector<Part> crank_parts;
    for (int i = 0; i < 56; ++i) {
        const float angle = 2.0f * JPH_PI * i / 56.0f;
        auto rim = box(Vec3(.72f, .22f, .35f), Material::Steel,
                       Vec3(kRadius * std::cos(angle), kRadius * std::sin(angle), 0));
        rim.rotation = Quat::sRotation(Vec3::sAxisZ(), angle + JPH_PI * .5f);
        crank_parts.push_back(rim);
    }
    for (int i = 0; i < 4; ++i) {
        auto spoke = box(Vec3(kRadius, .18f, .28f), i % 2 ? Material::Rust : Material::Yellow);
        spoke.rotation = Quat::sRotation(Vec3::sAxisZ(), i * JPH_PI * .25f);
        crank_parts.push_back(spoke);
    }
    crank_parts.push_back(wheel(.65f, .95f, Vec3(kRadius, 0, .85f)));
    crank_parts.push_back(wheel(.15f,1.15f,Vec3(0,0,-1.05f)));
    auto crank = m->body("crank", crank_parts, crank_center, 5200, kStart);
    auto crank_hinge = m->hinge({}, crank, crank_center, -.02f, target + .02f);
    // The nominal trace peaks near 310 kNm. A 600 kNm finite drive closes
    // the full stroke and still stalls well short under the 3,000 kg case.
    crank_hinge->GetMotorSettings().SetTorqueLimit(600000);

    std::vector<Part> deck_parts = {
        box(Vec3(3.5f, .3f, 2.0f), Material::Timber),
        box(Vec3(.3f, .3f, 1.25f), Material::Steel, Vec3(0, 0, -1.25f)),
        // Shared journal, not a tall post cutting through the moving rod.
        wheel(.18f,.6f,Vec3(0,0,-2.25f)),
        box(Vec3(.12f,.55f,.12f),Material::Yellow,Vec3(0,.85f,-1.2f)),
        box(Vec3(.2f,.12f,.12f),Material::Hazard,Vec3(0,1.15f,-1.2f))
    };
    // Open carriage collars match the native passive linear bearing.
    for(float x:{-3.8f,3.8f}){
        deck_parts.push_back(box(Vec3(.08f,.25f,.43f),Material::Steel,Vec3(x-.35f,0,2.5f)));
        deck_parts.push_back(box(Vec3(.08f,.25f,.43f),Material::Steel,Vec3(x+.35f,0,2.5f)));
        deck_parts.push_back(box(Vec3(.27f,.25f,.08f),Material::Steel,Vec3(x,0,2.15f)));
        deck_parts.push_back(box(Vec3(.27f,.25f,.08f),Material::Steel,Vec3(x,0,2.85f)));
        // L-shaped ties connect the floor to the open collar beyond the
        // receiver edge. The previous diagonal corner overlapped entry's
        // X>=3.8/Z<=2 floor and wedged the carriage against both guide posts.
        deck_parts.push_back(box(Vec3(.2f,.2f,.35f),Material::Steel,Vec3(std::copysign(3.15f,x),0,2.05f)));
        deck_parts.push_back(box(Vec3(.55f,.2f,.08f),Material::Steel,Vec3(std::copysign(3.45f,x),0,2.15f)));
    }
    auto deck=m->body("deck",deck_parts,Vec3(0,kDeckStartY,0),1200);
    auto deck_slider = m->slider({}, deck, Vec3::sAxisY(), 0, deck_travel);

    auto pitman = m->body("pitman", {
        box(Vec3(kRodLength * .5f, .28f, .32f), Material::Yellow),
        wheel(.55f, .36f, Vec3(-kRodLength * .5f, 0, 0)),
        wheel(.55f, .36f, Vec3( kRodLength * .5f, 0, 0))
    }, rod_center, 850, rod_angle);
    auto crank_pin = point_joint(*m, crank, pitman, pin_start);
    auto deck_pin = point_joint(*m, pitman, deck, deck_joint_start);
    m->transmissions.push_back(crank_pin.GetPtr());
    m->transmissions.push_back(deck_pin.GetPtr());
    m->no_collision(crank, pitman);
    m->no_collision(pitman, deck);
    m->no_collision(crank, deck);

    Drive drive;
    drive.hinge = crank_hinge;
    drive.translation_feedback = deck_slider;
    drive.radians_per_metre = target / deck_travel;
    drive.extent = deck_travel;
    drive.speed = .13f;
    drive.acceleration = .035f;
    m->drives.push_back(drive);

    m->walkable("deck", deck, Vec3(0, .3f, 0));
    m->walkable("entry", entry, Vec3(0, .3f, 0));
    m->walkable("exit", exit, Vec3(0, .3f, 0));
    return m;
}
} // namespace scraperx::sim::vertical
