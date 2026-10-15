#include "sim/simulation.hpp"
#include "sim/split_inspection_route.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
using scraperx::sim::TraversalState;
constexpr auto kWorld = Simulation::kWorldSolidEntityId;
constexpr auto kCrosshead = scraperx::sim::kSplitInspectionCrossheadEntity;
constexpr auto kReceiver = scraperx::sim::kSplitInspectionReceiverEntity;
constexpr auto kHatch = scraperx::sim::kSplitInspectionHatchEntity;
constexpr auto kGirder = scraperx::sim::kSplitInspectionGirderEntity;
const char *phase = "roof462";

void sample(const Simulation &s, const char *event) {
    const auto v = s.snapshot();
    const auto hatch_q = s.kit_body_rotation(s.kit_body_index(kHatch));
    std::cout << "SPLIT_INSPECTION_CSV," << phase << ',' << event << ',' << v.tick_index << ','
              << v.player_position.x << ',' << v.player_position.y << ',' << v.player_position.z << ','
              << v.player_linear_velocity.x << ',' << v.player_linear_velocity.y << ','
              << v.player_linear_velocity.z << ',' << v.player_grounded << ',' << v.support_entity_id << ','
              << v.player_crouched << ',' << v.player_balancing << ',' << v.death_count << ','
              << v.landing_count << ',' << v.last_impact_speed_mps << ',' << v.landing_jump_work_j << ','
              << v.checkpoint_footing_valid << ',' << hatch_q.x << ',' << hatch_q.y << ','
              << hatch_q.z << ',' << hatch_q.w << std::endl;
}

void require(bool condition, const char *why, const Simulation &s) {
    if (condition) return;
    sample(s, "failure");
    // Capture the failing host's real apparatus state. A passing ARM replay
    // cannot diagnose an x86 contact loss, and this receipt grants no support.
    std::cerr << std::setprecision(12);
    for (const auto entity : {kGirder, kHatch,
            scraperx::sim::kSplitInspectionSpanSeatEntity,
            scraperx::sim::kSplitInspectionBalanceBlockEntity}) {
        const auto body = s.kit_body_index(entity);
        const auto p = s.kit_body_position(body), v = s.kit_body_velocity(body);
        const auto q = s.kit_body_rotation(body);
        std::cerr << "SPLIT_FAILURE_BODY entity=" << entity << " index=" << body
                  << " position=" << p.x << ',' << p.y << ',' << p.z
                  << " rotation=" << q.x << ',' << q.y << ',' << q.z << ',' << q.w
                  << " velocity=" << v.x << ',' << v.y << ',' << v.z
                  << " kinetic_j=" << s.kit_body_kinetic_energy(body) << '\n';
    }
    const auto girder = s.kit_body_index(kGirder);
    const auto seat = s.kit_body_index(scraperx::sim::kSplitInspectionSpanSeatEntity);
    if (s.kit_body_part_count(girder) > 1 && s.kit_body_part_count(seat) > 0) {
        const auto as_vec = [](scraperx::sim::Vector3 p) {
            return JPH::Vec3(float(p.x), float(p.y), float(p.z));
        };
        const auto as_quat = [](scraperx::sim::Quaternion q) {
            return JPH::Quat(float(q.x), float(q.y), float(q.z), float(q.w));
        };
        const auto gp = s.kit_body_part(girder, 1), sp = s.kit_body_part(seat, 0);
        const auto gq = as_quat(s.kit_body_rotation(girder));
        const auto sq = as_quat(s.kit_body_rotation(seat));
        const auto normal = gq * as_quat(gp.rotation) * JPH::Vec3::sAxisY();
        const auto bottom = as_vec(s.kit_body_position(girder)) +
            gq * (as_vec(gp.offset) + as_quat(gp.rotation) * JPH::Vec3(0, -float(gp.half.y), 0));
        const auto top = as_vec(s.kit_body_position(seat)) +
            sq * (as_vec(sp.offset) + as_quat(sp.rotation) * JPH::Vec3(0, float(sp.half.y), 0));
        std::cerr << "SPLIT_FAILURE_SEAT contact=" << s.inspection_span_seat_contact()
                  << " underside_plane_gap_m=" << (bottom - top).Dot(normal)
                  << " span_part=1 seat_part=0 span_normal_y=" << normal.GetY() << '\n';
    }
    std::cerr << "FAIL SPLIT_INSPECTION phase=" << phase << " reason=" << why << '\n';
    std::exit(EXIT_FAILURE);
}

