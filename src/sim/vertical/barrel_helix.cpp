#include "sim/vertical/vertical_machine.hpp"
#include <cmath>
namespace scraperx::sim::vertical {
std::unique_ptr<Machine> barrel_helix(BuildContext c) {
    auto m=std::make_unique<Machine>(c,"sx.barrel_helix.v1",3,3);
    m->body("frame",{
        box(Vec3(.4f,17,.4f),Material::Concrete,Vec3(12.5f,16,-2.4f)),
        box(Vec3(7,.4f,.4f),Material::Steel,Vec3(5,31,-2.4f))
    },Vec3::sZero(),0);
    auto entry=m->body("entry",{box(Vec3(1.5f,.3f,2),Material::Concrete)},Vec3(13.8f,4.25f,0),0);
    auto exit=m->body("exit",{box(Vec3(1.5f,.3f,2),Material::Concrete)},Vec3(13.8f,28.25f,0),0);
    std::vector<Part> flight;
    Part shaft=box(Vec3(.45f,16,.45f),Material::Steel,Vec3(0,15,0));shaft.shape=Part::Shape::Cylinder;flight.push_back(shaft);
    const float k=24/(2*JPH_PI),radius=6,start=-.3f,end=2*JPH_PI+.3f;
    const int n=112;const float delta=(end-start)/n,tilt=std::atan(k/radius);
    for(int i=0;i<n;++i){
        const float a=start+(i+.5f)*delta;
        const float length=std::sqrt(std::pow(2*radius*std::sin(delta/2),2)+std::pow(k*delta,2));
        auto p=box(Vec3(length/2+.025f,.12f,1.0f),Material::Yellow,Vec3(radius*std::cos(a),3+k*a,radius*std::sin(a)));
        p.rotation=Quat::sRotation(Vec3::sAxisY(),-a-JPH_PI/2)*Quat::sRotation(Vec3::sAxisZ(),tilt);flight.push_back(p);
    }
    auto barrel=m->body("helix",flight,Vec3::sZero(),3500);
    auto h=m->axis_hinge({},barrel,Vec3(0,15,0),Vec3::sAxisY());h->GetMotorSettings().SetTorqueLimit(100000);
    auto deck=m->body("deck",{
        box(Vec3(2,.25f,1.8f),Material::Timber),
        box(Vec3(2,.15f,.25f),Material::Steel,Vec3(-2,-.3f,0))
    },Vec3(10,4.3f,0),1100);
    c.world.GetBodyInterface().SetFriction(c.kit.body_id(barrel),.3f);
    auto roller_part=box(Vec3(.45f,.3f,.45f),Material::Steel);
    roller_part.shape=Part::Shape::Cylinder;
    roller_part.rotation=Quat::sRotation(Vec3::sAxisZ(),-JPH_PI/2);
    auto roller=m->body("follower_roller",{roller_part},Vec3(6,3.45f,0),90);
    m->axis_hinge(deck,roller,Vec3(6,3.45f,0),Vec3::sAxisX());
    m->no_collision(deck,roller);
    // Only the rolling follower transfers cam force into the carriage.
    // The simplified bearing arm must not become a second sliding follower.
    m->no_collision(deck,barrel);
    c.world.GetBodyInterface().SetFriction(c.kit.body_id(roller),.3f);
    auto guide=m->slider({},deck,Vec3::sAxisY(),0,24);
    Drive drive;drive.hinge=h;drive.translation_feedback=guide;drive.radians_per_metre=2*JPH_PI/24;drive.extent=24;drive.speed=.18f;m->drives.push_back(drive);
    m->walkable("deck",deck,Vec3(0,.25f,0));m->walkable("entry",entry,Vec3(0,.3f,0));m->walkable("exit",exit,Vec3(0,.3f,0));
    return m;
}
}
