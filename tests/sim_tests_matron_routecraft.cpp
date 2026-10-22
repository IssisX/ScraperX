#include "sim_tests.hpp"
#include "sim/physics_world_support.hpp"
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <memory>
#include <chrono>
#include <ctime>
#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>

namespace scraperx::sim {
struct MatronRoutecraftTestAccess {
    struct Waypoint {
        JPH::RVec3 center;
        JPH::BodyID body;
        JPH::SubShapeID sub;
        JPH::Vec3 local;
        JPH::RVec3 point;
    };
    static std::vector<Waypoint> route(const MatronRoutecraft &planner) {
        std::vector<Waypoint> result;
        for (const auto &node : planner.route_)
            result.push_back({node.center,node.grip.body,node.grip.sub,node.grip.local,node.grip.point});
        return result;
    }
    static void queries(MatronRoutecraft &planner, unsigned count) { planner.report_.queries=count; }
};
}

namespace {
using namespace JPH;
using namespace scraperx::sim;
using namespace scraperx::sim::detail;
class MeasuredSolidFilter final : public BodyFilter {
public:
    explicit MeasuredSolidFilter(BodyID self) : self_(self) {}
    bool ShouldCollide(const BodyID &id) const override { return id != self_; }
    bool ShouldCollideLocked(const Body &body) const override { return !body.IsSensor(); }
private:
    BodyID self_;
};
struct Fixture {
    JoltRuntimeLease lease;
    detail::BroadPhaseLayerInterface broad;
    ObjectVsBroadPhaseFilter vs;
    detail::ObjectLayerPairFilter pair;
    TempAllocatorImpl temp{32 * 1024 * 1024};
    JobSystemSingleThreaded jobs{1024};
    PhysicsSystem physics;
    std::unique_ptr<kit::Kit> kit;
    std::unique_ptr<Matron> matron;
    std::vector<BodyID> solids;
    float min_y=10000, min_x=10000, max_x=-10000, penetration=0, controlled_penetration=0, measured_force=0, measured_power=0;
    double max_request_ms=0, max_planning_ms=0;
    double max_request_cpu_ms=0, max_planning_cpu_ms=0;
    unsigned planning_ticks=0, max_slice_expansions=0, max_slice_queries=0, max_slice_neighbor_checks=0;
    unsigned publications=0, max_published_after_ticks=0;
    Fixture(RVec3 start) {
        physics.Init(8192, 0, 16384, 8192, broad, vs, pair);
        physics.SetGravity(Vec3(0,-9.81F,0));
        matron=std::make_unique<Matron>(physics,object_layers::kStatic,object_layers::kMoving,object_layers::kOnSteel,BodyID());
        Matron::Saved saved; saved.state=Matron::State::Hunting;
        matron->restore(saved);
        physics.GetBodyInterface().SetPosition(matron->body(),start,EActivation::Activate); // fixture initial condition only
    }
    ~Fixture() {
        matron.reset(); kit.reset();
        for (auto id:solids) { if (physics.GetBodyInterface().IsAdded(id)) physics.GetBodyInterface().RemoveBody(id); physics.GetBodyInterface().DestroyBody(id); }
    }
    BodyID box(RVec3 at, Vec3 half) {
        BodyCreationSettings settings(new BoxShape(half),at,Quat::sIdentity(),EMotionType::Static,object_layers::kStatic);
        const BodyID id=physics.GetBodyInterface().CreateAndAddBody(settings,EActivation::DontActivate);
        solids.push_back(id); return id;
    }
    void ladder(float x,float bottom,float top,float z=0) {
        for(float y=bottom+0.3F;y<=top;y+=0.3F) box(RVec3(x,y,z),Vec3(0.28F,0.02F,0.02F));
    }
    RVec3 at() { return physics.GetBodyInterface().GetPosition(matron->body()); }
    void tick(MatronRoutecraft *planner=nullptr,
        unsigned expansion_budget=MatronRoutecraft::kPlanningExpansionsPerTick,
        unsigned transition_budget=MatronRoutecraft::kPlanningTransitionsPerTick,
        unsigned query_budget=MatronRoutecraft::kPlanningQueriesPerTick) {
        constexpr float dt=1.0F/90;
        const RVec3 before = at();
        const Vec3 velocity_before=physics.GetBodyInterface().GetLinearVelocity(matron->body());
        const auto &r=planner ? planner->report() : matron->climb_report();
        const auto old_plans=r.plans;
        const auto old_queries=r.native_queries;
        const auto old_published_after_ticks=r.published_after_ticks;
        const bool was_planning=r.phase!=MatronRoutecraft::PlanPhase::None;
        // CPU-clock calls can stall on the host; keep them outside wall timing.
        const auto cpu_started=std::clock();
        const auto started=std::chrono::steady_clock::now();
        if(planner) planner->step(physics,matron->body(),dt,expansion_budget,transition_budget,query_budget);
        else matron->pre_step(dt,RVec3(300,154,300),0);
        const double step_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
        const double step_cpu_ms=1000.0*(std::clock()-cpu_started)/CLOCKS_PER_SEC;
        if(was_planning || r.phase!=MatronRoutecraft::PlanPhase::None || r.plans>old_plans) {
            max_planning_ms=std::max(max_planning_ms,step_ms); ++planning_ticks;
            max_planning_cpu_ms=std::max(max_planning_cpu_ms,step_cpu_ms);
            require(at()==before && physics.GetBodyInterface().GetLinearVelocity(matron->body())==velocity_before,
                "routecraft: planning never writes body position or velocity");
        }
        max_slice_expansions=std::max(max_slice_expansions,r.slice_expansions);
        max_slice_queries=std::max(max_slice_queries,r.slice_queries);
        max_slice_neighbor_checks=std::max(max_slice_neighbor_checks,r.slice_neighbor_checks);
        require(r.slice_expansions<=expansion_budget,"routecraft: per-tick expansion cap");
        require(r.slice_queries<=query_budget,"routecraft: per-tick planning query-work cap");
        require(r.slice_neighbor_checks<=MatronRoutecraft::kPlanningNeighborChecksPerTick,
            "routecraft: per-tick neighbor check cap");
        if(old_published_after_ticks==0 && r.published_after_ticks>0) {
            require(r.published_after_ticks<=r.planning_ticks,"routecraft: publication wait is counted within request planning ticks");
            ++publications;
            max_published_after_ticks=std::max(max_published_after_ticks,r.published_after_ticks);
            std::cout<<"routecraft publication published_after_ticks="<<r.published_after_ticks
                <<" wait_s="<<r.published_after_ticks/90.0<<" planning_ticks="<<r.planning_ticks
                <<" plans="<<r.plans<<" queries="<<r.queries<<" native_queries="<<r.native_queries<<'\n';
        }
        if(r.plans>old_plans) std::cout<<"routecraft fault replan pre_step_ms="<<step_ms
            <<" native_queries="<<r.native_queries-old_queries<<'\n';
        {
            BodyLockRead body(physics.GetBodyLockInterface(),matron->body());
            const Vec3 force=body.GetBody().GetAccumulatedForce();
            require(std::abs(force.Length()-r.force)<0.01F,"routecraft: reported force matches actual body accumulator");
            measured_force=std::max(measured_force,force.Length());
            measured_power=std::max(measured_power,std::abs(force.Dot(body.GetBody().GetLinearVelocity())));
            require(force.Length()<=MatronRoutecraft::kForce+0.1F,"routecraft: actual accumulated force cap");
            require(measured_power<=MatronRoutecraft::kPower+0.1F,"routecraft: actual mechanical power cap");
            if(r.force>0) require((force+r.reaction).Length()<0.01F,"routecraft: equal static support reaction");
        }
        physics.Update(dt,1,&temp,&jobs);
        if(!planner) matron->post_step(dt);
        require(Vec3(at()-before).Length()<0.2F, "routecraft: measured physical movement below 0.2 m per tick");
        require(physics.GetBodyInterface().GetGravityFactor(matron->body())==1,"routecraft: gravity stays enabled");
        min_y=std::min(min_y,float(at().GetY())); min_x=std::min(min_x,float(at().GetX())); max_x=std::max(max_x,float(at().GetX()));
        CapsuleShape capsule(Matron::kHalfHeight,Matron::kRadius);
        AllHitCollisionCollector<CollideShapeCollector> hits;
        physics.GetNarrowPhaseQuery().CollideShape(&capsule,Vec3::sReplicate(1),RMat44::sTranslation(at()),
            CollideShapeSettings(),at(),hits,
            physics.GetDefaultBroadPhaseLayerFilter(physics.GetBodyInterface().GetObjectLayer(matron->body())),
            physics.GetDefaultLayerFilter(physics.GetBodyInterface().GetObjectLayer(matron->body())),
            MeasuredSolidFilter(matron->body()));
        const auto status=r.status;
        for(const auto &hit:hits.mHits) {
            penetration=std::max(penetration,hit.mPenetrationDepth);
            // An unsupported pending search follows the ordinary falling-body
            // tolerance; a retained grip keeps the stricter controlled bound.
            if((status==MatronRoutecraft::Status::Moving && r.phase==MatronRoutecraft::PlanPhase::None) ||
                status==MatronRoutecraft::Status::Holding || r.force>0)
                controlled_penetration=std::max(controlled_penetration,hit.mPenetrationDepth);
        }
        require(controlled_penetration<=0.025F,"routecraft: controlled capsule penetration under 25 mm");
        // Released falls in these fixtures reach under 10 m/s. Jolt may
        // retain one 90 Hz travel step plus contact slop before correction;
        // the recovery checks below also require actual settled floor height.
        require(penetration<=10.0F/90.0F+0.015F,"routecraft: recovery overlap bounded by one impact step");
        require(r.force<=MatronRoutecraft::kForce+0.01F,"routecraft: finite force");
        require(r.peak_power<=MatronRoutecraft::kPower+0.1F,"routecraft: finite power");
        require(r.max_reach<=MatronRoutecraft::kReach+0.001F,"routecraft: contact reach");
    }
    void finish_plan(int ticks=1800) {
        for(int i=0;i<ticks && matron->climb_report().phase!=MatronRoutecraft::PlanPhase::None;++i) tick();
        require(matron->climb_report().phase==MatronRoutecraft::PlanPhase::None,"routecraft: pending plan finishes");
    }
    void timings(const char *name) const {
        std::cout<<"PASS: routecraft slicing timing fixture="<<name<<" max_request_ms="<<max_request_ms
            <<" max_planning_pre_step_ms="<<max_planning_ms<<" planning_ticks="<<planning_ticks
            <<" max_slice_expansions="<<max_slice_expansions<<" max_slice_queries="<<max_slice_queries
            <<" max_slice_neighbor_checks="<<max_slice_neighbor_checks<<" publications="<<publications
            <<" max_published_after_ticks="<<max_published_after_ticks<<" max_wait_s="<<max_published_after_ticks/90.0
            <<" max_request_cpu_ms="<<max_request_cpu_ms
            <<" max_planning_pre_step_cpu_ms="<<max_planning_cpu_ms<<'\n';
    }
    bool go(RVec3 goal,int ticks=9000) {
        const auto cpu_started=std::clock();
        const auto started=std::chrono::steady_clock::now();
        const bool planned=matron->request_climb_goal(goal,Vec3::sAxisZ());
        const double request_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
        const double request_cpu_ms=1000.0*(std::clock()-cpu_started)/CLOCKS_PER_SEC;
        max_request_ms=std::max(max_request_ms,request_ms);
        max_request_cpu_ms=std::max(max_request_cpu_ms,request_cpu_ms);
        std::cout<<"routecraft request goal="<<goal<<" accepted="<<planned
            <<" request_ms="<<request_ms<<" request_cpu_ms="<<request_cpu_ms<<'\n';
        if (!planned) { std::cout<<"routecraft plan rejected status="<<int(matron->climb_report().status)<<" queries="<<matron->climb_report().queries<<'\n'; return false; }
        for(int i=0;i<ticks;++i) {
            tick();
            if(matron->climb_report().status==MatronRoutecraft::Status::Holding) return Vec3(at()-goal).Length()<0.1F;
            if(matron->climb_report().status!=MatronRoutecraft::Status::Moving) break;
        }
        const auto p=at(); std::cout<<"routecraft stopped at "<<p.GetX()<<','<<p.GetY()<<','<<p.GetZ()<<" status="<<int(matron->climb_report().status)<<" plans="<<matron->climb_report().plans<<'\n';
        return false;
    }
};

struct PlanTrace {
    std::vector<MatronRoutecraftTestAccess::Waypoint> route;
    MatronRoutecraft::Report report;
    unsigned slices=0, search_slices=0, max_slice_queries=0, max_slice_neighbor_checks=0;
    double request_ms=0, max_slice_ms=0;
    double request_cpu_ms=0, max_search_cpu_ms=0, max_validate_cpu_ms=0;
};
template<class Build>
PlanTrace trace_plan(RVec3 start, RVec3 goal, const Build &build, unsigned expansions, unsigned transitions, unsigned queries) {
    Fixture f(start); build(f);
    MatronRoutecraft planner;
    PlanTrace trace;
    constexpr unsigned unlimited=std::numeric_limits<unsigned>::max();
    const unsigned neighbor_budget=expansions==unlimited && transitions==unlimited && queries==unlimited
        ? unlimited : MatronRoutecraft::kPlanningNeighborChecksPerTick;
    const auto requested_cpu=std::clock();
    const auto requested=std::chrono::steady_clock::now();
    require(planner.request(f.physics,f.matron->body(),goal,Vec3::sAxisZ()),"routecraft: deterministic search accepted");
    trace.request_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-requested).count();
    trace.request_cpu_ms=1000.0*(std::clock()-requested_cpu)/CLOCKS_PER_SEC;
    require(planner.planning() && planner.report().expansions==0,"routecraft: request defers search expansions");
    // No physics updates: all budgets see exactly the same collision world,
    // source pose and velocity. Physical waiting is exercised separately below.
    while(planner.planning() && trace.slices<10000) {
        const auto phase=planner.report().phase;
        const auto before_cpu=std::clock();
        const auto before=std::chrono::steady_clock::now();
        planner.step(f.physics,f.matron->body(),1.0F/90,expansions,transitions,queries);
        trace.max_slice_ms=std::max(trace.max_slice_ms,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count());
        const double cpu_ms=1000.0*(std::clock()-before_cpu)/CLOCKS_PER_SEC;
        if(phase==MatronRoutecraft::PlanPhase::Search) {
            ++trace.search_slices;
            trace.max_search_cpu_ms=std::max(trace.max_search_cpu_ms,cpu_ms);
        }
        else trace.max_validate_cpu_ms=std::max(trace.max_validate_cpu_ms,cpu_ms);
        require(planner.report().slice_expansions<=expansions,"routecraft: explicit search expansion cap");
        require(planner.report().slice_queries<=queries,"routecraft: explicit planning query-work cap");
        require(planner.report().slice_neighbor_checks<=neighbor_budget,
            "routecraft: explicit neighbor check cap");
        trace.max_slice_queries=std::max(trace.max_slice_queries,planner.report().slice_queries);
        trace.max_slice_neighbor_checks=std::max(trace.max_slice_neighbor_checks,planner.report().slice_neighbor_checks);
        if(planner.planning()) require(planner.report().force==0,"routecraft: unvalidated route applies no load");
        ++trace.slices;
    }
    if(planner.planning() || planner.report().status!=MatronRoutecraft::Status::Moving)
        std::cerr<<"routecraft trace goal="<<goal<<" budget="<<expansions<<','<<transitions<<','<<queries
            <<" status="<<int(planner.report().status)<<" phase="<<int(planner.report().phase)
            <<" ticks="<<trace.slices<<" queries="<<planner.report().queries<<'\n';
    require(!planner.planning() && planner.report().status==MatronRoutecraft::Status::Moving,
        "routecraft: deterministic search and validation finish");
    trace.route=MatronRoutecraftTestAccess::route(planner); trace.report=planner.report();
    require(trace.report.planning_ticks==trace.slices && trace.report.published_after_ticks==trace.slices,
        "routecraft: publication wait reports every pending tick through the first usable route");
    return trace;
}
template<class Build>
void compare_slices(const char *name, RVec3 start, RVec3 goal, const Build &build) {
    constexpr unsigned unlimited=std::numeric_limits<unsigned>::max();
    const auto full=trace_plan(start,goal,build,unlimited,unlimited,unlimited);
    require(full.search_slices==1,"routecraft: unlimited reference completes A* in one tick");
    const auto sliced=trace_plan(start,goal,build,1,3,unlimited);
    const auto query_sliced=trace_plan(start,goal,build,unlimited,unlimited,32);
    for(const auto *trace : {&sliced,&query_sliced}) {
        require(!full.route.empty() && full.route.size()==trace->route.size(),"routecraft: slice-independent route length");
        for(std::size_t i=0;i<full.route.size();++i) {
            const auto &a=full.route[i], &b=trace->route[i];
            require(a.center==b.center && a.body==b.body && a.sub.GetValue()==b.sub.GetValue() &&
                a.local==b.local && a.point==b.point,"routecraft: exactly identical complete route across budgets");
        }
        require(full.report.expansions==trace->report.expansions && full.report.queries==trace->report.queries,
            "routecraft: slice-independent total expansions and query budget");
        require(full.report.native_queries==trace->report.native_queries,
            "routecraft: resuming query work never repeats native queries in an unchanged world");
        require(trace->slices>full.slices,"routecraft: small budget genuinely resumes across ticks");
    }
    std::cout<<"PASS: routecraft slicing exact fixture="<<name<<" nodes="<<full.route.size()
        <<" expansions="<<sliced.report.expansions<<" queries="<<sliced.report.queries<<" ticks="<<sliced.slices
        <<" request_ms="<<sliced.request_ms<<" max_slice_ms="<<sliced.max_slice_ms
        <<" unlimited_max_slice_ms="<<full.max_slice_ms<<" request_cpu_ms="<<sliced.request_cpu_ms
        <<" max_search_cpu_ms="<<sliced.max_search_cpu_ms<<" max_validate_cpu_ms="<<sliced.max_validate_cpu_ms
        <<" unlimited_max_search_cpu_ms="<<full.max_search_cpu_ms
        <<" query32_ticks="<<query_sliced.slices<<" query32_published_after_ticks="<<query_sliced.report.published_after_ticks
        <<" query32_max_slice_queries="<<query_sliced.max_slice_queries
        <<" query32_max_neighbor_checks="<<query_sliced.max_slice_neighbor_checks
        <<" query32_max_slice_ms="<<query_sliced.max_slice_ms<<'\n';
}
void reach_validation(Fixture &f, MatronRoutecraft &planner) {
    for(int i=0;i<10000 && planner.report().phase==MatronRoutecraft::PlanPhase::Search;++i)
        planner.step(f.physics,f.matron->body(),1.0F/90,1,4,std::numeric_limits<unsigned>::max());
    require(planner.report().phase==MatronRoutecraft::PlanPhase::Validate && planner.report().force==0,
        "routecraft: complete route awaits validation before load");
}
void check_sliced_planning() {
    compare_slices("symmetric_ties",RVec3(0,155.07,-0.55),RVec3(0,160,-0.55),[](Fixture &f) {
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2));
        f.ladder(-0.55F,154,161); f.ladder(0.55F,154,161);
    });
    compare_slices("backtracking",RVec3(0,158,-0.55),RVec3(1.2F,161,-0.55),[](Fixture &f) {
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2));
        f.ladder(0,154,158); f.ladder(1.2F,154,162);
        f.box(RVec3(0.6F,160.75,-0.4),Vec3(0.1F,3.25F,1.5F));
    });
    compare_slices("dense_detour",RVec3(20,155.07,11.45),RVec3(20,175,11.45),[](Fixture &f) {
        f.box(RVec3(20,153.9,11),Vec3(4,0.1F,3));
        for(int i=0;i<50;++i) f.box(RVec3(20,154.3+i*0.44,12),Vec3(1.03F,0.02F,0.02F));
        for(int i=0;i<80;++i) f.box(RVec3(21.7,154.3+i*0.27,12.7),Vec3(0.28F,0.02F,0.02F));
        f.box(RVec3(20,160,11.45),Vec3(0.4F,0.2F,0.4F));
    });
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2)); f.ladder(0,154,161);
        require(f.matron->request_climb_goal(RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: pending replacement setup accepted");
        const auto pending=f.matron->climb_report();
        require(!f.matron->request_climb_goal(RVec3(0,1000,-0.55),Vec3::sAxisZ()) &&
            f.matron->climb_report().phase==pending.phase && f.matron->climb_report().status==pending.status &&
            f.matron->climb_report().queries==pending.queries,"routecraft: rejected replacement preserves pending search");
        f.matron->cancel_climb_goal();
    }
    for(bool after_search : {false,true}) {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2)); f.ladder(0,154,161);
        MatronRoutecraft planner;
        require(planner.request(f.physics,f.matron->body(),RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: cancel setup accepted");
        if(after_search) reach_validation(f,planner);
        const auto phase=planner.report().phase;
        planner.step(f.physics,f.matron->body(),1.0F/90,1,1,1);
        require(planner.report().phase==phase && planner.report().slice_queries==1,
            "routecraft: cancellation interrupts query-limited search or validation work");
        planner.cancel();
        const auto queries=planner.report().queries, expansions=planner.report().expansions;
        const auto native_queries=planner.report().native_queries, planning_ticks=planner.report().planning_ticks;
        for(int i=0;i<6;++i) f.tick(&planner);
        require(!planner.planning() && planner.report().status==MatronRoutecraft::Status::Released &&
            planner.report().force==0 && MatronRoutecraftTestAccess::route(planner).empty() &&
            planner.report().queries==queries && planner.report().expansions==expansions &&
            planner.report().native_queries==native_queries && planner.report().planning_ticks==planning_ticks &&
            planner.report().published_after_ticks==0 && planner.report().slice_queries==0 &&
            planner.report().slice_neighbor_checks==0,
            "routecraft: cancelled search or validation never resumes");
    }
    for(bool after_validation : {false,true}) for(bool obstruct_path : {false,true}) {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2)); f.ladder(0,154,161);
        MatronRoutecraft planner;
        require(planner.request(f.physics,f.matron->body(),RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: stale route setup accepted");
        reach_validation(f,planner);
        const auto route=MatronRoutecraftTestAccess::route(planner);
        require(route.size()>2 && route.back().body!=route.front().body,"routecraft: invalidation targets a future contact");
        if(after_validation) {
            const auto plans=planner.report().plans;
            // Every sampled edge has passed, one per tick. Publication is a
            // separate tick and must see changes to an already checked edge.
            for(std::size_t i=0;i<route.size();++i)
                planner.step(f.physics,f.matron->body(),1.0F/90,1,4,std::numeric_limits<unsigned>::max());
            require(planner.report().phase==MatronRoutecraft::PlanPhase::Validate &&
                planner.report().plans==plans && planner.report().force==0,
                "routecraft: sampled validation finishes before publication");
        }
        if(obstruct_path) f.box(route.back().center,Vec3(0.3F,0.1F,0.3F));
        else f.physics.GetBodyInterface().RemoveBody(route.back().body);
        const auto plans=planner.report().plans, queries=planner.report().queries;
        planner.step(f.physics,f.matron->body(),1.0F/90,std::numeric_limits<unsigned>::max(),
            std::numeric_limits<unsigned>::max(),std::numeric_limits<unsigned>::max());
        require(planner.report().plans>plans && planner.report().force==0 &&
            (planner.planning() || planner.report().status!=MatronRoutecraft::Status::Moving),
            "routecraft: stale future grip or clearance restarts before execution");
        require(planner.report().queries>queries,"routecraft: stale-route replan preserves the cumulative query count");
        BodyLockRead body(f.physics.GetBodyLockInterface(),f.matron->body());
        require(body.GetBody().GetAccumulatedForce().Length()==0,"routecraft: invalid completed path adds no actual force");
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2)); f.ladder(0,154,161);
        MatronRoutecraft planner;
        require(planner.request(f.physics,f.matron->body(),RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: query cap setup accepted");
        planner.step(f.physics,f.matron->body(),1.0F/90,1,1,1);
        MatronRoutecraftTestAccess::queries(planner,199900);
        unsigned slices=0;
        for(;slices<200 && planner.planning();++slices) {
            const auto queries=planner.report().queries;
            planner.step(f.physics,f.matron->body(),1.0F/90,1,1,1);
            require(planner.report().queries>=queries,"routecraft: resuming never resets cumulative query budget");
            require(planner.report().slice_queries<=1 &&
                planner.report().slice_neighbor_checks<=MatronRoutecraft::kPlanningNeighborChecksPerTick,
                "routecraft: cumulative exhaustion respects each work slice");
        }
        require(slices>1 && !planner.planning() && planner.report().status==MatronRoutecraft::Status::BudgetExceeded &&
            planner.report().queries>200000,"routecraft: 200000-query limit spans the whole search");
        const auto exhausted=planner.report();
        planner.step(f.physics,f.matron->body(),1.0F/90);
        require(planner.report().queries==exhausted.queries && planner.report().native_queries==exhausted.native_queries &&
            planner.report().expansions==exhausted.expansions && planner.report().force==0,
            "routecraft: exhausted partial work cannot issue further queries or apply load");
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2)); f.ladder(0,154,161);
        MatronRoutecraft planner;
        require(planner.request(f.physics,f.matron->body(),RVec3(0,157.2,-0.55),Vec3::sAxisZ()),"routecraft: retained hold setup accepted");
        for(int i=0;i<1800 && planner.report().status==MatronRoutecraft::Status::Moving;++i) f.tick(&planner);
        require(planner.report().status==MatronRoutecraft::Status::Holding,"routecraft: retained hold physically acquired");
        const RVec3 held_at=f.at(), hand=planner.report().hand;
        const BodyID support=MatronRoutecraftTestAccess::route(planner).back().body;
        require(planner.request(f.physics,f.matron->body(),RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: replacement search accepted while holding");
        const auto queries=planner.report().queries, native_queries=planner.report().native_queries;
        for(int i=0;i<3;++i) {
            f.tick(&planner,MatronRoutecraft::kPlanningExpansionsPerTick,MatronRoutecraft::kPlanningTransitionsPerTick,0);
            require(planner.planning() && planner.report().force>0 && planner.report().hand==hand &&
                Vec3(f.at()-held_at).Length()<0.1F,"routecraft: waiting retains previous physical grip and pose");
        }
        require(planner.report().queries==queries && planner.report().native_queries>native_queries,
            "routecraft: live hold checks do not spend the search query budget");
        f.tick(&planner,1,1);
        f.physics.GetBodyInterface().RemoveBody(support);
        f.tick(&planner,0,0);
        require(planner.report().force==0,"routecraft: removed waiting support supplies no first-tick force");
        planner.cancel();
        f.tick(&planner);
        require(!planner.planning() && planner.report().status==MatronRoutecraft::Status::Released,
            "routecraft: pending held replacement cancels cleanly");
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2)); f.ladder(0,154,161);
        MatronRoutecraft planner;
        for(int i=0;i<20;++i) f.tick(&planner); // settle onto real floor contact
        f.physics.GetBodyInterface().AddImpulse(f.matron->body(),Vec3(100,0,0));
        const RVec3 start=f.at();
        require(planner.request(f.physics,f.matron->body(),RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: standing wait accepted");
        const auto queries=planner.report().queries;
        bool foot_force=false;
        for(int i=0;i<180;++i) {
            f.tick(&planner,0,0);
            foot_force=foot_force || planner.report().force>0;
            require(!planner.owns_motion() && planner.report().acquisitions==0,"routecraft: standing wait fabricates no hand grip");
        }
        const float waiting_vx=f.physics.GetBodyInterface().GetLinearVelocity(f.matron->body()).GetX();
        const float waiting_drift=Vec3(f.at()-start).Length();
        if(!foot_force || std::abs(waiting_vx)>=0.002F || waiting_drift>=0.08F)
            std::cerr<<"standing wait force_seen="<<foot_force<<" vx="<<waiting_vx<<" drift="<<waiting_drift
                <<" at="<<f.at()<<" penetration="<<f.penetration<<'\n';
        require(foot_force && std::abs(waiting_vx)<0.002F && waiting_drift<0.08F,
            "routecraft: finite foot traction stops coasting while planning");
        require(planner.planning() && planner.report().queries==queries,"routecraft: standing wait leaves search budget intact");
    }
    {
        Fixture f(RVec3(0,157,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2)); f.ladder(0,154,161);
        MatronRoutecraft planner;
        f.physics.GetBodyInterface().AddImpulse(f.matron->body(),Vec3(100,0,0));
        const Vec3 velocity=f.physics.GetBodyInterface().GetLinearVelocity(f.matron->body());
        require(planner.request(f.physics,f.matron->body(),RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: airborne wait accepted");
        for(int i=0;i<5;++i) {
            f.tick(&planner,0,0);
            require(planner.report().force==0 && !planner.owns_motion(),"routecraft: waiting airborne supplies no invented support");
        }
        const Vec3 after=f.physics.GetBodyInterface().GetLinearVelocity(f.matron->body());
        require(std::abs(after.GetX()-velocity.GetX())<0.001F && after.GetY()<velocity.GetY(),
            "routecraft: airborne planning preserves horizontal momentum and falls under gravity");
    }
    std::cout<<"PASS: routecraft slicing cancellation validation cumulative_budget held_wait grounded_wait real_fall\n";
}
}
void run_matron_routecraft() {
    using namespace JPH;
    using namespace scraperx::sim;
    check_sliced_planning();
    RVec3 replay=RVec3::sZero();
    for(int run=0;run<2;++run) {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2));
        f.ladder(0,154,160);
        require(f.go(RVec3(0,159,-0.55)),"routecraft: fixture ascent executes");
        if(run==0) replay=f.at(); else require(f.at()==replay,"routecraft: identical replay");
        const auto up=f.matron->climb_report();
        require(f.go(RVec3(0,155.2,-0.55)),"routecraft: fixture descent executes");
        std::cout<<"routecraft replay="<<run<<" ascent_y="<<replay.GetY()<<" descent_y="<<f.at().GetY()<<" acquisitions="<<up.acquisitions<<" peak_force="<<up.peak_force<<" peak_power="<<up.peak_power<<'\n';
        f.matron->cancel_climb_goal();
        require(f.matron->climb_report().force==0,"routecraft: exit clears load");
    }
    {
        Fixture f(RVec3(5,155.07,-129.6));
        f.box(RVec3(5,153.9,-130),Vec3(3,0.1F,2));
        f.box(RVec3(5,176.15,-127.5),Vec3(3,0.1F,1.4F));
        f.kit=std::make_unique<kit::Kit>(f.physics,object_layers::kStatic,object_layers::kMoving);
        bands::CounterweightWell well;
        bands::build_counterweight_well(*f.kit,well); // production collision geometry, including its compound ladder
        require(f.go(RVec3(5,177.37,-128.6)),"routecraft: authored ladder ascent and mantle");
        std::cout<<"routecraft authored ascent/mantle y="<<f.at().GetY()<<" grips="<<f.matron->climb_report().acquisitions<<'\n';
        require(f.go(RVec3(5,155.2,-129.6)),"routecraft: authored ladder descent");
        std::cout<<"routecraft authored descent y="<<f.at().GetY()<<'\n';
    }
    {
        Simulation world(InitialSpawn::Deck154);
        world.launch_matron_for_test({5,155.07,-129.6});
        Vector3 previous{5,155.07,-129.6};
        float maximum_step=0;
        const auto tick_world=[&]() {
            (void)world.advance_frame(Simulation::kFixedStepSeconds);
            const auto m=world.matron_state();
            const double dx=m.position.x-previous.x, dy=m.position.y-previous.y, dz=m.position.z-previous.z;
            maximum_step=std::max(maximum_step,float(std::sqrt(dx*dx+dy*dy+dz*dz)));
            require(m.alive && maximum_step<0.2F,"routecraft: full tower alive with motion under 0.2 m per tick");
            previous=m.position;
        };
        auto started=std::chrono::steady_clock::now();
        const bool planned=world.request_matron_climb_goal({5,177.37,-128.6},{0,0,1});
        std::cout<<"full tower initial plan_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()
            <<" native_queries="<<world.matron_climb_queries()<<" status="<<world.matron_climb_status()<<std::endl;
        require(planned,"routecraft: full tower plans ascent");
        unsigned replans=0;
        for(int tick=0;tick<9000 && world.matron_climb_status()==1;++tick) {
            if(replans<2 && world.matron_state().position.y>155.07+(177.37-155.07)*(replans+1)/3) {
                require(world.matron_state().climbing && world.matron_state().climb_force_n>0,
                    "routecraft: full tower replan starts with a real loaded grip");
                started=std::chrono::steady_clock::now();
                require(world.request_matron_climb_goal({5,177.37,-128.6},{0,0,1}),"routecraft: full tower mid-climb replan request");
                std::cout<<"full tower replan_request_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()
                    <<" native_queries="<<world.matron_climb_queries()<<" at_y="<<world.matron_state().position.y<<'\n';
                ++replans;
            }
            tick_world();
        }
        require(replans==2,"routecraft: full tower performs both physical mid-climb replans");
        std::cout<<"routecraft full tower ascent y="<<world.matron_state().position.y<<" status="<<world.matron_climb_status()<<'\n';
        require(world.matron_climb_status()==2,"routecraft: full tower ascent executes");
        // One additional held tick lets native floor sensing establish the ring.
        tick_world();
        require(std::abs(world.matron_state().floor_y-176.25)<0.1,"routecraft: upper level established");
        require(!world.request_matron_climb_goal({5,1000,-128.6},{0,0,1}) && world.matron_climb_status()==2,"routecraft: rejected replacement keeps current hold");
        world.cancel_matron_climb_goal();
        for(int i=0;i<30;++i) tick_world();
        require(std::abs(world.matron_state().position.y-(176.25+Matron::kStandCentre))<0.025,"routecraft: top transfer released onto actual ring");
        for(int i=0;i<90;++i) {
            tick_world(); const auto m=world.matron_state();
            require(std::abs(m.position.y-(176.25+Matron::kStandCentre))<0.025 && !m.climbing && !m.grip_on && m.carrying==0,
                "routecraft: one second standing on upper support holding nothing");
        }
        std::cout<<"routecraft full tower grounded top-out y="<<world.matron_state().position.y<<" standing_ticks=90"<<'\n';
        require(world.request_matron_climb_goal({5,155.2,-129.6},{0,0,1}),"routecraft: full tower plans descent");
        int descent_ticks=0;
        for(;descent_ticks<9000 && world.matron_climb_status()==1;++descent_ticks) tick_world();
        if(world.matron_climb_status()!=2) {
            const auto m=world.matron_state();
            std::cerr<<"full tower descent stopped status="<<world.matron_climb_status()<<" ticks="<<descent_ticks
                <<" native_queries="<<world.matron_climb_queries()<<" at="<<m.position.x<<','<<m.position.y<<','<<m.position.z
                <<" state="<<int(m.state)<<" floor="<<m.floor_y<<'\n';
        }
        require(world.matron_climb_status()==2,"routecraft: full tower descent executes");
        std::cout<<"routecraft full tower descent y="<<world.matron_state().position.y<<'\n';
        world.cancel_matron_climb_goal();
        for(int i=0;i<30;++i) tick_world();
        require(world.matron_state().alive && std::abs(world.matron_state().floor_y-154)<0.1,"routecraft: real upper-level descent recovers on lower deck");
        for(int i=0;i<90;++i) {
            tick_world(); const auto m=world.matron_state();
            require(std::abs(m.position.y-(154+Matron::kStandCentre))<0.025 && !m.climbing && !m.grip_on && m.carrying==0,
                "routecraft: one second standing on lower support holding nothing");
        }
        std::cout<<"routecraft full tower lower standing_ticks=90 maximum_step_m="<<maximum_step<<'\n';
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2));
        f.ladder(0,154,157); f.ladder(1.1F,156,162);
        require(f.go(RVec3(1.1F,161,-0.55)),"routecraft: shifted ladder lateral transfer");
        std::cout<<"routecraft sideways x="<<f.at().GetX()<<" y="<<f.at().GetY()<<" penetration="<<f.penetration<<'\n';
    }
    {
        Fixture f(RVec3(0,158,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2));
        f.ladder(0,154,158); f.ladder(1.2F,154,162);
        f.box(RVec3(0.6F,160.75,-0.4),Vec3(0.1F,3.25F,1.5F));
        require(f.go(RVec3(0,158,-0.55)),"routecraft: backtracking starts from a physical hold");
        f.min_y=float(f.at().GetY());
        require(f.go(RVec3(1.2F,161,-0.55)),"routecraft: descent before ascent around obstruction");
        require(f.min_y<156.5F,"routecraft: observed physical backtracking");
        std::cout<<"routecraft backtrack min_y="<<f.min_y<<" final_y="<<f.at().GetY()<<'\n';
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2));
        f.ladder(0,154,161);
        f.box(RVec3(0,158,-0.55),Vec3(3,0.1F,2));
        require(f.matron->request_climb_goal(RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: blocked body path starts bounded search");
        f.finish_plan();
        require(f.matron->climb_report().status==MatronRoutecraft::Status::NoRoute,"routecraft: explicit no route");
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2));
        f.ladder(0,154,161);
        require(f.matron->request_climb_goal(RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: removal start");
        for(int i=0;i<90;++i) f.tick();
        require(f.matron->climb_report().phase==MatronRoutecraft::PlanPhase::None &&
            f.matron->climb_report().force>0,"routecraft: loaded support before removal");
        for(std::size_t i=1;i<f.solids.size();++i) f.physics.GetBodyInterface().RemoveBody(f.solids[i]);
        f.tick();
        require(f.matron->climb_report().force==0,"routecraft: removed supports carry no load");
        f.finish_plan();
        require(f.matron->climb_report().status==MatronRoutecraft::Status::NoRoute,"routecraft: missing holds end route");
        f.matron->cancel_climb_goal();
        for(int i=0;i<180;++i) f.tick();
        require(std::abs(f.at().GetY()-155.05)<0.04,"routecraft: released body physically lands");
        std::cout<<"routecraft removal/recovery y="<<f.at().GetY()<<'\n';
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2));
        f.ladder(0,154,162); f.ladder(1.1F,154,162);
        require(f.matron->request_climb_goal(RVec3(0,161,-0.55),Vec3::sAxisZ()),"routecraft: changed geometry start");
        for(int i=0;i<90;++i) f.tick();
        const auto plans_before=f.matron->climb_report().plans;
        f.box(RVec3(0,157.5,-0.55),Vec3(0.35F,0.1F,0.4F));
        for(int i=0;i<3000 && f.matron->climb_report().status==MatronRoutecraft::Status::Moving;++i) f.tick();
        require(f.matron->climb_report().status==MatronRoutecraft::Status::Holding,"routecraft: new obstruction replans to lateral detour");
        require(f.matron->climb_report().plans>plans_before && f.max_x>0.8F,"routecraft: detour was physically executed");
        std::cout<<"routecraft replan count="<<f.matron->climb_report().plans<<" detour_x="<<f.max_x<<'\n';
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2)); f.ladder(0,154,161);
        require(f.matron->request_climb_goal(RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: grip removal start");
        for(int i=0;i<90;++i) f.tick();
        const auto plans_before=f.matron->climb_report().plans;
        const RVec3 hand=f.matron->climb_report().hand;
        for(std::size_t i=1;i<f.solids.size();++i) if(Vec3(f.physics.GetBodyInterface().GetPosition(f.solids[i])-hand).Length()<0.05F) f.physics.GetBodyInterface().RemoveBody(f.solids[i]);
        for(int i=0;i<1800 && f.matron->climb_report().status==MatronRoutecraft::Status::Moving;++i) f.tick();
        require(f.matron->climb_report().status==MatronRoutecraft::Status::Holding && f.matron->climb_report().plans>plans_before,"routecraft: one missing grip locally replans");
        f.matron->cancel_climb_goal();
        require(f.matron->climb_report().force==0,"routecraft: explicit interruption clears contact");
        for(int i=0;i<270;++i) f.tick();
        require(std::abs(f.at().GetY()-155.05)<0.05,"routecraft: interrupted grounded recovery");
        std::cout<<"routecraft interrupted recovery peak_overlap="<<f.penetration<<" controlled_overlap="<<f.controlled_penetration<<'\n';
    }
    {
        Fixture f(RVec3(0,157,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2)); f.ladder(0,154,161);
        require(f.matron->request_climb_goal(RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: disturbed load start");
        f.finish_plan();
        require(f.matron->climb_report().status==MatronRoutecraft::Status::Moving,"routecraft: disturbed load plan ready");
        f.measured_force=0; f.measured_power=0;
        // Planning advances real physics; establish the same downward momentum
        // as the original test through an impulse, never a velocity write.
        f.physics.GetBodyInterface().AddImpulse(f.matron->body(),Vec3(0,-440,0)-
            Matron::kMassKg*f.physics.GetBodyInterface().GetLinearVelocity(f.matron->body()));
        f.tick();
        require(f.measured_force>4400 && f.measured_power>8900,"routecraft: disturbed motion reaches power limit with finite force");
        std::cout<<"routecraft measured saturation N="<<f.measured_force<<" W="<<f.measured_power<<'\n';
        Matron::Saved saved; saved.state=Matron::State::Dormant;
        f.matron->restore(saved);
        require(f.matron->climb_report().force==0 && f.matron->climb_report().status==MatronRoutecraft::Status::Released,"routecraft: restore clears controller");
    }
    {
        Fixture f(RVec3(0,157,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2));
        f.ladder(0,154,161);
        f.physics.SetGravity(Vec3(0,-12,0)); // stronger physical load isolates force from power saturation
        require(f.matron->request_climb_goal(RVec3(0,160,-0.55),Vec3::sAxisZ()), "routecraft: force saturation setup");
        f.finish_plan();
        require(f.matron->climb_report().status==MatronRoutecraft::Status::Moving,"routecraft: force saturation plan ready");
        // First publication can catch a falling body close to the reach
        // limit. Wait for an actual loaded ascent before the independent
        // force-cap disturbance; every waiting tick still checks real physics.
        const auto loaded_ascent=[&]() {
            const auto &r=f.matron->climb_report();
            return r.phase==MatronRoutecraft::PlanPhase::None && r.force>0 &&
                f.physics.GetBodyInterface().GetLinearVelocity(f.matron->body()).GetY()>=0 &&
                Vec3(r.hand-f.at()).Length()<MatronRoutecraft::kReach-0.20F;
        };
        for(int i=0;i<180 && !loaded_ascent();++i) f.tick();
        require(loaded_ascent(),"routecraft: force-cap impulse starts during a physical loaded ascent");
        const RVec3 saturation_start=f.at();
        const auto saturation_report=f.matron->climb_report();
        f.measured_force=0; f.measured_power=0;
        f.physics.GetBodyInterface().AddImpulse(f.matron->body(),Vec3(0,-286,0)-
            Matron::kMassKg*f.physics.GetBodyInterface().GetLinearVelocity(f.matron->body()));
        f.tick();
        if(!(f.measured_force>5999 && f.measured_power<9000))
            std::cerr<<"routecraft force saturation start="<<saturation_start<<" hand="<<saturation_report.hand
                <<" initial_force="<<saturation_report.force<<" now="<<f.at()
                <<" measured_force="<<f.measured_force<<" measured_power="<<f.measured_power
                <<" status="<<int(f.matron->climb_report().status)<<" phase="<<int(f.matron->climb_report().phase)
                <<" plans="<<f.matron->climb_report().plans<<'\n';
        require(f.measured_force>5999 && f.measured_power<9000, "routecraft: actual 6000 N saturation independent of power cap");
        std::cout << "routecraft measured force saturation N=" << f.measured_force << " W=" << f.measured_power << '\n';
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2)); f.ladder(0,154,161);
        require(f.matron->request_climb_goal(RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: capture interruption start");
        f.finish_plan();
        require(f.matron->climb_report().force>0,"routecraft: loaded support before capture");
        f.matron->begin_grab(f.at()+Vec3(0,0,0.5F));
        require(f.matron->climb_report().status==MatronRoutecraft::Status::Released && f.matron->climb_report().force==0,"routecraft: capture immediately owns the hands");
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(4,0.1F,2)); f.ladder(0,154,161);
        require(f.matron->request_climb_goal(RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: moved support start");
        for(int i=0;i<45;++i) f.tick();
        require(f.matron->climb_report().phase==MatronRoutecraft::PlanPhase::None &&
            f.matron->climb_report().force>0,"routecraft: loaded support before displacement");
        for(std::size_t i=1;i<f.solids.size();++i) {
            const auto id=f.solids[i];
            f.physics.GetBodyInterface().SetPosition(id,f.physics.GetBodyInterface().GetPosition(id)+Vec3(4,0,0),EActivation::DontActivate);
        }
        f.tick();
        require(f.matron->climb_report().force==0,"routecraft: moved support applies no first-tick force");
        f.finish_plan();
        require(f.matron->climb_report().status==MatronRoutecraft::Status::NoRoute && f.matron->climb_report().force==0,"routecraft: displaced anchors invalidate before load");
    }
    {
        // Unfamiliar pitch, missing every third rung, shifted world origin.
        Fixture f(RVec3(2.3,157.07,-3.55));
        f.box(RVec3(2.3,155.9,-4),Vec3(3,0.1F,2));
        int rung=0;
        for(float y=156.27F;y<163;y+=0.37F,++rung) if(rung%3!=1) f.box(RVec3(2.3,y,-3),Vec3(0.28F,0.02F,0.02F));
        require(f.go(RVec3(2.3,162,-3.55)) && f.go(RVec3(2.3,157.2,-3.55)),"routecraft: unfamiliar shifted sparse pitch up/down");
        std::cout<<"routecraft unfamiliar sparse descent y="<<f.at().GetY()<<'\n';
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2)); f.ladder(0,154,161);
        require(f.matron->request_climb_goal(RVec3(0,160,-0.55),Vec3::sAxisZ()),"routecraft: selected support before occlusion");
        f.finish_plan();
        require(f.matron->climb_report().force>0,"routecraft: support is loaded before occlusion");
        f.measured_force=0; f.measured_power=0;
        // Wall misses the capsule but separates its shoulders from every bar.
        f.box(RVec3(0,157.5,-0.15),Vec3(2,3.5F,0.025F));
        f.tick();
        require(f.matron->climb_report().force==0,"routecraft: occluded support applies no first-tick force");
        f.finish_plan();
        require(f.matron->climb_report().status==MatronRoutecraft::Status::NoRoute && f.measured_force==0,"routecraft: selected occluded contact applies no actual force");
    }
    {
        Fixture f(RVec3(0,157,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2));
        MutableCompoundShapeSettings settings;
        settings.AddShape(Vec3(10,158,0),Quat::sIdentity(),new BoxShape(Vec3(0.28F,0.02F,0.02F)));
        settings.AddShape(Vec3(0,158,0),Quat::sIdentity(),new BoxShape(Vec3(0.28F,0.02F,0.02F)));
        Ref<MutableCompoundShape> shape=static_cast<MutableCompoundShape *>(settings.Create().Get().GetPtr());
        BodyCreationSettings body(shape,RVec3::sZero(),Quat::sIdentity(),EMotionType::Static,object_layers::kStatic);
        const BodyID id=f.physics.GetBodyInterface().CreateAndAddBody(body,EActivation::DontActivate); f.solids.push_back(id);
        require(f.matron->request_climb_goal(RVec3(0,157.2,-0.55),Vec3::sAxisZ()),"routecraft: compound contact selected");
        f.finish_plan();
        require(f.matron->climb_report().status==MatronRoutecraft::Status::Moving ||
            f.matron->climb_report().status==MatronRoutecraft::Status::Holding,"routecraft: compound contact plan ready");
        const Vec3 old_com=shape->GetCenterOfMass();
        shape->RemoveShape(1);
        f.physics.GetBodyInterface().NotifyShapeChanged(id,old_com,false,EActivation::DontActivate);
        f.tick();
        require(f.matron->climb_report().force==0,"routecraft: removed compound child applies no first-tick force");
        f.finish_plan();
        require(f.matron->climb_report().status==MatronRoutecraft::Status::NoRoute && f.matron->climb_report().force==0,"routecraft: stale removed compound child safely releases");
    }
    {
        Fixture f(RVec3(0,157,-0.55));
        f.box(RVec3(0,153.9,-1),Vec3(3,0.1F,2));
        BodyCreationSettings bar(new BoxShape(Vec3(0.28F,0.02F,0.02F)),RVec3(0,158,0),Quat::sIdentity(),EMotionType::Kinematic,object_layers::kMoving);
        f.solids.push_back(f.physics.GetBodyInterface().CreateAndAddBody(bar,EActivation::Activate));
        require(!f.matron->request_climb_goal(RVec3(0,157.2,-0.55),Vec3::sAxisZ()),"routecraft: moving support family explicitly rejected");
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1), Vec3(3,0.1F,2));
        const auto phantom = [&](bool sensor, RVec3 at, Vec3 half) {
            BodyCreationSettings settings(new BoxShape(half),at,Quat::sIdentity(),EMotionType::Static,
                sensor ? object_layers::kStatic : object_layers::kHand);
            settings.mIsSensor = sensor;
            f.solids.push_back(f.physics.GetBodyInterface().CreateAndAddBody(settings,EActivation::DontActivate));
        };
        phantom(true,RVec3(0,156,0),Vec3(0.28F,0.02F,0.02F));
        phantom(false,RVec3(0,156.3,0),Vec3(0.28F,0.02F,0.02F));
        require(!f.matron->request_climb_goal(RVec3(0,156,-0.55),Vec3::sAxisZ()),
            "routecraft: sensors and noncolliding layers cannot supply grips");
        f.ladder(0,154,160);
        phantom(true,RVec3(0,157,-0.4),Vec3(2,0.1F,1));
        phantom(false,RVec3(0,158,-0.4),Vec3(2,0.1F,1));
        require(f.go(RVec3(0,159,-0.55)), "routecraft: sensor and noncolliding barriers do not obstruct body or hand");
        require(f.matron->view().climbing && !f.matron->view().clinging,
            "routecraft: filtered traversal retains controller ownership without riding");
        require(f.go(RVec3(0,155.2,-0.55)), "routecraft: filtered barriers also permit descent");
        f.matron->cancel_climb_goal();
        for (int i = 0; i < 90; ++i) f.tick();
        require(std::abs(f.at().GetY()-(154+Matron::kStandCentre))<0.025F &&
            std::abs(f.matron->view().floor_y-154)<0.025F && !f.matron->view().climbing && !f.matron->view().clinging,
            "routecraft: filtered traversal releases onto actual floor without riding");
    }
    {
        Simulation world(InitialSpawn::Deck154);
        world.launch_matron_for_test({11,199.32,-131.41});
        Vector3 previous{11,199.32,-131.41};
        double maximum_step = 0;
        const auto tick = [&]() {
            (void)world.advance_frame(Simulation::kFixedStepSeconds);
            const auto m = world.matron_state();
            const double dx = m.position.x - previous.x, dy = m.position.y - previous.y, dz = m.position.z - previous.z;
            maximum_step = std::max(maximum_step, std::sqrt(dx*dx + dy*dy + dz*dz));
            require(m.alive && maximum_step < 0.2, "routecraft: panel alive with bounded physical steps");
            previous = m.position;
        };
        const auto travel = [&](Vector3 goal, double floor) {
            const double start_y=previous.y; // launch fixture pose before the first view refresh
            const auto started = std::chrono::steady_clock::now();
            const bool accepted = world.request_matron_climb_goal(goal, {0,0,1});
            std::cout << "panel goal_y=" << goal.y << " accepted=" << accepted << " status=" << world.matron_climb_status()
                << " plan_ms=" << std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()
                << " native_queries=" << world.matron_climb_queries() << std::endl;
            require(accepted, "routecraft: actual panel route found");
            unsigned replans=0;
            for (int i = 0; i < 9000 && world.matron_climb_status() == 1; ++i) {
                const double progress=(world.matron_state().position.y-start_y)/(goal.y-start_y);
                if (replans<2 && progress>(replans+1)/3.0) {
                    require(world.matron_state().climbing && world.matron_state().climb_force_n>0,
                        "routecraft: panel replan starts with a real loaded grip");
                    const auto replanning = std::chrono::steady_clock::now();
                    require(world.request_matron_climb_goal(goal,{0,0,1}), "routecraft: dense panel mid-climb replan");
                    std::cout << "panel replan_ms=" << std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-replanning).count()
                        << " native_queries=" << world.matron_climb_queries() << " at_y=" << world.matron_state().position.y << '\n';
                    ++replans;
                }
                tick();
            }
            require(replans==2,"routecraft: panel performs both physical mid-climb replans");
            require(world.matron_climb_status() == 2, "routecraft: actual panel physical arrival");
            world.cancel_matron_climb_goal();
            for (int i = 0; i < 30; ++i) tick();
            for (int i = 0; i < 90; ++i) {
                tick(); const auto m = world.matron_state();
                require(std::abs(m.position.y - (floor + Matron::kStandCentre)) < 0.025 && !m.climbing && !m.grip_on && m.carrying == 0,
                    "routecraft: panel destination standing without grip for one second");
            }
            std::cout << "panel arrived_y=" << world.matron_state().position.y << " standing_ticks=90 max_step=" << maximum_step << '\n';
        };
        travel({11,221.37,-130.55}, 220.25);
        travel({11,199.4,-131.41}, 198.25);
    }
    {
        // Production panel collision leaves with independent per-tick force,
        // power, gravity, reach, and penetration measurements in Fixture::tick.
        Fixture f(RVec3(11,199.32,-131.41));
        f.box(RVec3(11,220.15,-129.6),Vec3(3,0.1F,1.13F));
        f.kit = std::make_unique<kit::Kit>(f.physics,object_layers::kStatic,object_layers::kMoving);
        bands::CounterweightWell well;
        bands::build_counterweight_well(*f.kit,well);
        require(f.go(RVec3(11,221.37,-130.55)), "routecraft: measured production panel ascent");
        require(f.go(RVec3(11,199.4,-131.41)), "routecraft: measured production panel descent");
        std::cout << "panel measured_force=" << f.measured_force << " sampled_power=" << f.measured_power
            << " controlled_overlap=" << f.controlled_penetration << '\n';
        f.timings("production_panel");
    }
    {
        // Same generic search, translated dense members and unrelated nearby
        // distractors. The body must go sideways around the new solid.
        Fixture f(RVec3(20,155.07,11.45));
        f.box(RVec3(20,153.9,11), Vec3(4,0.1F,3));
        for (int i = 0; i < 50; ++i) f.box(RVec3(20,154.3 + i*0.44,12), Vec3(1.03F,0.02F,0.02F));
        for (int i = 0; i < 80; ++i) f.box(RVec3(21.7,154.3 + i*0.27,12.7), Vec3(0.28F,0.02F,0.02F));
        f.box(RVec3(20,160,11.45), Vec3(0.4F,0.2F,0.4F));
        require(f.go(RVec3(20,175,11.45)), "routecraft: dense translated fixture with distractors");
        require(f.min_x < 19.4F || f.max_x > 20.6F, "routecraft: dense fixture executes lateral detour");
        require(f.go(RVec3(20,155.2,11.45)), "routecraft: dense translated descent");
        std::cout << "dense fixture min_x=" << f.min_x << " max_x=" << f.max_x << " final_y=" << f.at().GetY() << '\n';
        f.timings("dense_translated");
    }
    {
        Fixture f(RVec3(0,155.07,-0.55));
        f.box(RVec3(0,153.9,-1), Vec3(3,0.1F,2));
        for (int i = 0; i < 2050; ++i) f.box(RVec3(i*0.0001,156,0), Vec3(0.28F,0.02F,0.02F));
        require(!f.matron->request_climb_goal(RVec3(0,156,-0.55),Vec3::sAxisZ()) &&
            f.matron->climb_report().status == MatronRoutecraft::Status::BudgetExceeded,
            "routecraft: genuine collector overflow rejects the complete query");
    }
    std::cout<<"PASS: matron-routecraft\n";
}