void begin(Simulation &s, const char *next) {
    phase = next;
    sample(s, "begin");
}

void tick(Simulation &s, int count = 1) {
    while (count-- > 0) {
        const auto r = s.advance_frame(Simulation::kFixedStepSeconds);
        require(r.accepted && r.steps_advanced == 1, "one native90Hz step accepted", s);
        const auto v = s.snapshot();
        require(std::isfinite(v.player_position.x) && std::isfinite(v.player_position.y) &&
                std::isfinite(v.player_position.z) && std::isfinite(v.player_linear_velocity.x) &&
                std::isfinite(v.player_linear_velocity.y) && std::isfinite(v.player_linear_velocity.z),
                "finite native motion", s);
        require(v.death_count == 0 && v.player_gravity_factor == 1 && !v.parachute_deployed,
                "same life and ordinary gravity", s);
        if (v.tick_index % 90 == 0) sample(s, "tick");
    }
}

void steer(Simulation &s, double x, double z, double extra_x = 0, double extra_z = 0) {
    const auto v = s.snapshot();
    const double dx = x-v.player_position.x, dz = z-v.player_position.z;
    const double ix = 1.8*dx-.28*v.player_linear_velocity.x+extra_x;
    const double iz = 1.8*dz-.28*v.player_linear_velocity.z+extra_z;
    const double scale = std::max(1.0, std::hypot(ix, iz));
    require(s.set_move_input(ix/scale, iz/scale), "ordinary steering accepted", s);
    if (std::hypot(dx, dz) > .02)
        require(s.set_facing(dx, dz), "ordinary facing accepted", s);
}

void walk(Simulation &s, double x, double z, const char *next, int budget = 2700) {
    begin(s, next);
    double ex = 0, ez = 0;
    double best_distance = std::hypot(x-s.snapshot().player_position.x, z-s.snapshot().player_position.z);
    int blocked_ticks = 0;
    for (int i = 0; i < budget; ++i) {
        const auto v = s.snapshot();
        const double dx = x-v.player_position.x, dz = z-v.player_position.z;
        // The600mm ballast pocket accepts the ordinary100mm walking arrival.
        // A carried pendulum need not be motionless before a deliberate drop.
        if (v.player_grounded && std::hypot(dx, dz) < .10 &&
            std::hypot(v.player_linear_velocity.x-v.support_point_linear_velocity.x,
                       v.player_linear_velocity.z-v.support_point_linear_velocity.z) < .14) {
            require(s.set_move_input(0, 0), "arrival releases stick", s);
            sample(s, "arrived");
            return;
        }
        if (std::hypot(dx, dz) > .75) ex = ez = 0;
        else {
            ex += dx*Simulation::kFixedStepSeconds*.4;
            ez += dz*Simulation::kFixedStepSeconds*.4;
            const double scale = std::max(1.0, std::hypot(ex, ez)/.35);
            ex /= scale; ez /= scale;
        }
        const double distance = std::hypot(dx, dz);
        const double effort = std::hypot(1.8*dx-.28*v.player_linear_velocity.x+ex,
                                         1.8*dz-.28*v.player_linear_velocity.z+ez);
        if (distance < best_distance-.025) { best_distance = distance; blocked_ticks = 0; }
        else if (distance > .3 && effort > .65) ++blocked_ticks;
        else blocked_ticks = 0;
        require(blocked_ticks < 180, "strong ordinary walking blocked for180ticks at actual sampled point", s);
        steer(s, x, z, ex, ez);
        tick(s);
    }
    require(false, "bounded ordinary walking did not reach target", s);
}

bool footing(const Snapshot &v, std::uint64_t support, double centre_y) {
    return v.player_grounded && v.support_entity_id == support && !v.player_crouched &&
           std::abs(v.player_position.y-centre_y) < .15;
}

void stable(Simulation &s, std::uint64_t support, double centre_y) {
    require(s.set_move_input(0, 0), "neutral footing input accepted", s);
    for (int i = 0; i < 90; ++i) {
        tick(s);
        require(footing(s.snapshot(), support, centre_y), "actual receiver retains standing contact", s);
    }
    require(s.snapshot().checkpoint_footing_valid, "native resting support patch established", s);
    sample(s, "stable");
}

