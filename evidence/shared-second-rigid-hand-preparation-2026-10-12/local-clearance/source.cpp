#include "sim/simulation.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>
using namespace scraperx::sim;
namespace {
Vector3 add(Vector3 a,Vector3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vector3 sub(Vector3 a,Vector3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vector3 scale(Vector3 a,double n){return {a.x*n,a.y*n,a.z*n};}
double dot(Vector3 a,Vector3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vector3 cross(Vector3 a,Vector3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double length(Vector3 a){return std::sqrt(dot(a,a));}
Vector3 rotate(Quaternion q,Vector3 v){const auto t=scale(cross({q.x,q.y,q.z},v),2);return add(v,add(scale(t,q.w),cross({q.x,q.y,q.z},t)));}
void vec(Vector3 v){std::cout<<v.x<<','<<v.y<<','<<v.z;}
struct Box {unsigned entity,part;Vector3 centre;std::array<Vector3,3> axis;std::array<double,3> half;bool hold;unsigned along;};
Box box(Simulation &s,unsigned entity,std::size_t b,std::size_t i){auto p=s.kit_body_part(b,i);auto q=s.kit_body_rotation(b);
 Box a{entity,static_cast<unsigned>(i),add(s.kit_body_position(b),rotate(q,p.offset)),
 {rotate(q,rotate(p.rotation,{1,0,0})),rotate(q,rotate(p.rotation,{0,1,0})),rotate(q,rotate(p.rotation,{0,0,1}))},
 {p.half.x,p.half.y,p.half.z},false,0};
 for(unsigned j=1;j<3;++j)if(a.half[j]>a.half[a.along])a.along=j;
 a.hold=p.shape==0&&2*a.half[a.along]>=.25;
 for(unsigned j=0;j<3;++j)if(j!=a.along&&2*a.half[j]>.180001)a.hold=false;
 return a;}
bool overlap(const Box &a,Vector3 centre,Vector3 half){
 const std::array<Vector3,3> axes{{{1,0,0},{0,1,0},{0,0,1}}};
 std::array<Vector3,15> test{};for(unsigned i=0;i<3;++i){test[i]=a.axis[i];test[3+i]=axes[i];for(unsigned j=0;j<3;++j)test[6+3*i+j]=cross(a.axis[i],axes[j]);}
 for(auto n:test){if(dot(n,n)<1e-20)continue;double r=std::abs(n.x)*half.x+std::abs(n.y)*half.y+std::abs(n.z)*half.z;
 for(unsigned i=0;i<3;++i){r+=std::abs(dot(n,a.axis[i]))*a.half[i];}if(std::abs(dot(sub(a.centre,centre),n))>r)return false;}
 return true;}
double intersection(const Box &a,Vector3 start,Vector3 finish){const auto d=sub(finish,start),o=sub(start,a.centre);double lo=0,hi=1;
 for(unsigned j=0;j<3;++j){double x=dot(o,a.axis[j]),dx=dot(d,a.axis[j]);if(std::abs(dx)<1e-12){if(std::abs(x)>a.half[j])return 2;}
 else{double l=(-a.half[j]-x)/dx,h=(a.half[j]-x)/dx;if(l>h)std::swap(l,h);lo=std::max(lo,l);hi=std::min(hi,h);if(lo>hi)return 2;}}
 return hi>0&&lo<1?lo:2;}
const Box *first(const std::vector<Box>& boxes,Vector3 start,Vector3 end,double &fraction){fraction=2;const Box *result=nullptr;
 const auto d=sub(end,start);end=add(end,scale(d,.02/length(d)));
 for(const auto &b:boxes){double f=intersection(b,start,end);if(f<fraction){fraction=f;result=&b;}}return result;}
void tick(Simulation&s){auto r=s.advance_frame(Simulation::kFixedStepSeconds);if(!r.accepted||r.steps_advanced!=1){std::cerr<<"SETUP_FAIL tick\n";std::exit(2);}}
void state(const char *name,const Snapshot&v){std::cout<<"STATE,"<<name<<",tick="<<v.tick_index<<",com=";vec(v.player_position);std::cout<<",velocity=";vec(v.player_linear_velocity);
 std::cout<<",grounded="<<v.player_grounded<<",foot="<<v.support_entity_id<<",cue="<<v.grip_available<<",cue_entity="<<v.grip_entity_id<<",cue_point=";vec(v.grip_point);
 std::cout<<",state="<<int(v.traversal_state)<<",owner="<<v.traversal_support_entity_id<<",hands="<<v.traversal_hand_constraint_count<<",generations="<<v.traversal_left_hand_generation<<','<<v.traversal_right_hand_generation<<",gravity="<<v.player_gravity_factor<<",deaths="<<v.death_count<<'\n';}
}
int main(){std::cout<<std::setprecision(12);Simulation s;for(unsigned i=0;i<180;++i)tick(s);
 if(!s.debug_restart_at({16.85,11.50,-118.0})||!s.set_facing(1,0)||!s.set_move_input(0,0)||!s.set_crouch_input(false)){std::cerr<<"SETUP_FAIL initial\n";return 2;}tick(s);state("cue",s.snapshot());
 if(!s.snapshot().grip_available||s.snapshot().grip_entity_id!=2952||s.snapshot().player_grounded||!s.request_traversal()){std::cerr<<"SETUP_FAIL ordinaryAction\n";return 2;}
 tick(s);const auto entry=s.snapshot();state("catch",entry);
 if(entry.traversal_hand_constraint_count!=1||entry.traversal_left_hand_generation==0||entry.traversal_right_hand_generation!=0||!entry.hanging_free_arc_active){std::cerr<<"SETUP_FAIL expected single left\n";return 2;}
 const auto body=s.kit_body_index(2952);if(body==Simulation::kKitNone)return 2;
 for(unsigned part:{2U,5U}){const auto a=box(s,2952,body,part);std::cout<<"INITIAL_BOX,part="<<part<<",centre=";vec(a.centre);std::cout<<",half="<<a.half[0]<<','<<a.half[1]<<','<<a.half[2]<<",hold="<<a.hold<<'\n';}
 unsigned clear_ticks=0,first_tick=0,fallback_clear_ticks=0;
 for(unsigned n=0;n<180;++n){tick(s);const auto v=s.snapshot();
 if(v.traversal_hand_constraint_count!=1||v.traversal_left_hand_generation!=entry.traversal_left_hand_generation||v.traversal_right_hand_generation!=0||!v.hanging_free_arc_active||v.player_grounded||v.death_count!=0||v.player_gravity_factor!=1||v.traversal_actuator_positive_work_j!=entry.traversal_actuator_positive_work_j||v.traversal_command_work_bound_j!=entry.traversal_command_work_bound_j){state("history_failed",v);std::cerr<<"FAIL retained neutral history\n";return 1;}
 std::vector<Box> boxes;for(std::size_t part=0;part<s.kit_body_part_count(body);++part)boxes.push_back(box(s,2952,body,part));
 const auto proxy=add(v.player_position,{0,.65,.2}),aim=add(proxy,{.4,0,0});unsigned eligible=0,visible=0;double best_distance=2;Vector3 best{};unsigned best_part=999;
 for(const auto &a:boxes)if(a.hold&&overlap(a,aim,{.35,.30,.30})){
  const double t=std::clamp(dot(sub(aim,a.centre),a.axis[a.along]),-a.half[a.along],a.half[a.along]);const auto point=add(a.centre,scale(a.axis[a.along],t));
  ++eligible;double fraction;const auto hit=first(boxes,proxy,point,fraction);const double radius=length(sub(point,v.player_position));
  const bool clear=point.x-v.player_position.x>=.175&&radius<=1.75&&hit&&hit->part==a.part;
  if(clear){++visible;const double d=length(sub(point,aim));if(d<best_distance){best_distance=d;best=point;best_part=a.part;}}
  std::cout<<"CANDIDATE,tick="<<v.tick_index<<",part="<<a.part<<",point=";vec(point);std::cout<<",radius="<<radius<<",first_part="<<(hit?hit->part:999)<<",first_fraction="<<fraction<<",clear="<<clear<<'\n';
 }
 double fallback_fraction;const auto fallback_hit=first(boxes,proxy,v.traversal_left_hand,fallback_fraction);
 const bool fallback_clear=fallback_hit&&fallback_hit->hold&&fallback_hit->part==5&&length(sub(v.traversal_left_hand,v.player_position))<=1.75;
 if(fallback_clear)++fallback_clear_ticks;
 if(visible){++clear_ticks;if(first_tick==0){first_tick=static_cast<unsigned>(v.tick_index);state("first_clear_candidate",v);}}
 const auto q=s.kit_body_rotation(body);std::cout<<"HISTORY,tick="<<v.tick_index<<",com=";vec(v.player_position);std::cout<<",velocity=";vec(v.player_linear_velocity);std::cout<<",retained_grip=";vec(v.traversal_left_hand);
 std::cout<<",generation="<<v.traversal_left_hand_generation<<",radius="<<v.hanging_actual_span_m<<",upper_proxy=";vec(proxy);std::cout<<",aim=";vec(aim);
 std::cout<<",eligible="<<eligible<<",visible="<<visible<<",best_part="<<best_part<<",best_point=";vec(best);
 std::cout<<",fallback_first_part="<<(fallback_hit?fallback_hit->part:999)<<",fallback_fraction="<<fallback_fraction<<",fallback_clear="<<fallback_clear<<",frame_origin=";vec(s.kit_body_position(body));std::cout<<",frame_quat="<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<'\n';
 }
 std::cout<<"RESULT fixed_neutral_ticks=180 clear_candidate_ticks="<<clear_ticks<<" first_clear_tick="<<first_tick<<" retained_anchor_visible_ticks="<<fallback_clear_ticks<<" actual_promotion_attempted=0 no_aftercatch_state_writes=1\n";
}
