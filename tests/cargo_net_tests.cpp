// AS-024: removal/unreachable holds/blocked top-out must reject this route.
#include "sim/simulation.hpp"
#include "sim/cargo_net.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
using scraperx::sim::Simulation;
namespace {
void require(bool ok, const char *message) {
    if (!ok) { std::cerr << "FAIL AS-024 " << message << '\n'; std::exit(1); }
}
void tick(Simulation &s) { require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "native fixed tick accepted"); }
void settle(Simulation &s, int n=45) { (void)s.set_move_input(0,0); while(n-- > 0) tick(s); }
void walk(Simulation &s, double x, double z, int limit=2700) {
    for (int i=0; i<limit; ++i) {
        const auto p=s.snapshot().player_position;
        const double dx=x-p.x, dz=z-p.z, d=std::hypot(dx,dz);
        if (d < 0.12) { settle(s); return; }
        const double scale=std::min(1.0,d/0.5)/d;
        (void)s.set_move_input(dx*scale,dz*scale); (void)s.set_facing(dx/d,dz/d); tick(s);
    }
    auto p=s.snapshot().player_position;
    std::cerr << "stalled walk at " << p.x << ',' << p.y << ',' << p.z << '\n';
    require(false,"ordinary walking reaches the next route point");
}
void loaded_pause(Simulation &s, double lane) {
    const auto start=s.snapshot();
    const auto vertices=s.cargo_net_vertices();
    require(!vertices.empty(),"loaded cargo mesh exists");
    // Retain one material vertex near the actual grip. Re-selecting a nearest
    // point each tick could confuse hand regripping with material deformation.
    std::size_t vertex=0;
    double nearest=1.0e30;
    for(std::size_t i=0;i<vertices.size();++i) {
        const auto &p=vertices[i]; const auto &h=start.traversal_left_hand;
        const double d=std::hypot(std::hypot(p.x-h.x,p.y-h.y),p.z-h.z);
        if(d<nearest) { nearest=d; vertex=i; }
    }
    (void)s.set_move_input(0,0);
    double force_y_sum=0, peak_material_dz=0, peak_grip_dz=0;
    constexpr int samples=180;
    // Height alone does not prove a contact-free hold: free radial swing can
    // briefly put the real capsule's foot on the loaded cloth. Allow one
    // existing two-second settling interval, then require the same180
    // consecutive unsupported solves. A contact restarts the load window;
    // it cannot contribute to the hand-only weight measurement.
    int consecutive=0, elapsed=0, foot_brushes=0;
    for(;elapsed<2*samples && consecutive<samples;++elapsed) {
        tick(s);
        const auto held=s.snapshot();
        if (held.player_grounded || held.support_entity_id!=0 ||
            held.traversal_state!=scraperx::sim::TraversalState::Climbing ||
            held.traversal_support_entity_id!=scraperx::sim::CargoNet::kEntity ||
            held.traversal_hand_constraint_count!=2 || held.player_gravity_factor!=1.0) {
            std::cerr << "PAUSE_TOPOLOGY lane=" << lane << " sample=" << elapsed << " tick=" << held.tick_index
                << " grounded=" << held.player_grounded << " support=" << held.support_entity_id
                << " traversal=" << int(held.traversal_state) << " hand_support=" << held.traversal_support_entity_id
                << " hands=" << held.traversal_hand_constraint_count << " gravity=" << held.player_gravity_factor
                << " position=" << held.player_position.x << ',' << held.player_position.y << ',' << held.player_position.z
                << " velocity=" << held.player_linear_velocity.x << ',' << held.player_linear_velocity.y << ',' << held.player_linear_velocity.z
                << " hand0=" << held.traversal_left_hand.x << ',' << held.traversal_left_hand.y << ',' << held.traversal_left_hand.z
                << " hand1=" << held.traversal_right_hand.x << ',' << held.traversal_right_hand.y << ',' << held.traversal_right_hand.z
                << " force0=" << held.traversal_left_hand_force.y << " force1=" << held.traversal_right_hand_force.y
                << " contact=" << held.support_contact_point.x << ',' << held.support_contact_point.y << ',' << held.support_contact_point.z
                << " generations=" << held.traversal_left_hand_generation << ',' << held.traversal_right_hand_generation
                << " side_contact=" << held.soft_side_contact_entity
                << " foot=" << held.foot_push_active << " foot_support=" << held.foot_push_support_entity_id << '\n';
        }
        require(held.traversal_state==scraperx::sim::TraversalState::Climbing &&
                held.traversal_support_entity_id==scraperx::sim::CargoNet::kEntity &&
                held.traversal_hand_constraint_count==2 && held.player_gravity_factor==1.0,
                "loaded pause retains two actual material hands and ordinary gravity");
        require(held.traversal_command_work_bound_j==start.traversal_command_work_bound_j,
                "idle loaded pause adds no commanded muscle work");
        const double force_y=held.traversal_left_hand_force.y+held.traversal_right_hand_force.y;
        require(std::isfinite(force_y),"loaded hand reaction is finite");
        if (held.player_grounded || held.support_entity_id!=0) {
            const auto p=held.player_position, contact=held.support_contact_point;
            // The authored standing capsule is0.9m half-height/0.35m radius.
            // Retain its actual bottom-cap receipt, with20mm contact margin;
            // a side/foreign support cannot be dismissed as a cloth brush.
            require(held.player_grounded && held.support_entity_id==scraperx::sim::CargoNet::kEntity &&
                    !held.foot_push_active && contact.y<=p.y-(.9-.35) &&
                    contact.y>=p.y-.9-.02 && std::hypot(contact.x-p.x,contact.z-p.z)<=.35+.02,
                    "pause settling permits only actual cloth contact on the capsule foot");
            consecutive=0;
            force_y_sum=0;
            ++foot_brushes;
        } else {
            ++consecutive;
            force_y_sum+=force_y;
        }
        const auto mesh=s.cargo_net_vertices();
        require(mesh.size()==vertices.size(),"loaded mesh retains material identity");
        peak_material_dz=std::max(peak_material_dz,std::abs(mesh[vertex].z-vertices[vertex].z));
        peak_grip_dz=std::max(peak_grip_dz,std::abs(held.traversal_left_hand.z-start.traversal_left_hand.z));
    }
    require(consecutive==samples,"loaded pause proves180 consecutive unsupported hand-load solves within4seconds");
    const double mean_force_y=force_y_sum/samples;
    constexpr double rider_weight_n=85.0*9.81;
    // Average over the compliant stop transient; a last-substep force sample
    // is not a static equilibrium measurement. The separate physical-hand
    // test rejects broken reciprocal impulses at the actual material owner.
    require(mean_force_y>0.75*rider_weight_n && mean_force_y<1.25*rider_weight_n,
            "mean hand reaction carries rider weight during the suspended pause");
    // Preserve the original 15mm minimum, now under real hand load. Measure
    // the window peak so an oscillation returning to its start is not failure.
    require(peak_material_dz>0.015 && peak_grip_dz>0.015,
            "climber load physically deflects the net and its real grip");
    std::cout << "CARGO_LOAD lane=" << lane << " mean_hand_force_n=" << mean_force_y
              << " material_peak_dz=" << peak_material_dz << " grip_peak_dz=" << peak_grip_dz
              << " grounded=0 hands=2 command_work_delta=0 unsupported_ticks=" << consecutive
              << " elapsed_ticks=" << elapsed << " cloth_brush_ticks=" << foot_brushes << '\n';
    (void)s.set_move_input(0,-1);
}
void route(double lane) {
    Simulation s;
    const auto gantry=s.kit_body_index(2952);
    require(gantry!=Simulation::kKitNone && s.kit_body_dynamic(gantry) && s.kit_body_mass(gantry)>1000,
            "gameplay gantry and receiver have finite native mass");
    settle(s);
    walk(s,lane,-114.0);
    (void)s.set_facing(0,-1);
    // Stop at the real CLIMB offer rather than driving through the mesh;
    // walking into holds while airborne can legitimately auto-grab them.
    (void)s.set_move_input(0,-0.25);
    for (int i=0; i<360 && !(s.snapshot().grip_available && s.snapshot().grip_entity_id==scraperx::sim::CargoNet::kEntity); ++i) tick(s);
    (void)s.set_move_input(0,0);
    require(s.snapshot().player_grounded && s.snapshot().player_position.y < 1.1,"entry is reached on grade without restart/teleport");
    require(s.snapshot().grip_available,"cargo mesh offers a real reachable native grip");
    (void)s.request_traversal(); tick(s);
    require((s.snapshot().traversal_state==scraperx::sim::TraversalState::Climbing),"Action takes the mesh through existing climbing authority");
    require(s.snapshot().player_gravity_factor==1.0,"soft hands retain actual rider gravity");
    require(s.snapshot().traversal_hand_constraint_count==2,"net climb has two finite physical hand attachments");
    const double before=s.snapshot().player_position.y;
    settle(s,90);
    require(s.snapshot().player_position.y-before<0.06,"no held motion cannot climb upward; physical sag is allowed");
    // Feet still carry the rider here. Loaded deformation is checked after
    // ordinary ascent clears the tread, not inferred from grounded grip drift.
    if(lane==20.0) {
        // A retained material coordinate is linear in the SAME two mesh
        // histories as the rendered vertices. This exercises the production
        // per-hand sampler without allocating a whole mesh in that sampler.
        int retained_samples=0;
        for(int sample=0;sample<12;++sample) {
            const auto before=s.snapshot();tick(s);const auto after=s.snapshot();
            require(s.advance_frame(Simulation::kFixedStepSeconds*.5).steps_advanced==0,"net half-tick is render only");
            for(bool left:{false,true}) {
                const auto a=left?before.traversal_left_hand:before.traversal_right_hand;
                const auto b=left?after.traversal_left_hand:after.traversal_right_hand;
                const auto rendered=s.render_traversal_hand(left);
                const auto generation_before=left?before.traversal_left_hand_generation:before.traversal_right_hand_generation;
                const auto generation_after=left?after.traversal_left_hand_generation:after.traversal_right_hand_generation;
                if(before.traversal_hand_constraint_count==2 && after.traversal_hand_constraint_count==2 &&
                   before.traversal_state==after.traversal_state &&
                   before.traversal_support_entity_id==after.traversal_support_entity_id &&
                   generation_before==generation_after) {
                    require(std::hypot(std::hypot(rendered.x-(a.x+b.x)*.5,rendered.y-(a.y+b.y)*.5),rendered.z-(a.z+b.z)*.5)<.00002,
                            "retained net material anchor uses the mesh half-tick history");
                    ++retained_samples;
                }
                // A changed material coordinate has different endpoints;
                // a world-point midpoint does not describe its mesh binding.
            }
            require(s.snapshot().tick_index==after.tick_index && s.snapshot().player_position.y==after.player_position.y,
                    "net render reads do not advance native player state");
            require(s.advance_frame(Simulation::kFixedStepSeconds*.5).steps_advanced==1,"finish net render half-tick");
        }
        require(retained_samples>=12,"net render check actually exercises retained mesh holds");
    }
    (void)s.set_move_input(0,-1);
    int climb_ticks=0;
    bool paused=false, transfer_action_exercised=false;
    for (int i=0; i<1800; ++i) {
        const auto previous=s.snapshot();
        if (!transfer_action_exercised && previous.traversal_support_entity_id==2952 &&
            previous.traversal_state==scraperx::sim::TraversalState::Climbing) {
            (void)s.request_traversal();
            transfer_action_exercised=true;
        }
        tick(s);
        require(s.snapshot().player_gravity_factor==1.0,"entire cargo climb and receiver transfer retain gravity");
        if(!paused && s.snapshot().player_position.y>4.5) { loaded_pause(s,lane); paused=true; }
        if((s.snapshot().traversal_state==scraperx::sim::TraversalState::Climbing)) ++climb_ticks;
        if (s.snapshot().player_grounded && s.snapshot().player_position.y > 11.7) break;
    }
    require(paused && transfer_action_exercised,"loaded pause and Action reach the actual receiving transfer");
    settle(s);
    auto p=s.snapshot();
    std::cout << "CARGO_TOP lane=" << lane << " position=" << p.player_position.x << ',' << p.player_position.y << ',' << p.player_position.z << " grounded=" << p.player_grounded << " climb_ticks=" << climb_ticks << " deaths=" << p.death_count << '\n';
    if(!p.player_grounded) {
        const auto vertices=s.cargo_net_vertices();
        for(int r=0;r<23;r+=3) { auto v=vertices[4*(r*9+4)];std::cout << "NET_DIAG row=" << r << " point=" << v.x << ',' << v.y << ',' << v.z << '\n'; }
        std::cout << "HAND_FORCE left=" << p.traversal_left_hand_force.y << "," << p.traversal_left_hand_force.z << " right=" << p.traversal_right_hand_force.y << "," << p.traversal_right_hand_force.z << " target=" << p.traversal_target_point.y << "," << p.traversal_target_point.z << " command_bound=" << p.traversal_command_work_bound_j << " gravity=" << p.player_gravity_factor << " attachments=" << p.traversal_hand_constraint_count << "\n";
        std::cout << "HANDS " << p.traversal_left_hand.y << ',' << p.traversal_left_hand.z << " / " << p.traversal_right_hand.y << ',' << p.traversal_right_hand.z << '\n';
    }
    require(p.player_grounded && !(p.traversal_state==scraperx::sim::TraversalState::Climbing) && p.player_position.y > 11.7 && p.player_position.y < 12.1,"held forward tops out onto real +11 m support");
    require(climb_ticks > 700 && p.climb_count > 0 && p.death_count==0,"ordinary hand-over-hand ascent has no launch or death");
    walk(s,18.5,-120.5); walk(s,18.5,-126.0);
    p=s.snapshot();
    require(p.player_grounded && p.support_entity_id==Simulation::kTowerEntityId && std::abs(p.player_position.y-11.9)<0.15,"wide receiver connects by walking to existing first tower ring");
    require(p.death_count==0 && s.slingshot_state().work_j==0,"slingshot is unused throughout alternative route");
    std::cout << "PASS AS-024 grade_to_first_ring lane=" << lane << " supported_height_m=11 deaths=0\n";
}
// Paired ordinary-input worlds distinguish a real material posture response
// from merely changing a command readback. No state edits follow construction.
void posture_entry(Simulation &s) {
    settle(s);walk(s,20,-114);
    require(s.set_facing(0,-1)&&s.set_move_input(0,-.25),"posture ordinary grade approach");
    int i=0;
    for(;i<360&&!(s.snapshot().grip_available&&s.snapshot().grip_entity_id==scraperx::sim::CargoNet::kEntity);++i)tick(s);
    require(i<360&&s.snapshot().player_grounded,"posture offer reached on actual grade");
    require(s.set_move_input(0,0)&&s.request_traversal(),"posture ordinary grounded Action");tick(s);
    require(s.snapshot().traversal_state==scraperx::sim::TraversalState::Climbing&&s.snapshot().traversal_hand_constraint_count==2,
            "posture actual two-hand material acquisition");
    require(s.set_move_input(0,-1),"posture ordinary ascent input");
    for(i=0;i<1200&&s.snapshot().player_position.y<=4.5;++i){tick(s);
        require(s.snapshot().traversal_state==scraperx::sim::TraversalState::Climbing&&s.snapshot().traversal_support_entity_id==1953&&
                s.snapshot().traversal_hand_constraint_count==2,"posture ascent retains actual material hands");}
    require(i<1200&&!s.snapshot().player_grounded&&s.set_move_input(0,0)&&s.set_crouch_input(false),
            "posture real airborne arrival and neutral command");
}
double posture_magnitude(scraperx::sim::Vector3 v){return std::hypot(std::hypot(v.x,v.y),v.z);}
void posture_held(const scraperx::sim::Snapshot &v,const scraperx::sim::Snapshot &start,bool requested){
    require(!v.player_grounded&&v.support_entity_id==0&&v.traversal_state==scraperx::sim::TraversalState::Climbing&&
            v.traversal_support_entity_id==1953&&v.traversal_hand_constraint_count==2&&v.player_gravity_factor==1&&v.death_count==0,
            "posture actual same-life airborne material hold");
    require(v.hanging_free_arc_active&&v.swing_tuck_available&&v.swing_tuck_requested==requested,
            "posture actual radial owner and explicit input");
    require(v.traversal_left_hand_generation==start.traversal_left_hand_generation&&
            v.traversal_right_hand_generation==start.traversal_right_hand_generation,"posture retains both material coordinates");
    for(const auto force:{v.traversal_left_hand_force,v.traversal_right_hand_force}){
        const double magnitude=posture_magnitude(force);
        require(std::isfinite(magnitude)&&magnitude<=1501,"posture finite bounded actual material force");}
    require(std::isfinite(v.hanging_actual_span_m)&&std::isfinite(v.hanging_commanded_span_m)&&
            std::isfinite(v.traversal_actuator_positive_work_j),"posture finite actual span and paid work");
}
struct PostureWindow {double mean_span=0,mean_rest=0,min_rest=1e9,max_rest=-1e9,peak_force=0;};
void material_posture() {
    Simulation treatment,control;posture_entry(treatment);posture_entry(control);
    const auto a=treatment.snapshot(),b=control.snapshot();
    require(a.tick_index==b.tick_index&&posture_magnitude({a.player_position.x-b.player_position.x,a.player_position.y-b.player_position.y,
            a.player_position.z-b.player_position.z})<1e-7&&posture_magnitude({a.player_linear_velocity.x-b.player_linear_velocity.x,
            a.player_linear_velocity.y-b.player_linear_velocity.y,a.player_linear_velocity.z-b.player_linear_velocity.z})<1e-7&&
            a.traversal_left_hand_generation==b.traversal_left_hand_generation&&a.traversal_right_hand_generation==b.traversal_right_hand_generation,
            "posture paired ordinary histories have matched initial tick pose velocity and grips");
    const auto initial_mesh=treatment.cargo_net_vertices();require(!initial_mesh.empty(),"posture actual material mesh");
    std::size_t vertex=0;double nearest=std::numeric_limits<double>::infinity();
    for(std::size_t j=0;j<initial_mesh.size();++j){const auto p=initial_mesh[j],h=a.traversal_left_hand;
        const double distance=posture_magnitude({p.x-h.x,p.y-h.y,p.z-h.z});if(distance<nearest){nearest=distance;vertex=j;}}
    double material_motion=0,base_rest=0,base_work=0;
    PostureWindow windows[3][2];
    for(int phase=0;phase<3;++phase){const bool tucked=phase==1;
        require(treatment.set_crouch_input(tucked),"posture explicit treatment command");
        auto previous_t=treatment.snapshot();
        double held_work=0;
        for(int i=0;i<180;++i){tick(treatment);tick(control);const auto t=treatment.snapshot(),c=control.snapshot();
            posture_held(t,a,tucked);posture_held(c,b,false);
            require(t.tick_index==c.tick_index,"posture common native sample tick");
            if(phase==0&&i==0){base_rest=t.hanging_commanded_span_m;base_work=t.traversal_actuator_positive_work_j;
                require(std::abs(base_rest-c.hanging_commanded_span_m)<1e-7,"posture matched initial radial length");}
            require(std::abs(c.hanging_commanded_span_m-base_rest)<.00001&&
                    c.traversal_command_work_bound_j==b.traversal_command_work_bound_j&&
                    c.traversal_actuator_positive_work_j==b.traversal_actuator_positive_work_j,
                    "posture neutral control has fixed rest and no extra work");
            const double paid=t.traversal_actuator_positive_work_j-previous_t.traversal_actuator_positive_work_j;
            require(paid>=0&&paid<=500*Simulation::kFixedStepSeconds+.00001,
                    "posture positive paid work is bounded and never refunded");
            if(phase==0)require(paid==0&&t.traversal_command_work_bound_j==a.traversal_command_work_bound_j,
                               "posture initial neutral treatment adds no work");
            if(i==134)held_work=t.traversal_actuator_positive_work_j;
            if(i>=135){require(t.traversal_actuator_positive_work_j==held_work,"posture held command adds no idle positive work");
                const scraperx::sim::Snapshot samples[]{t,c};
                for(int world=0;world<2;++world){auto &w=windows[phase][world];const auto &v=samples[world];
                    w.mean_span+=v.hanging_actual_span_m/45;w.mean_rest+=v.hanging_commanded_span_m/45;
                    w.min_rest=std::min(w.min_rest,v.hanging_commanded_span_m);w.max_rest=std::max(w.max_rest,v.hanging_commanded_span_m);}}
            for(int world=0;world<2;++world){auto &w=windows[phase][world];const auto &v=world?c:t;
                w.peak_force=std::max({w.peak_force,posture_magnitude(v.traversal_left_hand_force),posture_magnitude(v.traversal_right_hand_force)});}
            const auto mesh=treatment.cargo_net_vertices();require(mesh.size()==initial_mesh.size(),"posture retained mesh identity");
            material_motion=std::max(material_motion,posture_magnitude({mesh[vertex].x-initial_mesh[vertex].x,
                    mesh[vertex].y-initial_mesh[vertex].y,mesh[vertex].z-initial_mesh[vertex].z}));
            previous_t=t;
        }
        for(int world=0;world<2;++world){const auto &w=windows[phase][world];
            require(w.max_rest-w.min_rest<.001,"posture settled last45 length does not ratchet");
            std::cout<<"MATERIAL_POSTURE phase="<<phase<<" treatment="<<(world==0)<<" tick="<<treatment.snapshot().tick_index
                     <<" actual_mean="<<w.mean_span<<" rest_mean="<<w.mean_rest<<" held_rest_range="<<w.max_rest-w.min_rest
                     <<" peak_force="<<w.peak_force<<'\n';}
        const auto end=treatment.snapshot();
        require(std::abs(end.hanging_commanded_span_m-(base_rest-(tucked?.25:0)))<.001,
                "posture paid quarter-metre rest shortening and explicit restoration");
        if(tucked)require(end.traversal_actuator_positive_work_j>base_work,"posture actual tuck pays positive work");
        std::cout<<"MATERIAL_POSTURE paid_total="<<end.traversal_actuator_positive_work_j-base_work<<" phase="<<phase<<'\n';
    }
    require(windows[1][1].mean_span-windows[1][0].mean_span>.10,"posture actual paired radial body shortening exceeds ten centimetres");
    require(windows[2][0].mean_span-windows[1][0].mean_span>.10,"posture actual extension exceeds ten centimetres");
    require(material_motion>.015,"posture real retained material point deforms under hand load");
    const auto before=treatment.snapshot();require(treatment.request_release(),"posture ordinary Drop");tick(treatment);
    const auto released=treatment.snapshot();
    require(released.traversal_hand_constraint_count==0&&!released.hanging_free_arc_active&&!released.swing_tuck_available&&
            !released.swing_tuck_requested&&released.hanging_actual_span_m==0&&released.hanging_commanded_span_m==0&&
            released.swing_body_below_grip_m==0&&released.swing_commanded_body_below_grip_m==0&&released.player_gravity_factor==1,
            "posture Drop clears hands radial and posture readbacks while retaining gravity");
    std::cout<<"MATERIAL_POSTURE Drop before_vy="<<before.player_linear_velocity.y<<" after_vy="<<released.player_linear_velocity.y
             <<" before_vz="<<before.player_linear_velocity.z<<" after_vz="<<released.player_linear_velocity.z
             <<" material_motion="<<material_motion<<'\n';
    // Retain only the recent physical observations. A failed support assertion
    // must identify the receiving body and contact, especially across hosts.
    scraperx::sim::Snapshot drop_history[8]{};
    int drop_samples=0;
    const auto remember_drop=[&](const scraperx::sim::Snapshot &v){drop_history[drop_samples++%8]=v;};
    const auto trace_drop=[&](const char *reason){
        std::cerr<<"MATERIAL_POSTURE FIRST_FAILURE "<<reason<<" release_tick="<<released.tick_index<<'\n';
        for(int j=std::max(0,drop_samples-8);j<drop_samples;++j){const auto &v=drop_history[j%8];
            std::cerr<<"MATERIAL_POSTURE DROP_TRACE tick="<<v.tick_index<<" grounded="<<v.player_grounded
                     <<" support="<<v.support_entity_id<<" side="<<v.soft_side_contact_entity
                     <<" x="<<v.player_position.x<<" y="<<v.player_position.y<<" z="<<v.player_position.z
                     <<" vx="<<v.player_linear_velocity.x<<" vy="<<v.player_linear_velocity.y<<" vz="<<v.player_linear_velocity.z
                     <<" contact_x="<<v.support_contact_point.x<<" contact_y="<<v.support_contact_point.y
                     <<" contact_z="<<v.support_contact_point.z<<" hands="<<v.traversal_hand_constraint_count
                     <<" traversal="<<static_cast<int>(v.traversal_state)<<" gravity="<<v.player_gravity_factor
                     <<" landing_support="<<v.landing_support_entity_id<<" deaths="<<v.death_count<<'\n';}
    };
    remember_drop(released);
    bool fell=false,landed=false;
    for(int i=0;i<360;++i){tick(treatment);const auto v=treatment.snapshot();
        remember_drop(v);
        if(v.traversal_hand_constraint_count!=0||v.player_gravity_factor!=1||v.death_count!=0)
            trace_drop("posture Drop remains released and alive");
        require(v.traversal_hand_constraint_count==0&&v.player_gravity_factor==1&&v.death_count==0,"posture Drop remains released and alive");
        fell|=v.player_linear_velocity.y<-.1;
        if(v.player_grounded){
            // Release can touch the lower cloth or its real gantry tie rail
            // while descending. Neither intermediate contact proves arrival;
            // retain the deadline and require the actual grade tread below.
            if(v.support_entity_id==1953||v.support_entity_id==2952)continue;
            if(v.support_entity_id!=2954)trace_drop("posture actual grade tread receives Drop");
            require(v.support_entity_id==2954,"posture actual grade tread receives Drop");landed=true;break;}}
    if(!fell||!landed)trace_drop("posture ordinary Drop actually falls onto grade");
    require(fell&&landed,"posture ordinary Drop actually falls onto grade");
    std::cout<<"PASS material_posture paired=1 material=1953 actual_tuck_delta="<<windows[1][1].mean_span-windows[1][0].mean_span
             <<" actual_extension_delta="<<windows[2][0].mean_span-windows[1][0].mean_span<<" grade=2954 deaths=0\n";
}

}
int main(int argc,char **argv) {
    if(argc>1){if(std::string(argv[1])=="material_posture"){material_posture();return 0;}return 2;}
    route(19.0); route(20.0); route(21.0);
}