// Native Kit pose transforms the authored hatch face. This is observation,
// never a target angle or a simulation write; the player load owns its motion.
scraperx::sim::Vector3 hatch_point(const Simulation &s, const JPH::Vec3 local) {
    const auto p = s.kit_body_position(s.kit_body_index(kHatch));
    const auto q = s.kit_body_rotation(s.kit_body_index(kHatch));
    const auto r = JPH::Quat(q.x, q.y, q.z, q.w)*local;
    return {p.x+r.GetX(), p.y+r.GetY(), p.z+r.GetZ()};
}

double hatch_pitch(const Simulation &s) {
    const auto pivot = hatch_point(s, {0, 0, 0});
    const auto forward = hatch_point(s, {0, 0, 1});
    return std::atan2(pivot.y-forward.y, forward.z-pivot.z);
}

bool hatch_footing(const Simulation &s) {
    const auto v = s.snapshot();
    const auto face = hatch_point(s, {0, .05F, 0});
    const auto up = hatch_point(s, {0, 1.05F, 0});
    const double nx = up.x-face.x, ny = up.y-face.y, nz = up.z-face.z;
    const double height = face.y-(nx*(v.player_position.x-face.x)+nz*(v.player_position.z-face.z))/ny;
    return v.player_grounded && v.support_entity_id == kHatch && !v.player_crouched &&
           ny > .8 && std::abs(v.player_position.y-height-.9) < .18;
}

void free_flight(const Simulation &s) {
    const auto v = s.snapshot();
    require(v.traversal_state == TraversalState::None && v.traversal_hand_constraint_count == 0,
            "gap Jump is ballistic without hand-assisted transfer", s);
}

void approach(Simulation &s) {
    walk(s, -19.5, -130.0, "roof462_north_band");
    walk(s, 0, -130.0, "roof462_to_brace_toe");
    walk(s, 0, -128.55, "original_brace_toe");
    walk(s, -19.3, -128.55, "original_brace_ascent", 4500);
    const auto start = s.snapshot();
    require(start.player_grounded && start.support_entity_id == kWorld && start.player_position.y > 471.5,
            "original brace contact earns entry height", s);
    begin(s, "ordinary_crosshead_jump");
    require(s.set_facing(0, -1) && s.set_move_input(0, 0) && s.request_jump(),
            "single supported entry Jump accepted", s);
    bool airborne = false, landed = false;
    for (int i = 0; i < 360; ++i) {
        steer(s, -19.3, -129.5);
        tick(s);
        const auto v = s.snapshot();
        airborne |= !v.player_grounded && v.support_entity_id == 0;
        if (airborne && footing(v, kCrosshead, 474.15)) { landed = true; break; }
    }
    require(airborne && landed && s.snapshot().landing_jump_work_j > start.landing_jump_work_j,
            "finite brace push-off lands on actual crosshead", s);
    walk(s, -19.3, -129.5, "crosshead_braking");
    stable(s, kCrosshead, 474.15);
}

