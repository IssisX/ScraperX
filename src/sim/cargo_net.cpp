#include "sim/cargo_net.hpp"
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/StateRecorderImpl.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace scraperx::sim {
CargoNet::CargoNet(JPH::PhysicsSystem &system,JPH::ObjectLayer layer,JPH::BodyID frame)
    : CargoNet(system,layer,frame,CargoNetConfig{}) {}
CargoNet::CargoNet(JPH::PhysicsSystem &system,JPH::ObjectLayer layer,JPH::BodyID frame,const CargoNetConfig &config)
    :config_(config),system_(system),frame_(frame) {
    if((config_.surface!=CargoNetSurface::OpenWeave && config_.surface!=CargoNetSurface::ContinuousMembrane) ||
       config_.columns<2 || config_.rows<2 || !(config_.dx>0) || !(config_.dy>0) ||
       !(config_.vertex_mass>0) || !(config_.knot_half_width>0) ||
       2*config_.knot_half_width>=std::min(config_.dx,config_.dy) || config_.iterations==0 ||
       config_.damping<0 || config_.axial_damping_ratio<0 || config_.clip_stiffness<0 || config_.clip_damping<0 ||
       config_.stretch_compliance<0 || config_.shear_compliance<0 || config_.bend_compliance<0 ||
       !std::isfinite(config_.dx+config_.dy+config_.knot_half_width+config_.vertex_mass+
           config_.damping+config_.axial_damping_ratio+config_.clip_stiffness+config_.clip_damping+
           config_.stretch_compliance+config_.shear_compliance+config_.bend_compliance))
        throw std::invalid_argument("Invalid cargo net material or grid");
    if(config_.axial_damping_ratio>0 && !config_.internally_integrated_clips)
        throw std::invalid_argument("Axial cargo damping requires internal native integration");
    if(config_.piston_pad_mass_kg<0 || !std::isfinite(config_.piston_pad_mass_kg))
        throw std::invalid_argument("Invalid cargo reinforcement pad mass");
    if(config_.piston_pad_mass_kg>0) {
        const auto material=config_.piston_material;
        if(config_.surface!=CargoNetSurface::ContinuousMembrane ||
           !std::isfinite(material.GetX()) || !std::isfinite(material.GetY()) || !std::isfinite(material.GetZ()) ||
           material.GetZ()!=0 || material.GetX()<0 || material.GetX()>config_.columns-1 ||
           material.GetY()<0 || material.GetY()>config_.rows-1)
            throw std::invalid_argument("Invalid membrane reinforcement pad material point");
    }
    if(config_.tether_enabled) {
        const auto material=config_.tether_material,anchor=config_.tether_anchor_body_local;
        if(!config_.internally_integrated_clips || config_.surface!=CargoNetSurface::ContinuousMembrane ||
           !std::isfinite(material.GetX()) || !std::isfinite(material.GetY()) || !std::isfinite(material.GetZ()) ||
           !std::isfinite(anchor.GetX()) || !std::isfinite(anchor.GetY()) || !std::isfinite(anchor.GetZ()) || material.GetZ()!=0 ||
           material.GetX()<0 || material.GetX()>config_.columns-1 ||
           material.GetY()<0 || material.GetY()>config_.rows-1 ||
           !(config_.tether_rest_length_m>0) || !(config_.tether_stiffness>0) || config_.tether_damping<0 ||
           !std::isfinite(config_.tether_rest_length_m+config_.tether_stiffness+config_.tether_damping))
            throw std::invalid_argument("Invalid finite internally integrated membrane tether");
    }
    if(config_.internally_integrated_clips) {
        const JPH::BodyLockRead lock(system_.GetBodyLockInterfaceNoLock(),frame_);
        if(!lock.Succeeded() || !lock.GetBody().IsRigidBody() || !lock.GetBody().IsDynamic())
            throw std::invalid_argument("Internal cargo clips require a finite dynamic rigid frame");
        if(config_.tether_enabled) {
            const auto &frame_body=lock.GetBody();
            tether_anchor_com_local_=frame_body.GetCenterOfMassTransform().Inversed()*
                (frame_body.GetWorldTransform()*JPH::RVec3(config_.tether_anchor_body_local));
        }
    }
    if(!config_.piston_body.IsInvalid()) {
        const auto material=config_.piston_material,anchor=config_.piston_body_local;
        if(!config_.internally_integrated_clips || config_.piston_body==frame_ ||
           !std::isfinite(material.GetX()) || !std::isfinite(material.GetY()) || !std::isfinite(material.GetZ()) ||
           !std::isfinite(anchor.GetX()) || !std::isfinite(anchor.GetY()) || !std::isfinite(anchor.GetZ()) || material.GetZ()!=0 ||
           material.GetX()<0 || material.GetX()>config_.columns-1 ||
           material.GetY()<0 || material.GetY()>config_.rows-1 ||
           !(config_.piston_stiffness>0) || config_.piston_damping<0 ||
           !std::isfinite(config_.piston_stiffness+config_.piston_damping))
            throw std::invalid_argument("Invalid finite internally integrated piston attachment");
        const JPH::BodyLockRead lock(system_.GetBodyLockInterface(),config_.piston_body);
        if(!lock.Succeeded() || !lock.GetBody().IsRigidBody() || !lock.GetBody().IsDynamic())
            throw std::invalid_argument("Cargo piston attachment requires a finite dynamic rigid body");
        const auto &piston=lock.GetBody();
        piston_anchor_com_local_=piston.GetCenterOfMassTransform().Inversed()*
            (piston.GetWorldTransform()*JPH::RVec3(anchor));
    }
    sleep_rigid_bodies_.push_back(frame_);
    if(!config_.piston_body.IsInvalid()) sleep_rigid_bodies_.push_back(config_.piston_body);
    for(const auto id:config_.sleep_coupled_bodies) {
        if(!config_.paired_sleep || id.IsInvalid())
            throw std::invalid_argument("Coupled cargo sleep requires paired sleep and valid body identities");
        const JPH::BodyLockRead lock(system_.GetBodyLockInterfaceNoLock(),id);
        if(!lock.Succeeded() || !lock.GetBody().IsRigidBody() || !lock.GetBody().IsDynamic())
            throw std::invalid_argument("Coupled cargo sleep requires finite dynamic rigid bodies");
        if(std::find(sleep_rigid_bodies_.begin(),sleep_rigid_bodies_.end(),id)==sleep_rigid_bodies_.end())
            sleep_rigid_bodies_.push_back(id);
    }
    sleeping_rigid_positions_.resize(sleep_rigid_bodies_.size());
    sleeping_rigid_rotations_.resize(sleep_rigid_bodies_.size());
    using namespace JPH;
    Ref<SoftBodySharedSettings> settings=new SoftBodySharedSettings;
    // Open weave retains four corners per knot and real holes. A continuous
    // membrane instead has one physical vertex at each grid point.
    for(int r=0;r<config_.rows;++r) for(int c=0;c<config_.columns;++c) for(int corner=0;corner<corners_per_node();++corner) {
        SoftBodySharedSettings::Vertex v;
        const bool woven=config_.surface==CargoNetSurface::OpenWeave;
        Vec3(c*config_.dx+(woven?((corner&1)?config_.knot_half_width:-config_.knot_half_width):0),
             r*config_.dy+(woven?((corner&2)?config_.knot_half_width:-config_.knot_half_width):0),0).StoreFloat3(&v.mPosition);
        v.mInvMass=1.0F/config_.vertex_mass;
        settings->mVertices.push_back(v);
    }
    if(config_.piston_pad_mass_kg>0) {
        // sample() uses only the grid topology: no body exists yet. Uniform
        // triangle area lumping gives each corner one third of the pad mass,
        // independent of the attachment point's barycentric force weights.
        const auto triangle=sample(config_.piston_material.GetX(),config_.piston_material.GetY());
        if(triangle.count!=3 || triangle.vertices[0]==triangle.vertices[1] ||
           triangle.vertices[0]==triangle.vertices[2] || triangle.vertices[1]==triangle.vertices[2])
            throw std::invalid_argument("Cargo reinforcement pad requires three unique membrane vertices");
        reinforcement_indices_.assign(triangle.vertices.begin(),triangle.vertices.begin()+3);
        const float inverse_mass=float(1.0/(double(config_.vertex_mass)+double(config_.piston_pad_mass_kg)/3.0));
        for(const auto index:reinforcement_indices_) settings->mVertices[index].mInvMass=inverse_mass;
    }
    const auto face=[&](std::uint32_t a,std::uint32_t b,std::uint32_t c) {
        SoftBodySharedSettings::Face f; f.mVertex[0]=a;f.mVertex[1]=b;f.mVertex[2]=c;
        settings->AddFace(f); indices_.insert(indices_.end(),{a,b,c});
    };
    const auto quad=[&](std::uint32_t a,std::uint32_t b,std::uint32_t c,std::uint32_t d) { face(a,b,c);face(a,c,d); };
    for(int r=0;r<config_.rows;++r) for(int c=0;c<config_.columns;++c) {
        if(config_.surface==CargoNetSurface::ContinuousMembrane) {
            if(c+1<config_.columns && r+1<config_.rows)
                quad(vertex(c,r,0),vertex(c+1,r,0),vertex(c+1,r+1,0),vertex(c,r+1,0));
        } else {
            quad(vertex(c,r,0),vertex(c,r,1),vertex(c,r,3),vertex(c,r,2));
            if(c+1<config_.columns) quad(vertex(c,r,1),vertex(c+1,r,0),vertex(c+1,r,2),vertex(c,r,3));
            if(r+1<config_.rows) quad(vertex(c,r,2),vertex(c,r,3),vertex(c,r+1,1),vertex(c,r+1,0));
        }
    }
    // Native distance-constraint compliance is SI m/N (inverse edge stiffness),
    // not a continuum Young's modulus: E would additionally require a chosen
    // cross section/thickness. Retain the nominal stretch/shear/bend compliance
    // for the first membrane candidate; topology changes their resulting law.
    // Jolt owns constraints, gravity, vertex/rigid collision and integration.
    const SoftBodySharedSettings::VertexAttributes material(config_.stretch_compliance,config_.shear_compliance,config_.bend_compliance);
    settings->CreateConstraints(&material,1,SoftBodySharedSettings::EBendType::Distance);
    settings->Optimize();
    if(config_.axial_damping_ratio>0) for(const auto &edge:settings->mEdgeConstraints)
        if(!(edge.mCompliance>0) || !std::isfinite(edge.mCompliance))
            throw std::invalid_argument("Axial cargo damping requires finite positive native edge compliance");
    SoftBodyCreationSettings creation(settings,config_.origin,config_.rotation,layer);
    creation.mUserData=config_.entity;creation.mUpdatePosition=false;creation.mAllowSleeping=config_.paired_sleep;
    creation.mNumIterations=config_.iterations;creation.mLinearDamping=config_.damping;creation.mVertexRadius=0.035F;
    creation.mFriction=0.8F;creation.mFacesDoubleSided=true;
    body_=system_.GetBodyInterface().CreateAndAddSoftBody(creation,EActivation::Activate);
    if(config_.paired_sleep) {
        const BodyLockWrite lock(system_.GetBodyLockInterface(),frame_);
        if(lock.Succeeded()) lock.GetBody().SetAllowSleeping(true);
    }
    refresh();
    const auto frame_inverse=system_.GetBodyInterface().GetCenterOfMassTransform(frame_).Inversed();
    for(int row=0;row<config_.rows;++row) for(int column=0;column<config_.columns;++column) {
        const bool clipped=row==0 || row==config_.rows-1 ||
            (config_.clips==CargoNetClips::Perimeter && (column==0 || column==config_.columns-1));
        if(clipped) for(int corner=0;corner<corners_per_node();++corner) {
            const auto index=vertex(column,row,corner);
            attachment_vertices_.push_back(index);
            attachment_local_.push_back(frame_inverse*vertices_[index]);
        }
    }
    if(config_.internally_integrated_clips) {
        const BodyLockWrite lock(system_.GetBodyLockInterfaceNoLock(),body_);
        auto *motion=static_cast<SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
        motion->SetPreserveIntegratedVelocity(true);
        motion->SetExternalForceCallbacks(
            [](void *owner,float dt){static_cast<CargoNet *>(owner)->internal_clip_step(dt);},
            [](void *owner,BodyInterface &bodies){static_cast<CargoNet *>(owner)->finish_clip_step(bodies);},this);
    }
}
void CargoNet::set_contact_friction_callback(ContactFrictionCallback callback, void *context) {
    const JPH::BodyLockWrite lock(system_.GetBodyLockInterface(),body_);
    auto *motion=static_cast<JPH::SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
    motion->SetContactFrictionCallback(callback,context);
}
void CargoNet::pre_step(float dt) {
    // Owner-thread or serialized Jolt step-listener only. All body mutexes
    // are already held during the listener, so use the no-lock interfaces.
    auto &bodies=system_.GetBodyInterfaceNoLock();
    if(config_.paired_sleep) {
        if(assembly_asleep() && !sleeping_assembly_changed()) return;
        wake_assembly();
    }
    apply_piston_step(dt);
    if(config_.internally_integrated_clips) return;
    const JPH::BodyLockWrite lock(system_.GetBodyLockInterfaceNoLock(),body_);
    if(!lock.Succeeded()) return;
    auto &body=lock.GetBody();
    auto *motion=static_cast<JPH::SoftBodyMotionProperties *>(body.GetMotionProperties());
    const auto transform=body.GetCenterOfMassTransform();
    const auto frame_transform=bodies.GetCenterOfMassTransform(frame_);
    std::size_t attachment=0;
    for(const auto index:attachment_vertices_) {
        auto &v=motion->GetVertex(index);
        const auto actual=transform*JPH::RVec3(v.mPosition);
        const auto target=frame_transform*attachment_local_[attachment++];
        const auto relative=body.GetRotation()*v.mVelocity-bodies.GetPointVelocity(frame_,actual);
        // Configured finite passive clip stiffness and damping (legacy5000N/m,25Ns/m).
        // Every vertex has finite mass; no world pose pin or vertex overwrite.
        const auto impulse=(JPH::Vec3(target-actual)*config_.clip_stiffness-relative*config_.clip_damping)*dt;
        v.mVelocity+=body.GetRotation().Conjugated()*impulse*v.mInvMass;
        bodies.AddImpulse(frame_,-impulse,actual);
    }
}
void CargoNet::apply_piston_step(float dt) {
    if(config_.piston_body.IsInvalid() || !(dt>0) || !std::isfinite(dt)) return;
    // The serialized native pre-step listener runs before rigid and soft
    // position integration. Deliver the rigid reaction and stage the identical
    // cloth impulse for its native force integrations.
    const JPH::BodyLockWrite cloth_lock(system_.GetBodyLockInterfaceNoLock(),body_);
    const JPH::BodyLockRead piston_lock(system_.GetBodyLockInterfaceNoLock(),config_.piston_body);
    if(!cloth_lock.Succeeded() || !piston_lock.Succeeded()) return;
    auto &body=cloth_lock.GetBody();
    auto *motion=static_cast<JPH::SoftBodyMotionProperties *>(body.GetMotionProperties());
    const auto &piston=piston_lock.GetBody();
    const auto *piston_motion=piston.GetMotionProperties();
    const auto transform=body.GetCenterOfMassTransform();
    const auto rotation=body.GetRotation();
    const auto piston_transform=piston.GetCenterOfMassTransform();
    const auto piston_inverse_inertia=piston_motion->GetInverseInertiaForRotation(JPH::Mat44::sRotation(piston.GetRotation()));
    const float piston_inverse_mass=piston_motion->GetInverseMass();
    const auto piston_linear=piston.GetLinearVelocity(),piston_angular=piston.GetAngularVelocity();
    const auto weights=sample(config_.piston_material.GetX(),config_.piston_material.GetY());
    JPH::Vec3 local_point=JPH::Vec3::sZero(),local_velocity=JPH::Vec3::sZero();
    float material_inverse_mass=0;
    for(std::size_t i=0;i<weights.count;++i) {
        const auto &v=motion->GetVertex(weights.vertices[i]);
        local_point+=v.mPosition*weights.weights[i];
        local_velocity+=v.mVelocity*weights.weights[i];
        material_inverse_mass+=weights.weights[i]*weights.weights[i]*v.mInvMass;
    }
    const auto actual=transform*JPH::RVec3(local_point);
    const auto anchor=piston_transform*piston_anchor_com_local_;
    // A finite elastomer eye uses the same actual-point reaction law as
    // perimeter clips. Body rotation carries both spring and shear loss;
    // the actual cloth lever closes total linear/angular impulse.
    const auto lever=JPH::Vec3(actual-piston_transform.GetTranslation());
    const auto delta=JPH::Vec3(actual-anchor);
    const auto relative=rotation*local_velocity-(piston_linear+piston_angular.Cross(lever));
    const auto cross=JPH::Mat44::sCrossProduct(lever);
    const auto inverse_mass=JPH::Mat44::sScale(material_inverse_mass+piston_inverse_mass)-
        cross*piston_inverse_inertia*cross;
    const auto iterations=motion->GetNumIterations();
    // A constant finite cloth force is integrated N times while the rigid
    // reaction precedes its one position integration. The cloth displacement
    // response is (N+1)/(2N), derived from sum(1..N)/N², not a tuning factor.
    // The dashpot uses this actual discrete geometric strain rate. With A
    // below and M=inverse_mass, A-M/2 is positive semidefinite, so the isolated
    // linear predictor loses hc|v-AJ|² + J.(A-M/2)J + kh²|v-AJ|²/2.
    const float alpha=(float(iterations)+1)/(2.0F*float(iterations));
    const auto displacement_inverse_mass=inverse_mass-
        JPH::Mat44::sScale((1-alpha)*material_inverse_mass);
    const float h=dt,k=config_.piston_stiffness,c=config_.piston_damping;
    const float spring_dashpot=c+h*k;
    const auto reaction=(JPH::Mat44::sIdentity()+displacement_inverse_mass*(h*spring_dashpot)).Inversed3x3()*
        ((delta*k+relative*spring_dashpot)*h);
    piston_pending_reaction_=reaction;piston_pending_iterations_=iterations;
    piston_reaction_point_=actual;piston_reaction_moment_=JPH::Vec3::sZero();
    // Rigid reaction still precedes rigid integration. The same finite cloth
    // impulse is consumed by internal_clip_step at the cloth's real cadence,
    // avoiding the whole-tick kick that edge solves reverse at idle equilibrium.
    system_.GetBodyInterfaceNoLock().AddImpulse(config_.piston_body,reaction,actual);
    const auto mobility_impulse=inverse_mass*reaction;
    const auto next_delta=delta+(relative-displacement_inverse_mass*reaction)*h;
    const double loss=reaction.Dot(relative)-.5*reaction.Dot(mobility_impulse)+
        .5*k*(double(delta.LengthSq())-next_delta.LengthSq());
    // Relative predictor loss includes integration loss. Actual endpoint
    // geometry/energy is read independently after the native tick.
    piston_dissipated_work_+=std::max(0.0,loss);
}
void CargoNet::internal_clip_step(float dt) {
    // Jolt's last constraint group serializes each iteration for this soft body.
    // Rigid solve has completed, but other soft bodies may run concurrently:
    // read the real frame once and buffer its reactions until native writeback.
    const JPH::BodyLockWrite lock(system_.GetBodyLockInterfaceNoLock(),body_);
    if(!lock.Succeeded()) return;
    auto &body=lock.GetBody();
    auto *motion=static_cast<JPH::SoftBodyMotionProperties *>(body.GetMotionProperties());
    if(piston_pending_iterations_>0) {
        // Divide the remaining impulse by the remaining native integrations;
        // the last consumes the exact remainder rather than leaving a tail.
        const auto reaction=piston_pending_reaction_/float(piston_pending_iterations_);
        piston_pending_reaction_-=reaction;--piston_pending_iterations_;
        const auto weights=sample(config_.piston_material.GetX(),config_.piston_material.GetY());
        JPH::Vec3 local_point=JPH::Vec3::sZero();
        const auto local_impulse=body.GetRotation().Conjugated()*(-reaction);
        for(std::size_t i=0;i<weights.count;++i) {
            auto &v=motion->GetVertex(weights.vertices[i]);
            local_point+=v.mPosition*weights.weights[i];
            v.mVelocity+=local_impulse*(weights.weights[i]*v.mInvMass);
        }
        const auto actual=body.GetCenterOfMassTransform()*JPH::RVec3(local_point);
        // The early rigid impulse already supplied x_initial cross J. These
        // are the remaining real moments of the distributed actual-point
        // reactions, serialized with the other native rigid writebacks.
        piston_reaction_moment_+=JPH::Vec3(actual-piston_reaction_point_).Cross(reaction);
    }
    if(force_remaining_seconds_>0) {
        double elapsed=std::min(double(dt),force_remaining_seconds_);
        force_remaining_seconds_-=elapsed;
        // Close only float subdivision roundoff at the final native step,
        // retaining the full requested integral and leaving no replay tail.
        if(force_remaining_seconds_<=double(dt)*8*std::numeric_limits<float>::epsilon()) {
            elapsed+=force_remaining_seconds_;force_remaining_seconds_=0;
        }
        const auto local_impulse=body.GetRotation().Conjugated()*pending_world_force_*float(elapsed);
        for(std::size_t i=0;i<force_vertex_count_;++i) {
            auto &v=motion->GetVertex(force_vertices_[i]);
            if(v.mInvMass>0 && std::isfinite(v.mInvMass))
                v.mVelocity+=local_impulse*(force_weights_[i]*v.mInvMass);
        }
    }
    // Kelvin-Voigt strain-rate loss uses the actual optimized native edges,
    // before finite clips and Jolt integration. Compliance is SI m/N: the
    // pinned edge solver adds compliance / h² to the inverse-mass denominator.
    // Each implicit dashpot acts centrally and equally/oppositely, preserving
    // total linear/angular momentum while reducing pair kinetic energy. It
    // cannot damp rigid translation/rotation or manufacture an equilibrium.
    if(config_.axial_damping_ratio>0) for(const auto &edge:motion->GetSettings()->mEdgeConstraints) {
        auto &a=motion->GetVertex(edge.mVertex[0]);
        auto &b=motion->GetVertex(edge.mVertex[1]);
        const auto delta=b.mPosition-a.mPosition;
        const float length=delta.Length(),inverse_mass=a.mInvMass+b.mInvMass;
        // Coincident vertices have no defined axial direction; a fixed pair
        // has no movable mass. Neither can receive finite axial work here.
        if(!(length>1.0e-12F) || !(inverse_mass>0) || !std::isfinite(length+inverse_mass)) continue;
        const auto axis=delta/length;
        const float relative=(b.mVelocity-a.mVelocity).Dot(axis);
        const double c=2.0*config_.axial_damping_ratio*
            std::sqrt(1.0/(double(edge.mCompliance)*inverse_mass));
        const double hc=double(dt)*c;
        const float impulse=float(-hc*relative/(1.0+hc*inverse_mass));
        b.mVelocity+=axis*(impulse*b.mInvMass);
        a.mVelocity-=axis*(impulse*a.mInvMass);
    }
    if(!clip_step_started_) {
        const JPH::BodyLockRead frame_lock(system_.GetBodyLockInterfaceNoLock(),frame_);
        if(!frame_lock.Succeeded()) return;
        const auto &frame=frame_lock.GetBody();
        const auto *frame_motion=frame.GetMotionProperties();
        clip_frame_transform_=frame.GetCenterOfMassTransform();
        clip_frame_inverse_inertia_=frame_motion->GetInverseInertiaForRotation(JPH::Mat44::sRotation(frame.GetRotation()));
        clip_frame_inverse_mass_=frame_motion->GetInverseMass();
        clip_frame_linear_=frame.GetLinearVelocity();clip_frame_angular_=frame.GetAngularVelocity();
        clip_reaction_=JPH::Vec3::sZero();clip_torque_=JPH::Vec3::sZero();
        clip_step_started_=true;
    }
    const auto transform=body.GetCenterOfMassTransform();
    const auto rotation=body.GetRotation();
    const float spring_dashpot=config_.clip_damping+dt*config_.clip_stiffness;
    std::size_t attachment=0;
    for(const auto index:attachment_vertices_) {
        auto &v=motion->GetVertex(index);
        const auto actual=transform*JPH::RVec3(v.mPosition);
        const auto target=clip_frame_transform_*attachment_local_[attachment++];
        const auto lever=JPH::Vec3(actual-clip_frame_transform_.GetTranslation());
        const auto relative=rotation*v.mVelocity-(clip_frame_linear_+clip_frame_angular_.Cross(lever));
        const auto cross=JPH::Mat44::sCrossProduct(lever);
        const auto inverse_mass=JPH::Mat44::sScale(v.mInvMass+clip_frame_inverse_mass_)-
            cross*clip_frame_inverse_inertia_*cross;
        // Backward-Euler finite spring/dashpot impulse at the native cloth dt.
        // The frame point has its real translational and rotational inertia.
        const auto rhs=(JPH::Vec3(target-actual)*config_.clip_stiffness-relative*spring_dashpot)*dt;
        const auto impulse=(JPH::Mat44::sIdentity()+inverse_mass*(dt*spring_dashpot)).Inversed3x3()*rhs;
        v.mVelocity+=rotation.Conjugated()*impulse*v.mInvMass;
        const auto reaction=-impulse,torque=lever.Cross(reaction);
        clip_reaction_+=reaction;clip_torque_+=torque;
        clip_frame_linear_+=reaction*clip_frame_inverse_mass_;
        clip_frame_angular_+=clip_frame_inverse_inertia_*torque;
    }

    tether_tension_=0;
    if(config_.tether_enabled && dt>0) {
        const auto weights=sample(config_.tether_material.GetX(),config_.tether_material.GetY());
        JPH::Vec3 local_point=JPH::Vec3::sZero(),local_velocity=JPH::Vec3::sZero();
        float material_inverse_mass=0;
        for(std::size_t i=0;i<weights.count;++i) {
            const auto &v=motion->GetVertex(weights.vertices[i]);
            local_point+=v.mPosition*weights.weights[i];
            local_velocity+=v.mVelocity*weights.weights[i];
            material_inverse_mass+=weights.weights[i]*weights.weights[i]*v.mInvMass;
        }
        const auto actual=transform*JPH::RVec3(local_point);
        const auto anchor=clip_frame_transform_*tether_anchor_com_local_;
        const auto delta=JPH::Vec3(actual-anchor);
        const float length=delta.Length(),extension=length-config_.tether_rest_length_m;
        if(extension>0 && std::isfinite(length)) {
            const auto axis=delta/length;
            const auto lever=JPH::Vec3(anchor-clip_frame_transform_.GetTranslation());
            const auto torque_axis=lever.Cross(axis);
            const double inverse_mass=material_inverse_mass+clip_frame_inverse_mass_+
                torque_axis.Dot(clip_frame_inverse_inertia_*torque_axis);
            const double relative=(rotation*local_velocity-
                (clip_frame_linear_+clip_frame_angular_.Cross(lever))).Dot(axis);
            const double k=config_.tether_stiffness,c=config_.tether_damping,h=dt;
            // Backward Euler radial spring/dashpot with unilateral tension.
            // The transpose barycentric map and real anchor lever carry the
            // equal/opposite linear and angular reaction to the finite frame.
            const double impulse=std::max(0.0,h*(k*extension+(c+h*k)*relative)/
                (1.0+h*(c+h*k)*inverse_mass));
            const auto reaction=axis*float(impulse);
            const auto local_impulse=rotation.Conjugated()*(-reaction);
            for(std::size_t i=0;i<weights.count;++i) {
                auto &v=motion->GetVertex(weights.vertices[i]);
                v.mVelocity+=local_impulse*(weights.weights[i]*v.mInvMass);
            }
            const auto torque=lever.Cross(reaction);
            clip_reaction_+=reaction;clip_torque_+=torque;
            clip_frame_linear_+=reaction*clip_frame_inverse_mass_;
            clip_frame_angular_+=clip_frame_inverse_inertia_*torque;
            tether_tension_=float(impulse/h);
            const double next_extension=std::max(0.0,extension+h*(relative-impulse*inverse_mass));
            const double loss=impulse*relative-0.5*impulse*impulse*inverse_mass+
                0.5*k*(double(extension)*extension-next_extension*next_extension);
            // Convex unilateral spring's radial BE update is passive; clamp
            // only diagnostic roundoff. Actual geometric U is derived below.
            tether_dissipated_work_+=std::max(0.0,loss);
        }
    }
}
void CargoNet::finish_clip_step(JPH::BodyInterface &bodies) {
    // Serialized Jolt rigid writeback, after solved contacts and before sleep.
    // Both bodies were awakened by pre_step; only finite real impulses act here.
    if(clip_reaction_.LengthSq()>0) bodies.AddImpulse(frame_,clip_reaction_);
    if(clip_torque_.LengthSq()>0) bodies.AddAngularImpulse(frame_,clip_torque_);
    if(piston_reaction_moment_.LengthSq()>0)
        bodies.AddAngularImpulse(config_.piston_body,piston_reaction_moment_);
    piston_reaction_moment_=JPH::Vec3::sZero();
    clip_reaction_=JPH::Vec3::sZero();clip_torque_=JPH::Vec3::sZero();
    clip_step_started_=false;
}
void CargoNet::wake_assembly() {
    auto &bodies=system_.GetBodyInterfaceNoLock();
    // Activating an already-active body resets Jolt's sleep timer. Only wake
    // the inactive side so ongoing equilibrium can accumulate normally.
    if(!bodies.IsActive(body_)) bodies.ActivateBody(body_);
    for(const auto id:sleep_rigid_bodies_)
        if(!bodies.IsActive(id)) bodies.ActivateBody(id);
    sleeping_assembly_valid_=false;
}
bool CargoNet::assembly_asleep() const {
    const auto &bodies=system_.GetBodyInterfaceNoLock();
    if(bodies.IsActive(body_)) return false;
    for(const auto id:sleep_rigid_bodies_) if(bodies.IsActive(id)) return false;
    return true;
}
bool CargoNet::sleeping_assembly_changed() const {
    if(!sleeping_assembly_valid_) return true;
    const auto &bodies=system_.GetBodyInterfaceNoLock();
    for(std::size_t i=0;i<sleep_rigid_bodies_.size();++i) {
        const auto id=sleep_rigid_bodies_[i];
        const auto rotation=bodies.GetRotation(id);
        if((bodies.GetCenterOfMassPosition(id)-sleeping_rigid_positions_[i]).LengthSq()>0 ||
           (rotation.GetXYZ()-sleeping_rigid_rotations_[i].GetXYZ()).LengthSq()>0 ||
           rotation.GetW()!=sleeping_rigid_rotations_[i].GetW() ||
           bodies.GetLinearVelocity(id).LengthSq()>0 || bodies.GetAngularVelocity(id).LengthSq()>0) return true;
    }
    return false;
}
bool CargoNet::settled() const {
    return config_.paired_sleep && sleeping_assembly_valid_ &&
        assembly_asleep() && !sleeping_assembly_changed();
}
void CargoNet::post_step(float dt,bool externally_touched) {
    if(!config_.paired_sleep) return;
    auto &bodies=system_.GetBodyInterfaceNoLock();
    const JPH::BodyLockRead net_lock(system_.GetBodyLockInterfaceNoLock(),body_);
    const JPH::BodyLockRead frame_lock(system_.GetBodyLockInterfaceNoLock(),frame_);
    if(!net_lock.Succeeded() || !frame_lock.Succeeded()) return;
    const auto &net_body=net_lock.GetBody();
    bool interacting=externally_touched;
    JPH::BodyIDVector active;
    system_.GetActiveBodies(JPH::EBodyType::RigidBody,active);
    for(const auto id:active) if(std::find(sleep_rigid_bodies_.begin(),sleep_rigid_bodies_.end(),id)==sleep_rigid_bodies_.end()) {
        const JPH::BodyLockRead lock(system_.GetBodyLockInterfaceNoLock(),id);
        if(lock.Succeeded() && lock.GetBody().GetWorldSpaceBounds().Overlaps(net_body.GetWorldSpaceBounds())) {
            interacting=true;
            break;
        }
    }
    if(interacting) {
        equilibrium_time_=0;
        wake_assembly();
        return;
    }
    if(assembly_asleep()) {
        if(sleeping_assembly_changed()) {equilibrium_time_=0;wake_assembly();}
        return;
    }
    wake_assembly();
    const auto *motion=static_cast<const JPH::SoftBodyMotionProperties *>(net_body.GetMotionProperties());
    float max_vertex_speed_sq=0;
    for(const auto &v:motion->GetVertices()) {
        const float speed_sq=v.mVelocity.LengthSq();
        if(!std::isfinite(speed_sq)) {equilibrium_time_=0;return;}
        max_vertex_speed_sq=std::max(max_vertex_speed_sq,speed_sq);
    }
    float max_rigid_point_speed=0;
    for(const auto id:sleep_rigid_bodies_) {
        const JPH::BodyLockRead lock(system_.GetBodyLockInterfaceNoLock(),id);
        if(!lock.Succeeded()) {equilibrium_time_=0;return;}
        const auto &rigid=lock.GetBody();
        const auto &bounds=rigid.GetWorldSpaceBounds();
        const float radius=(JPH::RVec3(bounds.GetCenter())-rigid.GetCenterOfMassPosition()).Length()+bounds.GetExtent().Length();
        const float speed=rigid.GetLinearVelocity().Length()+rigid.GetAngularVelocity().Length()*radius;
        if(!std::isfinite(speed)) {equilibrium_time_=0;return;}
        max_rigid_point_speed=std::max(max_rigid_point_speed,speed);
    }
    const auto &settings=system_.GetPhysicsSettings();
    // Match the pinned Jolt soft-body squared-speed test and conservatively
    // bound every coupled rigid point by linear speed plus angular speed * radius.
    if(!(dt>0) || !std::isfinite(dt) ||
       max_vertex_speed_sq>settings.mPointVelocitySleepThreshold || max_rigid_point_speed>settings.mPointVelocitySleepThreshold) {
        equilibrium_time_=0;
        return;
    }
    equilibrium_time_+=dt;
    if(equilibrium_time_<settings.mTimeBeforeSleep) return;
    // Outside the step listener: all bodies retain finite mass, vertex state,
    // spring strain and their dynamic collision shapes. Native deactivation
    // only clears aggregate velocities within the native equilibrium tolerance.
    auto assembly=sleep_rigid_bodies_;
    assembly.push_back(body_);
    bodies.DeactivateBodies(assembly.data(),int(assembly.size()));
    for(std::size_t i=0;i<sleep_rigid_bodies_.size();++i) {
        sleeping_rigid_positions_[i]=bodies.GetCenterOfMassPosition(sleep_rigid_bodies_[i]);
        sleeping_rigid_rotations_[i]=bodies.GetRotation(sleep_rigid_bodies_[i]);
    }
    sleeping_assembly_valid_=true;
    equilibrium_time_=0;
}
CargoNet::~CargoNet() { auto &b=system_.GetBodyInterface();b.RemoveBody(body_);b.DestroyBody(body_); }
void CargoNet::refresh() {
    const JPH::BodyLockRead lock(system_.GetBodyLockInterfaceNoLock(),body_);
    if(!lock.Succeeded()) return;
    const auto &body=lock.GetBody();
    const auto *motion=static_cast<const JPH::SoftBodyMotionProperties *>(body.GetMotionProperties());
    const auto transform=body.GetCenterOfMassTransform();
    previous_vertices_=vertices_;
    vertices_.clear();vertices_.reserve(motion->GetVertices().size());
    for(const auto &v:motion->GetVertices()) vertices_.push_back(transform*JPH::RVec3(v.mPosition));
    if(previous_vertices_.size()!=vertices_.size()) previous_vertices_=vertices_;
}
std::vector<JPH::RVec3> CargoNet::render_vertices(float alpha) const {
    std::vector<JPH::RVec3> out;out.reserve(vertices_.size());
    alpha=std::clamp(alpha,0.0F,1.0F);
    for(std::size_t i=0;i<vertices_.size();++i) out.push_back(previous_vertices_[i]*(1-alpha)+vertices_[i]*alpha);
    return out;
}
CargoNet::MaterialSample CargoNet::sample(float c,float r) const noexcept {
    c=std::clamp(c,0.0F,float(config_.columns-1));r=std::clamp(r,0.0F,float(config_.rows-1));
    const int x=std::min(int(c),config_.columns-2),y=std::min(int(r),config_.rows-2);
    const float u=c-x,v=r-y;
    MaterialSample out;
    const auto add=[&](int column,int row,int corner,float weight) {
        out.vertices[out.count]=vertex(column,row,corner);out.weights[out.count++]=weight;
    };
    if(config_.surface==CargoNetSurface::ContinuousMembrane) {
        // Same diagonal and barycentric map as the two real faces per cell.
        // Each entry names one independent mass; no duplicated knot corners.
        if(v<=u) {add(x,y,0,1-u);add(x+1,y,0,u-v);add(x+1,y+1,0,v);}
        else {add(x,y,0,1-v);add(x+1,y+1,0,u);add(x,y+1,0,v-u);}
    } else {
        for(int row=0;row<2;++row) for(int column=0;column<2;++column) for(int k=0;k<4;++k)
            add(x+column,y+row,k,(column?u:1-u)*(row?v:1-v)*0.25F);
    }
    return out;
}
JPH::RVec3 CargoNet::node(int c,int r) const noexcept {
    JPH::RVec3 sum=JPH::RVec3::sZero();
    for(int k=0;k<corners_per_node();++k) sum+=vertices_[vertex(c,r,k)];
    return sum/double(corners_per_node());
}
JPH::RVec3 CargoNet::surface(float c,float r) const noexcept {
    return surface(c,r,vertices_);
}
JPH::RVec3 CargoNet::surface(float c,float r,const std::vector<JPH::RVec3> &vertices) const noexcept {
    if(config_.surface==CargoNetSurface::ContinuousMembrane) {
        const auto weights=sample(c,r);
        JPH::RVec3 point=JPH::RVec3::sZero();
        for(std::size_t i=0;i<weights.count;++i) point+=vertices[weights.vertices[i]]*weights.weights[i];
        return point;
    }
    // Preserve the original woven node averaging and bilinear evaluation.
    c=std::clamp(c,0.0F,float(config_.columns-1));r=std::clamp(r,0.0F,float(config_.rows-1));
    const int x=std::min(int(c),config_.columns-2),y=std::min(int(r),config_.rows-2);
    const float u=c-x,v=r-y;
    const auto at=[&](int column,int row) {
        JPH::RVec3 sum=JPH::RVec3::sZero();
        for(int k=0;k<4;++k) sum+=vertices[vertex(column,row,k)];
        return sum*0.25;
    };
    return (at(x,y)*(1-u)+at(x+1,y)*u)*(1-v)+(at(x,y+1)*(1-u)+at(x+1,y+1)*u)*v;
}
JPH::RVec3 CargoNet::render_world_point(JPH::Vec3 material,float alpha) const noexcept {
    alpha=std::clamp(alpha,0.0F,1.0F);
    return surface(material.GetX(),material.GetY(),previous_vertices_)*(1-alpha)+
           surface(material.GetX(),material.GetY(),vertices_)*alpha+JPH::RVec3(config_.rotation*JPH::Vec3(0,0,material.GetZ()));
}
JPH::Vec3 CargoNet::material_point(JPH::RVec3 world) const noexcept {
    const auto local=config_.rotation.Conjugated()*JPH::Vec3(world-config_.origin);
    float c=std::clamp(local.GetX()/config_.dx,0.0F,float(config_.columns-1));
    float r=std::clamp(local.GetY()/config_.dy,0.0F,float(config_.rows-1));
    // Invert the actual deformed surface in X/Y. Store material coordinates,
    // so holding still follows the mesh instead of hanging on the rest pose.
    for(int i=0;i<6;++i) {
        const auto at=surface(c,r);
        const float cx=c<config_.columns-1.01F?0.01F:-0.01F,ry=r<config_.rows-1.01F?0.01F:-0.01F;
        const JPH::Vec3 dc=config_.rotation.Conjugated()*JPH::Vec3((surface(c+cx,r)-at)/cx),dr=config_.rotation.Conjugated()*JPH::Vec3((surface(c,r+ry)-at)/ry);
        const float det=dc.GetX()*dr.GetY()-dr.GetX()*dc.GetY();
        if(std::abs(det)<1e-7F) break;
        const auto delta=config_.rotation.Conjugated()*JPH::Vec3(world-at);
        const float dx=delta.GetX(),dy=delta.GetY();
        c=std::clamp(c+(dx*dr.GetY()-dy*dr.GetX())/det,0.0F,float(config_.columns-1));
        r=std::clamp(r+(dy*dc.GetX()-dx*dc.GetY())/det,0.0F,float(config_.rows-1));
    }
    return JPH::Vec3(c,r,(config_.rotation.Conjugated()*JPH::Vec3(world-surface(c,r))).GetZ());
}
JPH::Vec3 CargoNet::material_velocity(JPH::Vec3 material) const noexcept {
    const auto weights=sample(material.GetX(),material.GetY());
    const JPH::BodyLockRead lock(system_.GetBodyLockInterfaceNoLock(),body_);
    if(!lock.Succeeded()) return JPH::Vec3::sZero();
    const auto *motion=static_cast<const JPH::SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
    JPH::Vec3 velocity=JPH::Vec3::sZero();
    for(std::size_t i=0;i<weights.count;++i)
        velocity+=motion->GetVertex(weights.vertices[i]).mVelocity*weights.weights[i];
    return lock.GetBody().GetRotation()*velocity;
}
JPH::RVec3 CargoNet::world_point(JPH::Vec3 material) const noexcept {
    const JPH::BodyLockRead lock(system_.GetBodyLockInterfaceNoLock(),body_);
    if(!lock.Succeeded()) return surface(material.GetX(),material.GetY());
    const auto &body=lock.GetBody();
    const auto *motion=static_cast<const JPH::SoftBodyMotionProperties *>(body.GetMotionProperties());
    const auto weights=sample(material.GetX(),material.GetY());
    JPH::Vec3 point=JPH::Vec3::sZero();
    for(std::size_t i=0;i<weights.count;++i)
        point+=motion->GetVertex(weights.vertices[i]).mPosition*weights.weights[i];
    return body.GetCenterOfMassTransform()*JPH::RVec3(point)+JPH::RVec3(config_.rotation*JPH::Vec3(0,0,material.GetZ()));
}
JPH::RVec3 CargoNet::tether_material_endpoint() const noexcept {
    return world_point(config_.tether_material);
}
JPH::RVec3 CargoNet::tether_anchor_endpoint() const noexcept {
    return system_.GetBodyInterfaceNoLock().GetCenterOfMassTransform(frame_)*tether_anchor_com_local_;
}
double CargoNet::tether_elastic_energy() const noexcept {
    if(!config_.tether_enabled) return 0;
    const double extension=std::max(0.0,double(JPH::Vec3(tether_material_endpoint()-tether_anchor_endpoint()).Length())-
        config_.tether_rest_length_m);
    return 0.5*config_.tether_stiffness*extension*extension;
}
JPH::RVec3 CargoNet::piston_material_endpoint() const noexcept {
    return world_point(config_.piston_material);
}
JPH::RVec3 CargoNet::piston_anchor_endpoint() const noexcept {
    if(config_.piston_body.IsInvalid()) return JPH::RVec3::sZero();
    return system_.GetBodyInterfaceNoLock().GetCenterOfMassTransform(config_.piston_body)*piston_anchor_com_local_;
}
double CargoNet::piston_elastic_energy() const noexcept {
    if(config_.piston_body.IsInvalid()) return 0;
    return 0.5*config_.piston_stiffness*double(JPH::Vec3(piston_material_endpoint()-piston_anchor_endpoint()).LengthSq());
}
bool CargoNet::grip(JPH::RVec3 centre,JPH::RVec3 aim,JPH::Vec3 facing,JPH::RVec3 &point) const noexcept {
    const auto local=config_.rotation.Conjugated()*JPH::Vec3(centre-config_.origin);
    if(local.GetX()<-.8F || local.GetX()>(config_.columns-1)*config_.dx+.8F ||
       local.GetY()<-.65F || local.GetY()>(config_.rows-1)*config_.dy+1.25F || std::abs(local.GetZ())>2.0F) return false;
    const JPH::Vec3 right(-facing.GetZ(),0,facing.GetX());
    float nearest=std::numeric_limits<float>::max();
    const auto candidate=[&](JPH::RVec3 a,JPH::RVec3 b) {
        const JPH::Vec3 d(b-a);const float t=std::clamp(JPH::Vec3(aim-a).Dot(d)/d.LengthSq(),0.0F,1.0F);
        const auto p=a+t*d;const JPH::Vec3 delta(p-aim);
        if(std::abs(delta.Dot(right))>0.34F || std::abs(delta.GetY())>0.34F || std::abs(delta.Dot(facing))>0.39F || JPH::Vec3(p-centre).Dot(facing)<0.16F) return;
        if(delta.LengthSq()<nearest) { nearest=delta.LengthSq();point=p; }
    };
    for(int r=0;r<config_.rows;++r) for(int c=0;c<config_.columns;++c) {
        if(c+1<config_.columns) candidate(node(c,r),node(c+1,r));
        if(r+1<config_.rows) candidate(node(c,r),node(c,r+1));
    }
    return std::isfinite(nearest) && nearest<std::numeric_limits<float>::max();
}
float CargoNet::material_inverse_mass(JPH::Vec3 material) const noexcept {
    const auto weights=sample(material.GetX(),material.GetY());
    const JPH::BodyLockRead lock(system_.GetBodyLockInterfaceNoLock(),body_);
    if(!lock.Succeeded()) return 0.0F;
    const auto *motion=static_cast<const JPH::SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
    float inverse_mass=0.0F;
    for(std::size_t i=0;i<weights.count;++i) {
        const float w=weights.weights[i];
        inverse_mass+=w*w*motion->GetVertex(weights.vertices[i]).mInvMass;
    }
    return inverse_mass;
}
void CargoNet::impulse(JPH::Vec3 material,JPH::Vec3 world_impulse) {
    if(config_.paired_sleep && world_impulse.LengthSq()>0) {equilibrium_time_=0;wake_assembly();}
    const auto weights=sample(material.GetX(),material.GetY());
    const JPH::BodyLockWrite lock(system_.GetBodyLockInterfaceNoLock(),body_);
    if(!lock.Succeeded()) return;
    auto *motion=static_cast<JPH::SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
    const auto local_impulse=lock.GetBody().GetRotation().Conjugated()*world_impulse;
    // The transpose of the position sampler partitions one impulse. All
    // nodes receive their share; attachment forces subsequently reach the frame.
    for(std::size_t i=0;i<weights.count;++i) {
        auto &vtx=motion->GetVertex(weights.vertices[i]);
        vtx.mVelocity+=local_impulse*(weights.weights[i]*vtx.mInvMass);
    }
}
void CargoNet::force(JPH::Vec3 material,JPH::Vec3 world_force,float duration_seconds) {
    const auto finite=[](JPH::Vec3 v){return std::isfinite(v.GetX()) &&
        std::isfinite(v.GetY()) && std::isfinite(v.GetZ());};
    if(!finite(material) || !finite(world_force) || !std::isfinite(duration_seconds) || duration_seconds<0 ||
       !finite(world_force*duration_seconds))
        throw std::invalid_argument("Invalid finite cargo material force");
    if(duration_seconds==0 || world_force==JPH::Vec3::sZero()) return;
    if(!config_.internally_integrated_clips) {impulse(material,world_force*duration_seconds);return;}
    if(force_remaining_seconds_>0)
        throw std::logic_error("Cargo material force overlaps an unconsumed owner load");
    // Clamp in floating point before converting: finite extreme coordinates
    // must never cause undefined float-to-int conversion. Freeze material
    // weights, but use live body rotation and inverse masses at each substep.
    const auto weights=sample(material.GetX(),material.GetY());
    force_vertices_=weights.vertices;force_weights_=weights.weights;force_vertex_count_=weights.count;
    pending_world_force_=world_force;force_remaining_seconds_=duration_seconds;
    equilibrium_time_=0;wake_assembly();
}
std::string CargoNet::capture() const {
    const JPH::BodyLockRead lock(system_.GetBodyLockInterfaceNoLock(),body_);
    JPH::StateRecorderImpl stream;
    static_cast<const JPH::SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties())->SaveState(stream);
    return stream.GetData();
}
void CargoNet::restore(const std::string &state) {
    force_remaining_seconds_=0;pending_world_force_=JPH::Vec3::sZero();
    tether_tension_=0;tether_dissipated_work_=0;
    piston_dissipated_work_=0;
    piston_pending_reaction_=JPH::Vec3::sZero();piston_pending_iterations_=0;
    piston_reaction_moment_=JPH::Vec3::sZero();
    clip_reaction_=JPH::Vec3::sZero();clip_torque_=JPH::Vec3::sZero();clip_step_started_=false;
    if(state.empty()) return;
    JPH::StateRecorderImpl stream;stream.WriteBytes(state.data(),state.size());stream.Rewind();
    { const JPH::BodyLockWrite lock(system_.GetBodyLockInterfaceNoLock(),body_);
      static_cast<JPH::SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties())->RestoreState(stream); }
    auto &b=system_.GetBodyInterface();
    b.NotifyShapeChanged(body_,JPH::Vec3::sZero(),false,JPH::EActivation::Activate);
    if(config_.paired_sleep) {equilibrium_time_=0;wake_assembly();}
    refresh();
    previous_vertices_=vertices_;
}
}
