#include "sim/vertical/vertical_machine.hpp"
#include <Jolt/Physics/Constraints/RackAndPinionConstraint.h>
#include <cmath>
namespace scraperx::sim::vertical {
std::unique_ptr<Machine> rack_climber(BuildContext c) {
    auto m=std::make_unique<Machine>(c,"sx.rack_climber.v1",3,2);
    m->body("station",{
        box(Vec3(.4f,33,.4f),Material::Concrete,Vec3(-9,29,-2.8f)),
        box(Vec3(3,.4f,.4f),Material::Steel,Vec3(-6,34,-2.8f)),
        box(Vec3(.25f,34,.25f),Material::Galvanised,Vec3(7,29,-2.8f))
    },Vec3::sZero(),0);
    auto entry=m->body("entry",{box(Vec3(1.5f,.3f,2),Material::Concrete)},Vec3(7.8f,2.95f,0),0);
    auto exit=m->body("exit",{box(Vec3(1.5f,.3f,2),Material::Concrete)},Vec3(7.8f,30.95f,0),0);
    std::vector<Part> rack={box(Vec3(.3f,21,.45f),Material::Steel,Vec3(0,14,0)),box(Vec3(3,.25f,2),Material::Timber,Vec3(3,0,0))};
    for(int i=0;i<53;++i)rack.push_back(box(Vec3(.22f,.12f,.6f),Material::Yellow,Vec3(-.4f,-6.4f+.785398f*i,0)));
    auto carriage=m->body("deck",rack,Vec3(0,3,0),1800);
    auto slide=m->slider({},carriage,Vec3::sAxisY(),0,28);
    std::vector<Part> gear={wheel(3.75f,.65f)};
    for(int i=0;i<32;++i){float a=2*JPH_PI*i/32;auto p=box(Vec3(.25f,.26f,.65f),Material::Yellow,Vec3(4*std::cos(a),4*std::sin(a),0));p.rotation=Quat::sRotation(Vec3::sAxisZ(),a);gear.push_back(p);}
    auto pinion=m->body("pinion",gear,Vec3(-4.3f,34,0),1600);
    auto hinge=m->hinge({},pinion,Vec3(-4.3f,34,0));
    hinge->GetMotorSettings().SetTorqueLimit(100000);
    RackAndPinionConstraintSettings s;s.mHingeAxis=m->direction(Vec3::sAxisZ());s.mSliderAxis=m->direction(Vec3::sAxisY());s.mRatio=.25f;
    s.mNumVelocityStepsOverride=40;s.mNumPositionStepsOverride=8;
    Ref<RackAndPinionConstraint> transmission=static_cast<RackAndPinionConstraint*>(s.Create(m->native(pinion),m->native(carriage)));
    transmission->SetConstraints(hinge,slide);m->own(transmission.GetPtr());m->transmissions.push_back(transmission.GetPtr());
    // Ideal tooth engagement owns transfer; colliding tooth meshes would add
    // a second, inconsistent transmission to the same degree of freedom.
    m->no_collision(pinion,carriage);
    Drive drive;drive.hinge=hinge;drive.translation_feedback=slide;drive.radians_per_metre=.25f;drive.extent=28;drive.speed=.4f;
    m->drives.push_back(drive);
    m->walkable("deck",carriage,Vec3(3,.25f,0));m->walkable("entry",entry,Vec3(0,.3f,0));m->walkable("exit",exit,Vec3(0,.3f,0));
    return m;
}
}