void cross_gap(Simulation &s) {
    const double unloaded_pitch = hatch_pitch(s);
    walk(s, -19.3, -129.5, "gap_runup_start");
    begin(s, "south_gap_runup");
    require(s.set_facing(0, -1) && s.set_move_input(0, -1), "ordinary southward runup accepted", s);
    bool requested = false;
    for (int i = 0; i < 180; ++i) {
        const auto v = s.snapshot();
        if (v.player_position.z <= -130.05) {
            require(footing(v, kCrosshead, 474.15) && v.player_position.z > -130.2 &&
                    v.player_linear_velocity.z < -2.5, "gap takeoff has actual crosshead contact and momentum", s);
            require(s.request_jump(), "single supported gap Jump accepted", s);
            requested = true; sample(s, "takeoff"); break;
        }
        tick(s);
    }
    require(requested, "runup reaches supported gap takeoff", s);
    const auto start = s.snapshot();
    begin(s, "unsupported_gap_crossing");
    bool airborne = false, over_gap = false, landed = false;
    for (int i = 0; i < 360; ++i) {
        steer(s, -19.3, -132.9);
        tick(s);
        const auto v = s.snapshot();
        free_flight(s);
        require(v.jump_vault_count == start.jump_vault_count, "gap uses one ballistic Jump without legacy vault", s);
        airborne |= !v.player_grounded && v.support_entity_id == 0;
        if (v.player_position.z < -130.75 && v.player_position.z > -131.75) {
            require(!v.player_grounded && v.support_entity_id == 0, "actual gap has no intermediate footing", s);
            over_gap = true;
        }
        if (airborne && v.player_grounded) {
            require(hatch_footing(s) && v.player_position.z < -132.3,
                    "gap flight lands on real south receiver", s);
            landed = true; break;
        }
    }
    require(airborne && over_gap && landed && s.snapshot().landing_count > start.landing_count &&
            s.snapshot().landing_jump_work_j > start.landing_jump_work_j,
            "single ordinary Jump crosses open gap and earns actual receiver", s);
    walk(s, -19.3, -132.9, "moving_leaf_landing_braking");
    begin(s, "actual_player_loaded_hatch_pitch");
    double loaded_pitch = hatch_pitch(s);
    for (int i = 0; i < 180; ++i) {
        steer(s, -19.3, -132.9);
        tick(s);
        require(hatch_footing(s), "player remains on real load-sensitive leaf", s);
        loaded_pitch = std::max(loaded_pitch, hatch_pitch(s));
        if (loaded_pitch > unloaded_pitch+.015) break;
    }
    // The coupled massive span delays the leaf response. Its complete
    // loaded stroke and empty restoration are checked in the ballast loop.
    sample(s, "actual_loaded_leaf");
    // Walk toward the actual hinge, reducing the rider lever arm before the
    // fixed rear strip and girder. The counterweight must restore itself.
    walk(s, -19.3, -134.45, "moving_leaf_to_fixed_hinge_strip");
    stable(s, kReceiver, 474.15);

    std::cout << "SPLIT_INSPECTION_HATCH,unloaded_pitch=" << unloaded_pitch
              << ",loaded_pitch=" << loaded_pitch << ",transferred_pitch=" << hatch_pitch(s) << '\n';
}

void miss_and_retry(Simulation &s) {
    walk(s, -19.3, -129.5, "short_jump_supported_start");
    const auto start = s.snapshot();
    require(footing(start, kCrosshead, 474.15), "miss starts on earned crosshead", s);
    begin(s, "deliberately_short_gap_jump");
    require(s.set_facing(0, -1) && s.set_move_input(0, -.2) && s.request_jump(),
            "ordinary short Jump accepted", s);
    bool airborne = false, descended = false, landed = false;
    for (int i = 0; i < 720; ++i) {
        steer(s, -19.3, -131.25);
        tick(s);
        const auto v = s.snapshot();
        airborne |= !v.player_grounded && v.support_entity_id == 0;
        free_flight(s);
        descended |= airborne && v.player_position.y < 472.5;
        if (airborne && v.player_grounded) {
            require(descended && footing(v, kWorld, 463.15), "short miss reaches actual462 roof", s);
            landed = true; break;
        }
    }
    require(airborne && descended && landed && s.snapshot().landing_count > start.landing_count &&
            s.snapshot().last_impact_speed_mps > 8,
            "real gap miss produces nontrivial roof impact on same life", s);
    sample(s, "roof462_impact");
    walk(s, -19.3, -131.25, "roof_recovery_braking");
    tick(s, 180);
    stable(s, kWorld, 463.15);
    approach(s);
    cross_gap(s);
}

