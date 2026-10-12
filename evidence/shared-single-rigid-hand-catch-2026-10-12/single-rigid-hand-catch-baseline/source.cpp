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
 if(!s.debug_restart_at({16.85,11.50,-118.0})||!s.set_facing(1,0)||!s.set_move_input(0,0)||!s.set_crouch_input(false)){std::cerr<<"SETUP_FAIL clear initial stage\n";return 2;}tick(s);
 const auto before=s.snapshot();state("cue",before);std::vector<Box> boxes;
 for(unsigned entity:{2952U,1952U,2954U}){const auto index=s.kit_body_index(entity);if(index==Simulation::kKitNone){std::cerr<<"SETUP_FAIL known body\n";return 2;}
 std::cout<<"BODY,entity="<<entity<<",mass="<<s.kit_body_mass(index)<<",dynamic="<<s.kit_body_dynamic(index)<<",origin=";vec(s.kit_body_position(index));auto q=s.kit_body_rotation(index);std::cout<<",quat="<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<'\n';
 for(std::size_t i=0;i<s.kit_body_part_count(index);++i){auto a=box(s,entity,index,i);boxes.push_back(a);std::cout<<"BOX,entity="<<entity<<",part="<<i<<",centre=";vec(a.centre);std::cout<<",half="<<a.half[0]<<','<<a.half[1]<<','<<a.half[2]<<",hold="<<a.hold<<",axis="<<a.along<<",axis_vectors=";for(auto axis:a.axis){vec(axis);std::cout<<';';}std::cout<<'\n';}}
 if(!before.grip_available||before.grip_entity_id!=2952||before.player_grounded||before.death_count!=0){std::cerr<<"SETUP_FAIL actual airborne2952cue\n";return 2;}
 bool centre_ok=false,lower_ok=false,upper_blocked=false;unsigned upper_eligible=0;
 for(unsigned hand=0;hand<3;++hand){const double height=hand==2?.65:.45;const double across=hand==0?0:(hand==1?-.2:.2);
 const auto proxy=add(before.player_position,{0,height,across});const auto aim=add(proxy,{.4,0,0});double f=2;const auto hit=first(boxes,proxy,before.grip_point,f);
 std::cout<<"APPROACH,"<<hand<<",proxy=";vec(proxy);std::cout<<",aim=";vec(aim);std::cout<<",seed=";vec(before.grip_point);std::cout<<",first_entity="<<(hit?hit->entity:0)<<",first_part="<<(hit?hit->part:999)<<",first_fraction="<<f<<",first_is_hold="<<(hit&&hit->hold)<<'\n';
 if(hand==0)centre_ok=hit&&hit->entity==2952&&hit->hold;
 if(hand==1)lower_ok=hit&&hit->entity==2952&&hit->hold;
 if(hand==2)upper_blocked=hit&&hit->entity==2952&&!hit->hold;
 unsigned eligible=0;for(const auto &b:boxes)if(overlap(b,aim,{.35,.30,.30})){std::cout<<"QUERY_OVERLAP,"<<hand<<",entity="<<b.entity<<",part="<<b.part<<",hold="<<b.hold;
 if(b.hold){++eligible;const double t=std::clamp(dot(sub(aim,b.centre),b.axis[b.along]),-b.half[b.along],b.half[b.along]);const auto point=add(b.centre,scale(b.axis[b.along],t));double cf=2;const auto ch=first(boxes,proxy,point,cf);std::cout<<",candidate=";vec(point);std::cout<<",first_part="<<(ch?ch->part:999)<<",visible_own_leaf="<<(ch&&ch->entity==b.entity&&ch->part==b.part);}
 std::cout<<'\n';}std::cout<<"QUERY_TOTAL,"<<hand<<",known_local_eligible="<<eligible<<'\n';if(hand==2)upper_eligible=eligible;}
 if(!centre_ok||!lower_ok||!upper_blocked||upper_eligible!=0){std::cerr<<"SETUP_FAIL actual centre/lower visible upper blocked/no alternative witness\n";return 2;}
 if(!s.request_traversal()){std::cerr<<"SETUP_FAIL ordinaryAction\n";return 2;}tick(s);const auto after=s.snapshot();state("after_action",after);
 if(after.traversal_state!=TraversalState::None||after.traversal_hand_constraint_count!=0){std::cerr<<"SETUP_FAIL expected baseline pair clears\n";return 2;}
 std::cout<<"BASELINE_CONFIRMED safe_visible_lower=1 upper_post_blocked=1 known_local_upper_candidates=0 current_pair_clears=1 no_postcatch_state_writes=1\n";
}
