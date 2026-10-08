#include "vertical_machine.hpp"
#include <cmath>
namespace scraperx::sim::vertical {
std::unique_ptr<Machine> crown_gondola(BuildContext c) {
    auto m=std::make_unique<Machine>(c,"sx.crown_gondola.v1",3,2);
    const float target=DegreesToRadians(178.f),radius=16.5f,axis_y=20.5f;
    m->body("bearing_frame",{
        box(Vec3(.5f,21.5f,.5f),Material::Concrete,Vec3(-3,20.5f,-3.5f)),
        box(Vec3(3,.4f,.4f),Material::Steel,Vec3(-1,20.5f,-3.5f)),
        box(Vec3(7.5f,.3f,.5f),Material::Steel,Vec3(-10.5f,2.6f,-3.5f)),
        box(Vec3(7.5f,.3f,.5f),Material::Steel,Vec3(-10.5f,35.6f,-3.5f)),
        // Rear bearing has an open shaft bore; no static solid through the spokes.
        box(Vec3(.16f,.5f,.5f),Material::Steel,Vec3(-.34f,20.5f,-2.7f)),
        box(Vec3(.16f,.5f,.5f),Material::Steel,Vec3(.34f,20.5f,-2.7f)),
        box(Vec3(.18f,.16f,.5f),Material::Steel,Vec3(0,20.16f,-2.7f)),
        box(Vec3(.18f,.16f,.5f),Material::Steel,Vec3(0,20.84f,-2.7f)),
        box(Vec3(.7f,.7f,.5f),Material::Yellow,Vec3(-3,4.5f,-3.5f))
    },Vec3::sZero(),0);
    auto entry=m->body("entry",{box(Vec3(14.5f,.3f,1.5f),Material::Concrete),box(Vec3(.12f,.55f,.12f),Material::Yellow,Vec3(0,.85f,0)),box(Vec3(.2f,.12f,.12f),Material::Hazard,Vec3(0,1.1f,0))},Vec3(-3.5f,1.95f,4.8f),0);
    const float endx=radius*std::sin(target),endy=axis_y-radius*std::cos(target)-2;
    // Separate structural lips: a passive hanging-arm handoff crosses the
    // missing span. Keep the existing cabin receiver and recall control.
    auto exit=m->body("exit",{
        box(Vec3(5.05f,.3f,1.5f),Material::Concrete,Vec3(1.374144f,0,0)),
        box(Vec3(2.212072f,.3f,.8f),Material::Concrete,Vec3(-13.187928f,0,0)),
        box(Vec3(.12f,.55f,.12f),Material::Yellow,Vec3(-14.337928f,.85f,-.55f)),
        box(Vec3(.2f,.12f,.12f),Material::Hazard,Vec3(-14.337928f,1.1f,-.55f)),
        box(Vec3(.12f,.55f,.12f),Material::Yellow,Vec3(0,.85f,0)),
        box(Vec3(.2f,.12f,.12f),Material::Hazard,Vec3(0,1.1f,0))},Vec3(endx-3.5f,endy-.05f,4.8f),0);
    std::vector<Part> rotor;
    rotor.push_back(wheel(.12f,.8f,Vec3(0,0,-.6f)));
    rotor.push_back(wheel(.12f,.5f,Vec3(0,-radius,.5f))); // Visible hanger pin across the rim/cabin offset.
    for(int i=0;i<64;++i){const float a=2*JPH_PI*i/64;auto p=box(Vec3(radius*std::sin(JPH_PI/64)+.06f,.22f,.35f),Material::Steel,Vec3(radius*std::cos(a),radius*std::sin(a),0));p.rotation=Quat::sRotation(Vec3::sAxisZ(),a+JPH_PI/2);rotor.push_back(p);}
    // Full diametral spokes, not four half-length arms.
    for(int i=0;i<2;++i){auto p=box(Vec3(radius,.22f,.28f),Material::Rust);p.rotation=Quat::sRotation(Vec3::sAxisZ(),i*JPH_PI/2);rotor.push_back(p);}
    auto wheel=m->body("crown",rotor,Vec3(0,axis_y,-1.8f),7000);
    auto h=m->hinge({},wheel,Vec3(0,axis_y,-1.8f),DegreesToRadians(-2.f),DegreesToRadians(179.f));
    h->GetMotorSettings().SetTorqueLimit(400000);
    auto deck=m->body("deck",{
        box(Vec3(3,.25f,2),Material::Timber),
        box(Vec3(.12f,1,.12f),Material::Steel,Vec3(-2.8f,1,-1.8f)),
        box(Vec3(.12f,1,.12f),Material::Steel,Vec3(2.8f,1,-1.8f)),
        box(Vec3(2.8f,.12f,.12f),Material::Steel,Vec3(0,2,-1.8f)),
        box(Vec3(1,.3f,1),Material::Concrete,Vec3(0,-.6f,0)),
        box(Vec3(.12f,.55f,.12f),Material::Yellow,Vec3(0,.85f,-1.2f)),
        box(Vec3(.2f,.12f,.12f),Material::Hazard,Vec3(0,1.15f,-1.2f))
    },Vec3(0,2,1),1200);
    auto hanger=m->hinge(wheel,deck,Vec3(0,4,-.8f));hanger->SetMaxFrictionTorque(100);c.kit.set_damping(deck,0,.3f);
    m->no_collision(wheel,deck);
    Drive drive;drive.hinge=h;drive.extent=target;drive.speed=.13f;drive.acceleration=.03f;m->drives.push_back(drive);
    m->walkable("deck",deck,Vec3(0,.25f,0));m->walkable("entry",entry,Vec3(0,.3f,0));m->walkable("exit",exit,Vec3(0,.3f,0));
    m->ports.push_back({"upper_recall",exit,Vec3(-14.337928f,1.1f,-.55f),false});
    return m;
}
}