scraperx::sim::Vector3 span_toe(const Simulation&s){
 const auto i=s.kit_body_index(kGirder);const auto p=s.kit_body_position(i);const auto q=s.kit_body_rotation(i);
 const JPH::Quat rotation(float(q.x),float(q.y),float(q.z),float(q.w));
 const auto offset=rotation*(scraperx::sim::kSplitInspectionGirderToe-JPH::Vec3(scraperx::sim::kSplitInspectionSpanPivot));
 return {p.x+offset.GetX(),p.y+offset.GetY(),p.z+offset.GetZ()};
}
bool actual_span_seat_contact(const Simulation&s){
 return s.inspection_span_seat_contact();
}
void drop_balance_block(Simulation&s){
 constexpr auto block=scraperx::sim::kSplitInspectionBalanceBlockEntity;
 require(s.set_facing(0,1)&&s.set_move_input(0,0),"face actual ballast pocket",s);
 // Follow the real load through its finite carry turn. Project its centre
 // along gravity onto the actual tilted hatch, rather than dropping after
 // an arbitrary turn delay while it may still hang beside the pocket.
 bool aligned=false;
 for(int i=0;i<900;++i){
  tick(s);require(s.snapshot().carrying_entity_id==block,"placement retains original carried load",s);
  const auto hp=s.kit_body_position(s.kit_body_index(kHatch));
  const auto hq=s.kit_body_rotation(s.kit_body_index(kHatch));
  const JPH::Quat q(float(hq.x),float(hq.y),float(hq.z),float(hq.w));
  const auto bp=s.kit_body_position(s.kit_body_index(block));
  const auto bv=s.kit_body_velocity(s.kit_body_index(block));
  const auto local=q.Conjugated()*JPH::Vec3(float(bp.x-hp.x),float(bp.y-hp.y),float(bp.z-hp.z));
  const auto up=q.Conjugated()*JPH::Vec3::sAxisY();
  const auto landing=local-up*((local.GetY()-.13F)/up.GetY());
  if(i%180==0)std::cout<<"BALANCE_AIM tick="<<s.snapshot().tick_index<<" local_landing="<<landing.GetX()<<','<<landing.GetZ()<<" speed="<<std::hypot(bv.x,bv.z)<<std::endl;
  if(std::abs(landing.GetX())<.16&&landing.GetZ()>1.54&&landing.GetZ()<1.86&&std::hypot(bv.x,bv.z)<.3){aligned=true;break;}
 }
 require(aligned,"ordinary carry turn puts actual load over retaining pocket",s);
 require(s.request_set_down(),"ordinary drop onto moving hatch",s);tick(s);
 require(s.snapshot().carrying_entity_id==0,"drop removes carry constraint",s);
}
void assist_span_settling(Simulation&s){
 begin(s,"stand_with_ballast_while_span_lowers");
 require(s.set_move_input(0,0),"neutral ordinary stance on loaded hatch",s);
 int settled=0;double heights[180]{};double previous=span_toe(s).y;
 for(int i=0;i<3600;++i){
  tick(s);const auto v=s.snapshot();
  require(v.player_grounded&&v.support_entity_id==kHatch,"operator remains on actual loaded hatch",s);
  if(i%180==0)std::cout<<"BALANCE_OPERATOR tick="<<v.tick_index<<" toe="<<span_toe(s).y<<" seat_contact="<<actual_span_seat_contact(s)<<" span_ke="<<s.kit_body_kinetic_energy(s.kit_body_index(kGirder))<<std::endl;
  const double height=span_toe(s).y;
  const double speed=std::abs(height-previous)/Simulation::kFixedStepSeconds;previous=height;
  if(actual_span_seat_contact(s)&&std::abs(height-scraperx::sim::kSplitInspectionTopY)<.08&&speed<.02)
   heights[settled++]=height;
  else settled=0;
  if(settled>=180){
   const auto range=std::minmax_element(heights,heights+180);
   if(*range.second-*range.first<.02){sample(s,"operator_assisted_span_settled");return;}
   settled=0;
  }
 }
 require(false,"ordinary operator load lowers and settles real span",s);
}
std::uint32_t balance_cable(const Simulation&s){
 for(std::uint32_t i=0;i<s.kit_cable_count();++i){
  scraperx::sim::Vector3 points[4];if(s.kit_cable_points(i,points,4)!=4)continue;
  bool lower=false,upper=false;
  for(const auto&p:points){
   lower|=std::hypot(p.x+19.35,p.y-471.8)<.01&&std::abs(p.z+135.2)<.01;
   upper|=std::hypot(p.x-7.9,p.y-481.5)<.01&&std::abs(p.z+135.2)<.01;
  }
  if(lower&&upper)return i;
 }
 require(false,"real drawbridge cable has both authored guides",s);return 0;
}
void fixed_ballast_wait(const Simulation&s,std::uint32_t cable){
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==kReceiver,"ballast works while player stays on real fixed waiting pad",s);
 scraperx::sim::Vector3 points[4];require(s.kit_cable_points(cable,points,4)==4,"drawbridge cable stays physically intact",s);
}
void place_balance_block(Simulation&s){
 constexpr auto block=scraperx::sim::kSplitInspectionBalanceBlockEntity;
 begin(s,"counterweight_pickup");
 const auto cable=balance_cable(s);
 for(const auto entity : {block, scraperx::sim::kSplitInspectionBearingEntity,
                          scraperx::sim::kSplitInspectionSpanSeatEntity,
                          scraperx::sim::kSplitInspectionCableFrameEntity})
  require(s.entity_body_count(entity)==1,"encounter entity has one ordinary-world body",s);
 require(s.set_facing(-1,0)&&s.set_crouch_input(true),"deliberate reach toward block",s);tick(s,30);
 const auto bi=s.kit_body_index(block);const auto bp=s.kit_body_position(bi);const auto bv=s.kit_body_velocity(bi);
 std::cout<<"BALANCE_PICKUP actual_target="<<s.snapshot().carry_target_entity_id<<" block="<<bp.x<<','<<bp.y<<','<<bp.z<<" velocity="<<bv.x<<','<<bv.y<<','<<bv.z<<std::endl;
 require(s.snapshot().carry_target_entity_id==block,"real block is contextual reachable pickup",s);
 require(s.request_pick_up(),"ordinary block pickup",s);tick(s,15);
 require(s.snapshot().carrying_entity_id==block,"real carry constraint holds balance block",s);
 require(s.set_crouch_input(false),"stand with block",s);tick(s,30);
 walk(s,-19.35,-133.05,"counterweight_drop_alignment");
 drop_balance_block(s);
 walk(s,-19.35,-133.45,"clear_placed_ballast_before_loading_hatch");
 assist_span_settling(s);
 walk(s,-19.35,-134.75,"step_away_from_placed_counterweight");stable(s,kReceiver,474.15);
 begin(s,"counterweight_lowers_long_span");
 bool seated=false;
 for(int i=0;i<5400;++i){tick(s);const auto toe=span_toe(s);
  if(i%180==0){const auto bp=s.kit_body_position(s.kit_body_index(block));std::cout<<"BALANCE_MOTION tick="<<s.snapshot().tick_index<<" toe_y="<<toe.y<<" hatch_pitch="<<hatch_pitch(s)<<" block="<<bp.x<<','<<bp.y<<','<<bp.z<<'\n';}
  fixed_ballast_wait(s,cable);
  if(std::abs(toe.y-scraperx::sim::kSplitInspectionTopY)<.08&&hatch_pitch(s)>.12&&actual_span_seat_contact(s)){seated=true;break;}
 }
 require(seated,"placed block lowers actual28m span onto receiving seat after player leaves",s);
 const auto hold_start=span_toe(s);
 for(int i=0;i<450;++i){tick(s);fixed_ballast_wait(s,cable);require(std::abs(span_toe(s).y-scraperx::sim::kSplitInspectionTopY)<.08&&actual_span_seat_contact(s),"physical ballast keeps span at actual seat without player load",s);}
 require(std::abs(span_toe(s).y-hold_start.y)<.02,"seated span settles rather than merely passing useful height",s);
 sample(s,"ballast_alone_holds_span");
 // Recover the same real body; no inventory grant or relocation repairs it.
 walk(s,-19.35,-133.05,"counterweight_recovery_approach");
 require(s.set_facing(0,1)&&s.set_crouch_input(true),"reach placed block",s);tick(s,30);
 require(s.snapshot().carry_target_entity_id==block,"placed ballast remains reachable for recovery",s);
 require(s.request_pick_up(),"ordinary recovery pickup",s);tick(s,15);
 require(s.snapshot().carrying_entity_id==block,"recovery holds original physical weight",s);
 require(s.set_crouch_input(false),"stand after recovery",s);tick(s,30);
 walk(s,-19.35,-134.75,"interrupt_counterweight_operation");stable(s,kReceiver,474.15);
 bool restored=false;
 for(int i=0;i<5400;++i){tick(s);require(s.snapshot().carrying_entity_id==block,"interruption preserves held weight",s);
  if(span_toe(s).y>474.8&&hatch_pitch(s)<.06){restored=true;break;}}
 require(restored,"removing ballast physically restores raised span",s);
 require(!actual_span_seat_contact(s),"raised span has actually left the receiving seat",s);
 sample(s,"weight_recovered_span_restored");
 walk(s,-19.35,-133.05,"counterweight_drop_alignment");
 drop_balance_block(s);
 walk(s,-19.35,-133.45,"clear_retry_ballast_before_loading_hatch");
 assist_span_settling(s);
 walk(s,-19.35,-134.75,"retry_step_away");stable(s,kReceiver,474.15);
 restored=false;
 for(int i=0;i<5400;++i){tick(s);fixed_ballast_wait(s,cable);if(std::abs(span_toe(s).y-scraperx::sim::kSplitInspectionTopY)<.08&&hatch_pitch(s)>.12&&actual_span_seat_contact(s)){restored=true;break;}}
 require(restored,"same-life recovered weight lowers span again",s);
 std::cout<<"PASS BALANCE_BLOCK_LOOP original_body_recovered=1 ordinary_retry=1 physical_restore=1 no_relocation=1\n";
}
void onward(Simulation &s) {
    walk(s, -19.3, -134.15, "receiver_turn_to_girder");
    walk(s, -17.8, -134.15, "actual_narrow_girder_toe");
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == kGirder,
            "ordinary turn reaches actual narrow girder contact", s);
    walk(s, -4, -134.15, "narrow_girder_ascent");
    require(s.set_crouch_input(true), "ordinary crouch before low conduit accepted", s);
    walk(s, -3, -134.15, "actual_conduit_duckunder");
    require(s.snapshot().player_crouched && s.snapshot().support_entity_id == kGirder,
            "compact capsule passes conduit on narrow real footing", s);
    require(s.set_crouch_input(false), "ordinary stand request under conduit accepted", s);
    tick(s, 30);
    require(s.snapshot().player_crouched, "actual conduit clearance rejects standing", s);
    require(s.set_crouch_input(true), "ordinary crouch continues passage", s);
    walk(s, -2, -134.15, "conduit_clearance_exit");
    require(s.set_crouch_input(false), "ordinary standing after conduit accepted", s);
    tick(s, 45);
    require(!s.snapshot().player_crouched && s.snapshot().player_grounded &&
            s.snapshot().support_entity_id == kGirder, "clear girder permits standing", s);
    // Turn on the west half of the actual bearing shoe, before the
    // counterweight arm rises from the pivot. Keep normal capsule clearance.
    walk(s, 9.8, -134.15, "narrow_ascent_to_roof_shoe", 4500);
    require(footing(s.snapshot(), scraperx::sim::kSplitInspectionBearingEntity, 485.15), "fixed bearing shoe earns484 roof height", s);
    walk(s, 9.8, -132.6, "actual484_roof_exit");
    stable(s, kWorld, 485.15);
}

void route(const std::string &mode) {
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::Slingshot);
    require(s.entity_body_count(kCrosshead) == 1 && s.entity_body_count(kReceiver) == 1 &&
            s.entity_body_count(kGirder) == 1 && s.entity_body_count(kHatch) == 1, "route exists in ordinary Slingshot world", s);
    // The only staging operation: the actual roof earned by the preceding route.
    require(s.debug_restart_at({-19.5, 463.2, -142}), "single actual462 roof staging accepted", s);
    require(s.set_move_input(0, 0) && s.set_crouch_input(false) && s.set_sprint_input(false),
            "ordinary initial stance accepted", s);
    tick(s, 45);
    stable(s, kWorld, 463.15);
    approach(s);
    if (mode == "approach") {
        std::cout << "PASS SPLIT_INSPECTION approach ordinary_inputs_only=1 initial_staging_only=1\n";
        return;
    }
    if (mode == "miss-retry") miss_and_retry(s);
    else cross_gap(s);
    place_balance_block(s);
    onward(s);
    std::cout << "PASS SPLIT_INSPECTION mode=" << mode
              << " actual_gap_airborne=1 actual_receiver=1 loaded_hatch_motion=1 hinge_transfer=1 conduit_crouch=1 actual484roof=1"
                 " ordinary_inputs_only=1 initial_staging_only=1 deaths=0 miss_retry="
              << (mode == "miss-retry") << '\n';
}
} // namespace

int main(int argc, char **argv) {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "SPLIT_INSPECTION_CSV_HEADER,phase,event,tick,x,y,z,vx,vy,vz,grounded,support,"
                 "crouch,balance,deaths,landings,impact_mps,jump_work_j,firm_footing,hatch_qx,hatch_qy,hatch_qz,hatch_qw\n";
    const std::string mode = argc > 1 ? argv[1] : "primary";
    if (mode != "primary" && mode != "approach" && mode != "miss-retry" && mode != "counterweight") {
        std::cerr << "FAIL SPLIT_INSPECTION unknown mode\n";
        return EXIT_FAILURE;
    }
    route(mode);
    return EXIT_SUCCESS;
}
