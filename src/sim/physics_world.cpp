// The physics world: construction and teardown, the tick, bodies, the world's solids and supports.
// Members of Simulation::PhysicsWorld (physics_world.hpp).

#include "sim/physics_world.hpp"
#include "sim/twin_tower.hpp"
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

namespace scraperx::sim::detail {

std::mutex JoltRuntimeLease::mutex_;
std::uint32_t JoltRuntimeLease::lease_count_ = 0;

} // namespace scraperx::sim::detail

namespace scraperx::sim {

namespace {

// The presentation's static dressing as native collision (see
// kWorldSolidEntityId). Boxes are oriented; hulls are world-space point sets.
struct WorldSolidBox final {
    float px, py, pz;
    float qx, qy, qz, qw;
    float hx, hy, hz;
};
struct WorldSolidHull final {
    std::uint32_t first;
    std::uint32_t count;
};
#include "sim/world_solids.inc"

} // namespace

Simulation::PhysicsWorld::PhysicsWorld(const InitialSpawn initial_spawn,
                                       std::vector<Simulation::TestExtraSolid> test_extra_solids)
    : temp_allocator_(8U * 1024U * 1024U),
      job_system_(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 1),
      test_extra_solids_(std::move(test_extra_solids)) {
    // Raised from 1024: the world's solid dressing adds ~1 600 static
    // bodies (world_solids.inc) on top of the tower's and the bands'.
    physics_system_.Init(4096,
                         0,
                         2048,
                         1024,
                         broadphase_layer_interface_,
                         object_vs_broadphase_filter_,
                         object_layer_pair_filter_);
    physics_system_.SetContactListener(&contact_listener_);

    auto &bodies = physics_system_.GetBodyInterface();

    // Grade: the yard the player is born on, outdoors, wide enough to walk
    // the tower approach.
    deck_id_ = add_box(bodies,
                       JPH::Vec3(240.0F, 0.5F, 240.0F),
                       JPH::RVec3(0.0, -0.5, -60.0),
                       JPH::EMotionType::Static,
                       object_layers::kStatic,
                       0.6F,
                       Simulation::kStaticDeckEntityId);

    // The neighboring shaft remains solid and stands on grade. Its face
    // leaves a 48 m nominal gap to the main frame, retaining x=-30 offset.
    const float mass_half_height = static_cast<float>(Simulation::kTowerHeightMeters * 0.5);
    tower_id_ = add_box(bodies,
                        JPH::Vec3(46.0F, mass_half_height, 40.0F),
                        JPH::RVec3(-30.0, mass_half_height, TwinTower::kCenterZ),
                        JPH::EMotionType::Static,
                        object_layers::kStatic,
                        0.8F,
                        Simulation::kTowerEntityId);

    // The proving ground's movement fixtures (WO-002, WO-003): only in a
    // world started at one of its spawns, never in the game's.
    if (scraperx::sim::is_proving_spawn(initial_spawn)) {
        translating_support_id_ = add_box(bodies,
                                          JPH::Vec3(2.75F, 0.25F, 2.75F),
                                          JPH::RVec3(0.0, 0.25, 8.0),
                                          JPH::EMotionType::Kinematic,
                                          object_layers::kMoving,
                                          0.8F,
                                          Simulation::kTranslatingSupportEntityId);

        rotating_support_id_ = add_box(bodies,
                                       JPH::Vec3(3.0F, 0.25F, 3.0F),
                                       JPH::RVec3(-8.0, 0.25, 0.0),
                                       JPH::EMotionType::Kinematic,
                                       object_layers::kMoving,
                                       0.8F,
                                       Simulation::kRotatingSupportEntityId);

        vault_rail_id_ = add_box(bodies,
                                 JPH::Vec3(0.22F, 0.475F, 2.5F),
                                 JPH::RVec3(5.0, 0.475, -6.0),
                                 JPH::EMotionType::Static,
                                 object_layers::kStatic,
                                 0.7F,
                                 Simulation::kVaultRailEntityId);

        mantle_ledge_id_ = add_box(bodies,
                                   JPH::Vec3(2.0F, 0.775F, 2.0F),
                                   JPH::RVec3(11.0, 0.775, -6.0),
                                   JPH::EMotionType::Static,
                                   object_layers::kStatic,
                                   0.7F,
                                   Simulation::kMantleLedgeEntityId);

        hang_ledge_id_ = add_box(bodies,
                                 JPH::Vec3(2.5F, 1.8F, 2.5F),
                                 JPH::RVec3(11.0, 1.8, 4.0),
                                 JPH::EMotionType::Static,
                                 object_layers::kStatic,
                                 0.7F,
                                 Simulation::kHangLedgeEntityId);

        moving_ledge_id_ = add_box(bodies,
                                   JPH::Vec3(2.0F, 1.8F, 2.0F),
                                   JPH::RVec3(9.0, 1.8, kMovingLedgeCenterZ),
                                   JPH::EMotionType::Kinematic,
                                   object_layers::kMoving,
                                   0.8F,
                                   Simulation::kMovingLedgeEntityId);

        blocked_ledge_id_ = add_box(bodies,
                                    JPH::Vec3(1.5F, 0.775F, 1.5F),
                                    JPH::RVec3(-6.0, 0.775, -8.0),
                                    JPH::EMotionType::Static,
                                    object_layers::kStatic,
                                    0.7F,
                                    Simulation::kBlockedLedgeEntityId);

        blocked_ledge_canopy_id_ = add_box(bodies,
                                           JPH::Vec3(2.2F, 0.15F, 2.2F),
                                           JPH::RVec3(-6.0, 2.7, -8.0),
                                           JPH::EMotionType::Static,
                                           object_layers::kStatic,
                                           0.7F,
                                           Simulation::kBlockedLedgeCanopyEntityId);

        {
            const auto crawl_part = [this, &bodies](const JPH::Vec3 half, const JPH::RVec3 at) {
                machine_bodies_.push_back(add_box(bodies, half, at, JPH::EMotionType::Static,
                                                  object_layers::kStatic, 0.7F,
                                                  Simulation::kCrawlBeamEntityId));
            };
            const float beam_top = kCrawlBeamUndersideY + 2.0F * kCrawlBeamHalfY;
            crawl_part(JPH::Vec3(kCrawlLaneHalfX, kCrawlBeamHalfY, kCrawlBeamHalfZ),
                       JPH::RVec3(kCrawlLaneCenterX, kCrawlBeamUndersideY + kCrawlBeamHalfY,
                                  kCrawlBeamZ));
            for (const float side : {-1.0F, 1.0F}) {
                crawl_part(JPH::Vec3(kCrawlPostHalf, beam_top * 0.5F, kCrawlPostHalf),
                           JPH::RVec3(kCrawlLaneCenterX + side * (kCrawlLaneHalfX + kCrawlPostHalf),
                                      beam_top * 0.5F, kCrawlBeamZ));
            }
        }
    }

    if (scraperx::sim::is_proving_spawn(initial_spawn)) {
        const auto bar_part = [this, &bodies](const JPH::Vec3 half, const JPH::RVec3 at) {
            machine_bodies_.push_back(add_box(bodies, half, at, JPH::EMotionType::Static,
                                              object_layers::kStatic, 0.7F,
                                              Simulation::kMonkeyBarsEntityId));
        };
        for (int bar = 0; bar < 6; ++bar) {
            bar_part(JPH::Vec3(0.04F, 0.04F, 0.62F), JPH::RVec3(static_cast<double>(bar), 3.1, -20.0));
        }
        for (const double side : {-1.0, 1.0}) {
            bar_part(JPH::Vec3(3.3F, 0.12F, 0.12F), JPH::RVec3(2.5, 3.26, -20.0 + side * 0.74));
            for (const double end : {-0.9, 5.9}) {
                bar_part(JPH::Vec3(0.12F, 1.69F, 0.12F), JPH::RVec3(end, 1.69, -20.0 + side * 0.74));
            }
        }
        bar_part(JPH::Vec3(1.0F, 0.25F, 0.5F), JPH::RVec3(6.9, 0.25, -20.0));
        hook_rack_id_ = add_box(bodies, JPH::Vec3(0.15F, 0.6F, 0.15F), JPH::RVec3(-2.0, 0.6, -20.9),
                                JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                                Simulation::kHookRackEntityId);
        machine_bodies_.push_back(hook_rack_id_);
        // The run wall, 18 m long (x -12 to 6), and a second facing its south
        // face across a 2.5 m passage (faces at z -27.25 and -29.75): the
        // wall-run chain is kicked from one to the other and back
        // (run_wall_chain), each crossing at a sprint covering 4 m of wall.
        // South, so the north face's wall-run lane and the wall climb's
        // run-up at x -6 stay as they were.
        machine_bodies_.push_back(add_box(bodies, JPH::Vec3(9.0F, 2.0F, 0.25F), JPH::RVec3(-3.0, 2.0, -27.0),
                                          JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                                          Simulation::kRunWallEntityId));
        machine_bodies_.push_back(add_box(bodies, JPH::Vec3(9.0F, 2.0F, 0.25F), JPH::RVec3(-3.0, 2.0, -30.0),
                                          JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                                          Simulation::kRunWallEntityId));
    }

    build_stack(bodies);
    build_world_solids(bodies);

    player_shape_ = new JPH::CapsuleShape(0.55F, kPlayerRadius);
    player_crouch_shape_ =
        new JPH::CapsuleShape(kPlayerCrouchHalfHeight - kPlayerRadius, kPlayerRadius);
    // The torso left when both legs are torn off (physics_world_fatality.cpp):
    // the standing capsule's upper half, hips (its middle) to crown.
    player_torso_shape_ = new JPH::RotatedTranslatedShape(
        JPH::Vec3(0.0F, 0.5F * kPlayerHalfHeight, 0.0F), JPH::Quat::sIdentity(),
        new JPH::CapsuleShape(0.5F * kPlayerHalfHeight - kPlayerRadius, kPlayerRadius));
    grip_region_shape_ = new JPH::BoxShape(kGripSearchHalf);
    // A climber's moves are swept with a body 5 cm slimmer than the
    // capsule: a body on its holds already touches the rungs it climbs.
    climb_sweep_shape_ =
        new JPH::CapsuleShape(0.55F + kPlayerRadius - kClimbSweepRadius, kClimbSweepRadius);
    JPH::BodyCreationSettings player_settings(player_shape_,
                                              spawn_position(initial_spawn),
                                              JPH::Quat::sIdentity(),
                                              JPH::EMotionType::Dynamic,
                                              object_layers::kPlayer);
    player_settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX |
                                   JPH::EAllowedDOFs::TranslationY |
                                   JPH::EAllowedDOFs::TranslationZ;
    player_settings.mAllowSleeping = false;
    player_settings.mFriction = 0.0F;
    player_settings.mLinearDamping = 0.0F;
    player_settings.mUserData = Simulation::kPlayerEntityId;
    player_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    player_settings.mMassPropertiesOverride.mMass = kPlayerMassKg;
    player_id_ = bodies.CreateAndAddBody(player_settings, JPH::EActivation::Activate);

    // AS-006: the mechanism ascent's bands, built after the player.
    kit_ = std::make_unique<scraperx::sim::kit::Kit>(physics_system_, object_layers::kStatic,
                                                    object_layers::kMoving);
    scraperx::sim::bands::build_counterweight_well(*kit_, well_);
    scraperx::sim::bands::build_wet_isolation(*kit_, wet_);
    scraperx::sim::bands::build_plate_shop(*kit_, shop_);
    scraperx::sim::bands::build_facade_crane(*kit_, crane_);
    scraperx::sim::bands::build_midstack_service(*kit_, service_);
    // Band 0, the Stack: the ascent from grade.
    scraperx::sim::bands::build_stack(*kit_, stack_);
    // The ground slingshot, in the yard south of the tower.
    slingshot_ = std::make_unique<Slingshot>(physics_system_, *kit_, player_id_);
    // AS-012, the swing off the 220 ring's south face.
    swing_ = std::make_unique<Swing>(physics_system_, *kit_, player_id_);
    // C6, the west band's climb from the 242 ring to the 264 ring.
    ClimbC6::build(*kit_);
    // The owner's vertical machines, on their own motors, where the route
    // has no way up: from the 264 ring, the cascade mast.
    vertical_ = std::make_unique<scraperx::sim::vertical::Route>(*kit_, physics_system_);
    // Phase 3: the trapeze in the yard east of the start. Built last, so
    // every body before it keeps its place in the kit.
    trapeze_ = std::make_unique<Trapeze>(physics_system_, *kit_, player_id_);
    // Parkour step 2: the pipe gallery beside it, built after it for the
    // same reason.
    PipeGallery::build(*kit_);
    // Parkour step 5: the grappling hook, found on the 220 ring's
    // north-west corner, off the line from C's platform to the swing.
    build_hook_rack();
    // The vertical route's late bodies (the pitman's deck counterweight),
    // after every other kit body so older saves' lists still line up.
    vertical_->build_late();
    // C7, the west face's exposed climb from the 264 ring to the
    // mast-pitman walkway: the alternative to riding the mast. Last too.
    ClimbC7::build(*kit_);
    // After C7, so every save written before it still lines up.
    vertical_->build_wheel_reload();
    // The summit: the deck, the dead searchlight and the runway arm. Last.
    summit_ = std::make_unique<Summit>(physics_system_, *kit_);
    // Loose stock, the Matron's deck's and the yard's: after the summit, so older
    // saves line up.
    loose_stock_ = std::make_unique<LooseStock>(physics_system_, *kit_);
    // The yard's forklift, north of the drums: after the stock, so older saves
    // line up.
    forklifts_.push_back(std::make_unique<Forklift>(physics_system_, *kit_, object_layers::kMoving, player_id_));
    // The yard crane's, on its laydown facing east.
    forklifts_.push_back(std::make_unique<Forklift>(physics_system_, *kit_, object_layers::kMoving, player_id_,
                                                    Forklift::kCraneChassisEntity, Forklift::kCraneCarriageEntity,
                                                    Forklift::kCraneParkX, Forklift::kCraneParkZ, 0.5F * JPH::JPH_PI));
    // The yard crane west of the stack and its docks on the stack's west
    // face: before the ziplines and the tower's stock, which keep clear of
    // them.
    yard_crane_ = std::make_unique<YardCrane>(physics_system_, *kit_);
    // Forklifts the crane put on the decks it serves, by their docks.
    forklifts_.push_back(std::make_unique<Forklift>(physics_system_, *kit_, object_layers::kMoving, player_id_,
                                                    Forklift::kDeck11ChassisEntity, Forklift::kDeck11CarriageEntity,
                                                    -21.5F, -139.5F, 0.0F, 11.0F));
    forklifts_.push_back(std::make_unique<Forklift>(physics_system_, *kit_, object_layers::kMoving, player_id_,
                                                    Forklift::kDeck44ChassisEntity, Forklift::kDeck44CarriageEntity,
                                                    -21.5F, -147.5F, JPH::JPH_PI, 44.0F));
    // Life L2: the yard crew, real bodies on grade (not in the kit, not saved).
    crew_ = std::make_unique<Crew>(physics_system_, object_layers::kMoving);
    // The Matron, on deck 1 (11 m): last, like the crew (not in the kit, not saved).
    // The 154 m deck stays where it is. Tests that need her there call place_matron_home(154).
    matron_ = std::make_unique<Matron>(physics_system_, object_layers::kStatic, object_layers::kMoving,
                                       object_layers::kOnSteel, player_id_);
    matron_->set_home(Matron::kStackPitch);   // deck 1, 11 m. Same place on the band, lower deck.
    // The ziplines (zipline.hpp): after everything they must keep clear of
    // (each line is swept against the world as built so far), before the
    // tower's loose stock, which keeps clear of their posts. From the lowest
    // deck down to the yard; from each deck's band across the well down to
    // the far band of the deck below, sides alternating; the same up the
    // rings. A line that does not fit is not built. The sweep sees the world
    // at rest: the yard line runs at x = -2, east of the incline's slab
    // shaft (x -7.1 .. -3.9, z -118 .. -121; the slab comes down it on a
    // ride and a cable through the shaft stopped it) and west of the yard
    // derrick's column at x = 0.8.
    // The places no fixture may take: 4 m about every place the player is
    // put down and every lane the crew walk (the tower's stock below keeps
    // clear of the same), and the hook's practice ground.
    std::vector<JPH::RVec3> keep_clear = crew_->walked(1.0F);
    for (int spawn = 0; spawn < static_cast<int>(InitialSpawn::Count); ++spawn) {
        keep_clear.push_back(spawn_position(static_cast<InitialSpawn>(spawn)));
    }
    constexpr float kKeepClearM = 4.0F;
    std::vector<JPH::AABox> reserved;
    for (const JPH::RVec3 &place : keep_clear) {
        reserved.emplace_back(JPH::Vec3(place) - JPH::Vec3(kKeepClearM, 2.0F, kKeepClearM),
                              JPH::Vec3(place) + JPH::Vec3(kKeepClearM, 2.5F, kKeepClearM));
    }
    reserved.push_back(hook_practice_ground());
    zipline_ = std::make_unique<Zipline>(physics_system_, *kit_, player_id_, std::move(reserved));
    corpses_ = std::make_unique<Corpses>(physics_system_, object_layers::kCorpse);
    (void)zipline_->add_line(JPH::RVec3(-2.0, 22.0, -125.5), JPH::RVec3(-2.0, 0.0, -72.0));
    // Across the well, from the band's inner edge (the top post 1.4 m in from
    // it, its stand toward the well hugging the edge, the rest of the band
    // open; the bottom post a meter and a half in from the far band's) to the
    // far band of the deck below: south to north on the even decks, east to
    // west on the odd, along the band wherever it first fits; and where the
    // near band has no room, from the far band back across.
    const auto across_well = [&](const double top_y, const double bottom_y, const double top_inner,
                                 const double bottom_inner, const bool south_to_north) {
        for (const double side : {1.0, -1.0}) {
            for (const double along : {6.0, -8.0, 10.0, -12.0, 0.0, -4.0, 3.0, 14.0, -14.0}) {
                const double top_out = side * (top_inner + 1.4);
                const double bottom_out = -side * (bottom_inner + 1.5);
                const bool built = south_to_north
                    ? zipline_->add_line(JPH::RVec3(along, top_y, kStackCenterZ + top_out),
                                         JPH::RVec3(along, bottom_y, kStackCenterZ + bottom_out))
                    : zipline_->add_line(JPH::RVec3(kStackCenterX + top_out, top_y, kStackCenterZ + along),
                                         JPH::RVec3(kStackCenterX + bottom_out, bottom_y, kStackCenterZ + along));
                if (built) {
                    return;
                }
            }
        }
    };
    const double stack_inner = kStackHalfExtent - kStackDeckBandDepth;
    for (int level = 2; level <= kStackLevelCount; ++level) {
        across_well(kStackLevelHeight * level, kStackLevelHeight * (level - 1), stack_inner, stack_inner,
                    level % 2 == 0);
    }
    // The upper rings (world_solids.inc, UpperDeck: 4 m bands whose inner
    // edges close in 0.9 m a ring going up, from 21.1 m out at 176 m).
    for (int ring = 1; ring <= 10; ++ring) {
        const double top_y = 176.25 + 22.0 * ring;
        across_well(top_y, top_y - 22.0, 21.1 - 0.9 * ring, 21.1 - 0.9 * (ring - 1), ring % 2 == 0);
    }
    // The Matron knows the lines (and goes down one after the player in the Turn).
    matron_->set_zipline(zipline_.get());
    // The tower's lived-in spots of loose stock: after every other body, so
    // each place is tested against all of them, and clear by kKeepClearM of
    // every place the player is put down and of every lane the crew walk.
    // Their kit bodies come last, so older saves' lists line up.
    loose_stock_->dress_tower(keep_clear, kKeepClearM);
    // The yard crane's devices and the steel laid for its magnet: after the
    // tower's stock, so saves made before them line up too.
    loose_stock_->lay_crane_steel();
    yard_crane_->build_devices();
    // The weights machines are started with: after the devices, so saves made
    // before them line up too.
    loose_stock_->lay_machine_weights();
    // What the Matron takes and throws as a pest (matron.hpp): after the
    // weights, so saves made before them line up too.
    loose_stock_->lay_matron_throwables();
    {
        std::vector<Matron::Throwable> throwables;
        for (int i = loose_stock_->count() - LooseStock::kMatronThrowables; i < loose_stock_->count(); ++i) {
            const kit::BodyIndex body = loose_stock_->body(i);
            throwables.push_back({kit_->body_id(body), kit_->body_entity(body),
                                  LooseStock::carry_handle(loose_stock_->placed()[static_cast<std::size_t>(i)].kind)});
        }
        matron_->set_throwables(std::move(throwables));
    }
    // The bay on the 154 m deck (bay.hpp): its kit bodies after every other's,
    // so older saves' lists line up.
    bay_ = std::make_unique<Bay>(physics_system_, *kit_);
    matron_->set_shared_survival(&bay_->district());
    // Append static exterior access after existing saved kit bodies.
    TwinTower::build(*kit_);
    // The hits heard (impacts.hpp): every piece of stock, each forklift's frame
    // and forks, the yard crane's hook block. After the tower's stock, so it
    // has them all.
    impacts_ = std::make_unique<ImpactLedger>(physics_system_);
    const auto track = [this](const kit::BodyIndex body, const ImpactSound sound) {
        impacts_->track(kit_->body_id(body), kit_->body_entity(body), sound, kit_->body_mass(body));
    };
    for (int i = 0; i < loose_stock_->count(); ++i) {
        track(loose_stock_->body(i), LooseStock::sound(loose_stock_->placed()[static_cast<std::size_t>(i)].kind));
    }
    for (const std::unique_ptr<Forklift> &truck : forklifts_) {
        track(truck->chassis_body(), ImpactSound::Machine);
        track(truck->carriage_body(), ImpactSound::Machine);
    }
    track(yard_crane_->hook_body(), ImpactSound::Machine);
    track(yard_crane_->magnet_body(), ImpactSound::HeavySteel);
    track(yard_crane_->spreader_body(), ImpactSound::HeavySteel);

    // Tests only: the extra solids built after every body the world builds.
    build_test_extra_solids(bodies, std::numeric_limits<double>::quiet_NaN(), true);

    physics_system_.OptimizeBroadPhase();

    checkpoint_position_ = JPH::RVec3(0.0, 0.9, 0.0);
    commit_machine_checkpoint();

    read_state();
}

Simulation::PhysicsWorld::~PhysicsWorld() {
    physics_system_.SetContactListener(nullptr);
    if (carry_constraint_ != nullptr) {
        physics_system_.RemoveConstraint(carry_constraint_);
        carry_constraint_ = nullptr;
    }
    drop_hang_arms();
    stow_hook(physics_system_.GetBodyInterface());
    matron_.reset();
    bay_.reset();
    summit_.reset();
    yard_crane_.reset();
    forklifts_.clear();
    impacts_.reset();
    loose_stock_.reset();
    vertical_.reset();
    trapeze_.reset();
    swing_.reset();
    slingshot_.reset();
    kit_.reset();
    for (JPH::Ref<JPH::TwoBodyConstraint> &constraint : machine_constraints_) {
        if (constraint != nullptr) {
            physics_system_.RemoveConstraint(constraint);
        }
    }
    machine_constraints_.clear();
    auto &bodies = physics_system_.GetBodyInterface();
    for (auto it = machine_bodies_.rbegin(); it != machine_bodies_.rend(); ++it) {
        remove_and_destroy(bodies, *it);
    }
    machine_bodies_.clear();
    remove_and_destroy(bodies, tower_id_);
    remove_and_destroy(bodies, player_id_);
    remove_and_destroy(bodies, blocked_ledge_canopy_id_);
    remove_and_destroy(bodies, blocked_ledge_id_);
    remove_and_destroy(bodies, moving_ledge_id_);
    remove_and_destroy(bodies, hang_ledge_id_);
    remove_and_destroy(bodies, mantle_ledge_id_);
    remove_and_destroy(bodies, vault_rail_id_);
    remove_and_destroy(bodies, rotating_support_id_);
    remove_and_destroy(bodies, translating_support_id_);
    remove_and_destroy(bodies, deck_id_);
}

// The remote's dead man is under the thumb, not the feet: a rider swaying on
// a load keeps it worked; hands full, on a hold, in a seat or at a wheel let
// it go.
bool Simulation::PhysicsWorld::yard_crane_able() const noexcept {
    if (carry_constraint_ != nullptr || traversal_state_ != TraversalState::None ||
        (active_forklift() >= 0 && forklifts_[static_cast<std::size_t>(active_forklift())]->driving()) ||
        slingshot_->controls_player() || swing_->controls_player() ||
        trapeze_->controls_player() || zip_riding_) {
        return false;
    }
    const JPH::RVec3 at = physics_system_.GetBodyInterfaceNoLock().GetPosition(player_id_);
    return std::hypot(static_cast<float>(at.GetX()) - YardCrane::kMastX, static_cast<float>(at.GetZ()) - YardCrane::kMastZ) <=
           YardCrane::kRadioRangeM;
}

bool Simulation::PhysicsWorld::yard_crane_remote_in_reach() const noexcept {
    return yard_crane_->remote_in_reach(physics_system_.GetBodyInterfaceNoLock().GetPosition(player_id_));
}

bool Simulation::PhysicsWorld::at_forklift_controls(const int index) const noexcept {
    const Forklift &truck = *forklifts_[static_cast<std::size_t>(index)];
    if (!grounded_ || support_entity_id_ != truck.chassis_entity()) {
        return false;
    }
    const JPH::RVec3 at = physics_system_.GetBodyInterfaceNoLock().GetPosition(player_id_);
    const JPH::RVec3 station = truck.station();
    return std::hypot(static_cast<float>(at.GetX() - station.GetX()), static_cast<float>(at.GetZ() - station.GetZ())) <=
           Forklift::kStationReachM;
}

int Simulation::PhysicsWorld::active_forklift() const noexcept {
    for (int index = 0; index < forklift_count(); ++index) {
        if (forklifts_[static_cast<std::size_t>(index)]->driving()) {
            return index;
        }
    }
    for (int index = 0; index < forklift_count(); ++index) {
        if (at_forklift_controls(index)) {
            return index;
        }
    }
    return -1;
}

void Simulation::PhysicsWorld::step(const StepCommands &requested,
                                    const float delta_seconds,
                                    const double next_time_seconds) noexcept {
    StepCommands commands = requested;
    // A punch under way owns his hands (physics_world_strike.cpp).
    strike_owns_hands(commands);
    // WO-008: captured before anything this tick can change grounded_, so
    // it means exactly "was the player standing on something one tick ago."
    const bool was_grounded_before_tick = grounded_;

    auto &bodies = physics_system_.GetBodyInterface();
    update_support_motion(bodies, delta_seconds, next_time_seconds);

    // A toggle while airborne only: deploying/retracting on the ground is
    // meaningless and would let a grounded button-mash pre-arm the canopy.
    if (commands.parachute_toggle_requested && !was_grounded_before_tick) {
        parachute_deployed_ = !parachute_deployed_;
    }

    if (regrab_lockout_ticks_ > 0) {
        --regrab_lockout_ticks_;
    }
    if (grounded_) {
        dyno_no_catch_ = false;   // a missed lunge: the feet are down again
    }
    facing_ = normalized_horizontal(commands.facing_x, commands.facing_z);
    look_pitch_ = static_cast<float>(commands.look_pitch);

    // The forklift: the stick read as the driver's, forward and right of the
    // view. Driving, the stick and the buttons are the truck's and the body
    // stands on its floor.
    {
        const JPH::Vec3 right(-facing_.GetZ(), 0.0F, facing_.GetX());
        const double stick_forward = commands.move_input_x * facing_.GetX() + commands.move_input_z * facing_.GetZ();
        const double stick_right = commands.move_input_x * right.GetX() + commands.move_input_z * right.GetZ();
        bool driving = false;
        for (int index = 0; index < forklift_count(); ++index) {
            Forklift &truck = *forklifts_[static_cast<std::size_t>(index)];
            const bool at = at_forklift_controls(index);
            truck.pre_step(delta_seconds, commands.forklift_action && (truck.driving() || at), at,
                           static_cast<float>(stick_forward), static_cast<float>(stick_right),
                           static_cast<float>(commands.forklift_lift));
            driving = driving || truck.driving();
        }
        if (driving) {
            commands.move_input_x = 0.0;
            commands.move_input_z = 0.0;
            commands.jump_requested = false;
            commands.traversal_requested = false;
            commands.up_requested = false;
            commands.crouch_held = false;
            commands.sprint_held = false;
            commands.pick_up_requested = false;
        }
    }

    // The yard crane, by its remote: working it, the stick (a world
    // direction already) sends the hook and the hoist's buttons are held;
    // the body stands where it is.
    {
        if (commands.yard_crane_take) {
            (void)yard_crane_->take_remote(bodies.GetPosition(player_id_));
        }
        yard_crane_->pre_step(delta_seconds, commands.yard_crane_operate, yard_crane_able(),
                         static_cast<float>(commands.move_input_x), static_cast<float>(commands.move_input_z),
                         static_cast<float>(commands.yard_crane_hoist), commands.yard_crane_device,
                         carry_constraint_ != nullptr ? carried_entity_ : 0);
        if (yard_crane_->operating()) {
            commands.move_input_x = 0.0;
            commands.move_input_z = 0.0;
            commands.jump_requested = false;
            commands.traversal_requested = false;
            commands.up_requested = false;
            commands.crouch_held = false;
            commands.sprint_held = false;
            commands.pick_up_requested = false;
        }
    }

    resolve_up(commands);
    // Her hand on them (physics_world_grab.cpp): the stick is a thrash.
    const bool held_by_her = grab_pre_step(bodies, commands, delta_seconds);
    const bool body_hers = held_by_her || player_free_rot_;

    // Harnessed into the slingshot's pouch or the swing's seat, the body
    // is the machine's: no crouch, carry, rig or traversal begins.
    if (!slingshot_->controls_player() && !swing_->controls_player() && !trapeze_->controls_player()) {
        update_crouch(bodies, commands);
        update_rig(bodies, commands);
        update_carry(bodies, commands, delta_seconds);
        apply_traversal_commands(bodies, commands);
        if (traversal_state_ == TraversalState::Climbing) {
            update_climb(bodies, commands, delta_seconds);
        } else if (traversal_state_ == TraversalState::Hanging) {
            update_shimmy(bodies, commands, delta_seconds);
        } else if (traversal_state_ == TraversalState::Swinging) {
            update_swing(bodies, commands);
        }
    }

    const bool sling_was_controlling = slingshot_->controls_player();
    slingshot_->pre_step(delta_seconds, commands.sling_draw, commands.sling_yaw,
                         commands.sling_elevation, commands.sling_action, commands.sling_drop,
                         grounded_ && support_entity_id_ == Slingshot::kPouchEntity);
    swing_->pre_step(delta_seconds, commands.swing_action, commands.swing_drop);
    trapeze_->pre_step(delta_seconds, commands.trapeze_action, commands.trapeze_drop, commands.crouch_held,
                       !grounded_ && traversal_state_ == TraversalState::None);
    const bool sling_controlling =
        slingshot_->controls_player() || swing_->controls_player() || trapeze_->controls_player();
    if (sling_was_controlling && !slingshot_->controls_player() && slingshot_->state().released) {
        slingshot_flight_ = true;
    }
    if (sling_controlling) {
        release_carry();
        clear_traversal();
        parachute_deployed_ = false;
    }
    if (grounded_ && support_entity_id_ != Slingshot::kPouchEntity && !sling_controlling) {
        slingshot_flight_ = false;
    }

    if (sling_controlling || traversal_state_ != TraversalState::None) {
        jump_buffer_ticks_ = 0;
        coyote_ticks_ = 0;
    }
    if (jump_buffer_ticks_ > 0) {
        --jump_buffer_ticks_;
    }
    bool jump_started = false;
    planted_grip_ = 0.0F;
    // A teeter is ordinary locomotion's; whatever else holds the body
    // (harness, hands, a throw) ends it.
    if (sling_controlling || traversal_state_ != TraversalState::None || (slingshot_flight_ && !grounded_)) {
        teetering_ = false;
        line_held_ = false;
        flight_from_jump_ = false;
        clear_support_jolt();
    }
    apply_wind(bodies, commands.time_seconds);
    update_water(bodies, delta_seconds);
    if (!sling_controlling && traversal_state_ == TraversalState::None && !body_hers) {
        if (swimming_) {
            // Afloat: no ground to walk on, no air to steer in; the stroke
            // and the kick (physics_world_water.cpp).
            parachute_deployed_ = false;
            swim_locomotion(bodies, commands);
        } else if (slingshot_flight_ && !grounded_) {
            // Thrown: the stick steers with a bounded force (2 m/s/s at
            // full stick) and does not brake the throw to a walking speed
            // as air control would.
            bodies.AddForce(player_id_,
                            JPH::Vec3(static_cast<float>(commands.move_input_x * kPlayerMassKg * 2.0), 0.0F,
                                      static_cast<float>(commands.move_input_z * kPlayerMassKg * 2.0)));
        } else {
            jump_started = apply_locomotion(bodies, commands, delta_seconds);
        }
        if (!jump_started) {
            try_begin_hang(bodies, commands);
        }
        if (!was_grounded_before_tick) {
            apply_parachute_drag(bodies, delta_seconds);
            // Distinct from fall_peak_speed_mps_ (a running max, telemetry
            // only): this is overwritten every tick, so it always holds
            // exactly the velocity the body carries into this tick's
            // contact resolution -- what "impact speed" has to mean for
            // late deceleration (a well-timed parachute) to matter.
            pre_contact_velocity_ = bodies.GetLinearVelocity(player_id_);
            const float vertical_speed = pre_contact_velocity_.GetY();
            pre_contact_fall_speed_mps_ = std::max(0.0F, -vertical_speed);
            fall_peak_speed_mps_ =
                std::max(fall_peak_speed_mps_, pre_contact_fall_speed_mps_);
        }
    }

    if (traversal_state_ != TraversalState::None) {
        drive_traversal(bodies, delta_seconds);
        load_hold(bodies);
    }
    hook_pre_step(bodies, commands, delta_seconds);

    vertical_->pre_step(delta_seconds, commands.lift_action, grounded_ ? support_entity_id_ : 0,
                        commands.lift_haul);
    summit_->pre_step();
    // What the Matron can hear besides the player (matron.hpp hear): the last
    // step's hits, hardest first; the Counterweight Well's Stage A cage and
    // skip from their actual motion; then the yard crane's drives while they run.
    for (const ImpactSnapshot &hit : impacts_->tick()) {
        const float loud = ImpactLedger::loudness(static_cast<float>(hit.energy_j));
        // Her own throw coming down is not a sound to her (matron.hpp).
        if (loud > 0.0F && hit.entity != matron_->thrown_entity()) {
            matron_->hear(static_cast<float>(hit.at.x), static_cast<float>(hit.at.y), static_cast<float>(hit.at.z), loud);
        }
    }
    // Stage A is one passive gravity-counterweight machine, not two
    // independent things to investigate. Its player-facing cage/guide is the
    // acoustic source; the paired skip's motion is the same mechanism and
    // must not create a second false lead while she is riding the cage.
    // A pulled handle or open catch alone is still silent. CHOSEN:
    // full-speed guide/rope noise is 0.35, the yard crane's unloaded-hoist
    // baseline, and scales down with the cage's actual speed.
    constexpr float kWellMachineMoveMps = 0.1F;
    constexpr float kWellMachineFullMps = 2.5F;
    constexpr float kWellMachineFullLoudness = 0.35F;
    const float well_speed = kit_->body_velocity(well_.a_cage).Length();
    // Clinging to moving steel is proprioceptively her ride, not an external
    // acoustic lead. Feeding that same machine back through hear() makes her
    // chase the support under her instead of the player she rode after.
    if (well_speed >= kWellMachineMoveMps && !matron_->view().clinging) {
        const JPH::RVec3 at = kit_->body_center_of_mass(well_.a_cage);
        const float loudness =
            kWellMachineFullLoudness * std::min(1.0F, well_speed / kWellMachineFullMps);
        matron_->hear(static_cast<float>(at.GetX()), static_cast<float>(at.GetY()),
                      static_cast<float>(at.GetZ()), loudness);
    }
    for (const YardCrane::Noise &noise : yard_crane_->noises()) {
        if (noise.loudness > 0.0F) {
            matron_->hear(noise.x, noise.y, noise.z, noise.loudness);
        }
    }
    // Her ways out to guard: Stage A's cage, while it is down at her deck;
    // B's, while it is down at the 176 ring.
    matron_->set_ways_out(kit_->guide_travel(well_.a_cage_guide) < 0.5F,
                          kit_->guide_travel(well_.b_cage_guide) < 0.5F);
    // And whether the player has rigged them to ride: she undoes that.
    matron_->set_rigged(kit_->rope_end_entity(well_.a_rope) == kWellACageEntityId,
                        kit_->rope_end_entity(well_.b_rope) == kWellBCageEntityId);
    matron_->set_player_holding(carried_entity_);
    // The bay (bay.hpp): its district starts again by itself first; she
    // knows whether its leaf is a floor and whether the chain holds it; and
    // a climber's steps near its running fan are lost in its noise (the
    // room's own sound is not).
    bay_->pre_step(delta_seconds);
    matron_->set_bay(kit_->body_id(bay_->leaf_body()), bay_->leaf_open(), bay_->hooked());
    // Out of her locker, she looks the tower over once (her climbs are what
    // she finds).
    if (matron_->wants_routes()) {
        (void)matron_affordances();
    }
    const JPH::RVec3 player_at = bodies.GetPosition(player_id_);
    // His ankle; in her fingers, the point of it they hold.
    matron_->set_player_ankle(grip_ != nullptr ? bodies.GetWorldTransform(player_id_) * grip_on_body_
                                               : player_ankle(bodies));
    // Where his head faces (his facing, his look's pitch): hers to read only
    // through her eyes.
    {
        const float cos_p = std::cos(look_pitch_);
        matron_->set_player_look(facing_.IsNearZero()
                                     ? JPH::Vec3::sZero()
                                     : JPH::Vec3(facing_.GetX() * cos_p, std::sin(look_pitch_), facing_.GetZ() * cos_p));
    }
    // His true state, which she cannot open: her perception (matron_perception.hpp)
    // makes her evidence of it.
    matron_->pre_step(delta_seconds,
                      Stimulus(player_at, std::max(matron_loudness(bodies) * bay_->mask_factor(player_at),
                                                   static_cast<float>(commands.room_loudness))));
    grab_drive(bodies, delta_seconds);
    strike_step(bodies, commands, delta_seconds);
    if (body_hers && !was_grounded_before_tick) {
        // Off the ground in her hand (or righting after): the speed it
        // carries into contact, for the fall rules once she lets go.
        pre_contact_velocity_ = bodies.GetLinearVelocity(player_id_);
        pre_contact_fall_speed_mps_ = std::max(0.0F, -pre_contact_velocity_.GetY());
        fall_peak_speed_mps_ = std::max(fall_peak_speed_mps_, pre_contact_fall_speed_mps_);
    }
    if (const int way = matron_->take_unrig(); way >= 0) {
        // Her hands have the rope's end off the cage's eye: it hangs there on
        // its shackle, to be hooked on again.
        (void)kit_->unhook(way == 0 ? well_.a_rope : well_.b_rope);
    }
    kit_->pre_step(delta_seconds);
    crew_->pre_step(delta_seconds, bodies.GetPosition(player_id_), bodies.GetLinearVelocity(player_id_),
                    grounded_);
    contact_listener_.begin_tick();
    // A hang or a harness taken after the feet were planted takes the body
    // off them.
    contact_listener_.set_planted_grip(
        traversal_state_ == TraversalState::None && !sling_controlling ? planted_grip_ : 0.0F);
    physics_system_.Update(delta_seconds, 1, &temp_allocator_, &job_system_);
    kit_->post_step(delta_seconds);
    bay_->post_step();
    impacts_->post_step(carried_entity_);
    for (const std::unique_ptr<Forklift> &truck : forklifts_) {
        truck->post_step(delta_seconds);
    }
    yard_crane_->post_step(delta_seconds);
    zipline_->post_step(delta_seconds);
    if (zip_riding_) {
        // The hands on a zipline's bar hold what a ledge's grip holds: the
        // stop's jolt at a run-in faster than the buffer takes tears them off
        // and the body is thrown on (kLedgeGripMaxForceN, as hang_on_ledge).
        // One good arm holds kOneArmGripScale of two hands'.
        const float grip = one_arm() ? kOneArmGripScale : 1.0F;
        const float load = zipline_->state().hands_load_n;
        if (load > kLedgeGripMaxForceN * grip) {
            ++hang_over_grip_ticks_;
            hang_over_grip_impulse_ += (load - kLedgeGripMaxForceN * grip) * delta_seconds;
        } else {
            hang_over_grip_ticks_ = 0;
            hang_over_grip_impulse_ = 0.0F;
        }
        const bool wrenched = hang_over_grip_impulse_ > kLedgeGripTearImpulseNs * grip;
        if (hang_over_grip_ticks_ >= Trapeze::kSlipTicks || wrenched) {
            ++hang_slip_count_;
            ++zip_torn_count_;
            release_hang(bodies);
            if (wrenched) {
                break_arm(1);   // torn out of the hands by the jolt: the shoulder takes it
            }
        }
    }
    summit_->post_step();
    grab_post_step(bodies, delta_seconds);
    matron_->post_step(delta_seconds);
    if (arms_lost() == 2 && !matron_->claimed() && !matron_->claim_armless()) {
        // No body goes on armless (Cory, 2026-10-27: your second arm gone is
        // the fatality). She claims it (matron_hold.cpp); when she cannot
        // (dead, or out of the world), the run ends all the same.
        run_ended_by_matron_ = true;
    }
    if (matron_->take_released_edge()) {
        matron_checkpoint_pending_ = true;
    }
    slingshot_->post_step(delta_seconds);
    swing_->post_step(delta_seconds);
    trapeze_->post_step(delta_seconds);
    hook_post_step(bodies, delta_seconds);
    crew_->post_step(delta_seconds);

    SupportSample support = contact_listener_.sample();
    struck_closing_mps_ = contact_listener_.struck_closing();
    hit_closing_mps_ = contact_listener_.hit_closing();
    hit_toward_ = contact_listener_.hit_toward();
    const bool takeoff = jump_started || springboard_takeoff_;
    springboard_takeoff_ = false;
    if (takeoff || traversal_state_ != TraversalState::None) {
        support = {};
    }
    support_sample_ = support;
    grounded_ = support.grounded;
    support_entity_id_ = support.entity_id;

    // WO-008: a genuine (non-traversal, non-jump) landing is the one
    // moment fall consequence is resolved. Traversal-completion landings
    // never reach here with a real fall velocity -- complete_traversal
    // always sets a support-relative landing velocity first -- so no
    // separate traversal exemption is needed.
    bool died_this_tick = false;
    if (fatality_death_) {
        // The fatality's floor version is over (black): a death where the
        // body lies, and the checkpoint's restore, as after the hang's fall.
        fatality_death_ = false;
        leave_corpse(bodies, bodies.GetLinearVelocity(player_id_), 0.0F, false);
        restore_from_checkpoint(bodies);
        died_this_tick = true;
    }
    // While she is finishing it (Claimed) the run is hers: its end is the
    // fatality's (above), not a fall's death.
    const bool hers = matron_->claimed();
    if (!died_this_tick && grounded_ && !was_grounded_before_tick && traversal_state_ == TraversalState::None &&
        !takeoff && grip_ == nullptr && !hers) {
        last_impact_speed_mps_ = pre_contact_fall_speed_mps_;
        // Her fatality lets go only into a fall foreseen to land past this.
        static_assert(static_cast<double>(Matron::kFatalLandMps) > kLethalImpactSpeedMps,
                      "the fatality's let-go must be foreseen to kill");
        if (last_impact_speed_mps_ > kLethalImpactSpeedMps) {
            // The body stays where it hit, as it was moving into the hit.
            leave_corpse(bodies, pre_contact_velocity_, pre_contact_velocity_.Length(), false);
            restore_from_checkpoint(bodies);
            died_this_tick = true;
        } else {
            bool prepared = commands.crouch_held;
            if (precision_target_valid_) {
                const JPH::RVec3 at = bodies.GetPosition(player_id_);
                const float off = static_cast<float>(std::hypot(at.GetX() - precision_target_.GetX(),
                                                                at.GetZ() - precision_target_.GetZ()));
                if (off <= kPlayerRadius + kPrecisionLandingSlack) {
                    prepared = true;   // the legs were set for exactly this landing
                    ++precision_landing_count_;
                }
            }
            record_landing(bodies, prepared);
        }
        precision_target_valid_ = false;
    }
    // Her hands on the player (matron.hpp, her hands): at every stage, she
    // takes hold.
    if (!died_this_tick && matron_->caught(bodies.GetPosition(player_id_))) {
        // No kill on touch. Her hand goes for the ankle (matron_hold.cpp);
        // what follows is physics. A body already on a ladder is not an ankle
        // to stoop for: pulling him off the first rungs is what kept the climb
        // from ever starting.
        if (matron_->grab_ready() && traversal_state_ != TraversalState::Climbing) {
            matron_->count_catch();
            matron_->begin_grab(bodies.GetPosition(player_id_));
        } else if (matron_->shoves() && matron_->shove_ready()) {
            // While her hands cannot take hold (a grab's cooldown, a body on
            // a ladder), short of fully changed: a pest's shove away from her,
            // a dangerous one's at the rim. Not a catch.
            matron_->count_shove();
            shove_player(bodies, matron_->shove_direction(bodies.GetPosition(player_id_)));
        }
    }
    // Run into something (or run into) hard enough (kHitLethalSpeed): a
    // death side-on. Softer, a hit the body feels.
    if (!died_this_tick && !hers && hit_closing_mps_ >= kHitLethalSpeed) {
        leave_corpse(bodies, bodies.GetLinearVelocity(player_id_), hit_closing_mps_, true);
        restore_from_checkpoint(bodies);
        died_this_tick = true;
    } else if (!died_this_tick && hit_closing_mps_ >= kHitFeltSpeed) {
        ++hit_count_;
        hit_speed_mps_ = hit_closing_mps_;
        if (hit_closing_mps_ >= kArmBreakHitSpeed) {
            // The forearm thrown up before the face takes it, on the side the
            // blow came from.
            const JPH::Vec3 right = facing_.Cross(JPH::Vec3::sAxisY());
            break_arm(hit_toward_.Dot(right) >= 0.0F ? 1 : 0);
        }
    }
    if (!died_this_tick) {
        advance_recovery(bodies, delta_seconds);
    }
    if (!died_this_tick && grounded_ && traversal_state_ == TraversalState::None &&
        footing_is_firm(bodies)) {
        // Once she is out the checkpoint is not renewed (it would lie just
        // ahead of her): the one at her release stays, committed the first
        // firm tick after she bursts out.
        if (!matron_->freezes_checkpoint() || matron_checkpoint_pending_) {
            commit_checkpoint(bodies);
            matron_checkpoint_pending_ = false;
        }
        static_assert(Simulation::kRouteTopEntityId == scraperx::sim::vertical::Route::kDerrickStaticFirst + 2,
                      "the route's top is the derrick's upper receiver");
        if (!summit_reached_ && support_entity_id_ == Simulation::kRouteTopEntityId) {
            summit_reached_ = true;
            summit_seconds_ = next_time_seconds;
            summit_deaths_ = death_count_;
        }
    }
    if (grounded_) {
        fall_peak_speed_mps_ = 0.0F;
        parachute_deployed_ = false;
    }

    check_hang_grip(bodies, delta_seconds);
    if (traversal_state_ != TraversalState::None) {
        resolve_traversal_outcome(bodies);
    }

    update_affordance(bodies);
    update_precision_sight(bodies);
    update_up_move(bodies);
    read_state();
}

// The first surface straight below a point within reach, the player's
// own body ignored: where a bird may stand. Read only.
bool Simulation::PhysicsWorld::surface_below(const JPH::RVec3 from, const float reach, JPH::RVec3 &point) const noexcept {
    if (!(reach > 0.0F)) {
        return false;
    }
    JPH::RayCastResult hit;
    const JPH::Vec3 down(0.0F, -reach, 0.0F);
    if (!cast_ray(from, down, hit)) {
        return false;
    }
    point = JPH::RRayCast(from, down).GetPointOnRay(hit.mFraction);
    return true;
}

// The slingshot's predicted path from the pouch now, swept with the
// player's capsule and cut at the first thing it would hit.
std::vector<Vector3> Simulation::PhysicsWorld::slingshot_prediction() const {
    std::vector<Vector3> result;
    if (!slingshot_->state().seated) {
        return result;
    }
    JPH::IgnoreMultipleBodiesFilter filter;
    filter.IgnoreBody(player_id_);
    filter.IgnoreBody(kit_->body_id(slingshot_->pouch_body()));
    filter.IgnoreBody(kit_->body_id(kit_->body_for_entity(Slingshot::kRailEntity)));
    filter.IgnoreBody(kit_->body_id(kit_->body_for_entity(Slingshot::kLaunchRailEntity)));
    filter.IgnoreBody(kit_->body_id(slingshot_->frame_body()));
    const std::vector<JPH::RVec3> points = slingshot_->prediction();
    if (points.empty()) {
        return result;
    }
    const auto to_vector = [](const JPH::RVec3 p) {
        return Vector3{static_cast<double>(p.GetX()), static_cast<double>(p.GetY()),
                       static_cast<double>(p.GetZ())};
    };
    result.push_back(to_vector(points.front()));
    for (std::size_t n = 1; n < points.size(); ++n) {
        const JPH::RVec3 from = points[n - 1];
        const JPH::Vec3 displacement(points[n] - from);
        JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
        const JPH::RShapeCast sweep(player_shape_.GetPtr(), JPH::Vec3::sReplicate(1.0F),
                                    JPH::RMat44::sTranslation(from), displacement);
        physics_system_.GetNarrowPhaseQuery().CastShape(sweep, JPH::ShapeCastSettings(), from,
                                                        collector, {}, {}, filter);
        if (collector.HadHit()) {
            result.push_back(to_vector(from + displacement * collector.mHit.mFraction));
            break;
        }
        result.push_back(to_vector(points[n]));
    }
    return result;
}

void Simulation::PhysicsWorld::launch_kit_body_for_test(const std::uint32_t body, const JPH::RVec3 at,
                                                       const JPH::Vec3 velocity) {
    JPH::BodyInterface &bodies = physics_system_.GetBodyInterface();
    const JPH::BodyID id = kit_->body_id(kit::BodyIndex{body});
    bodies.SetPositionAndRotation(id, at, bodies.GetRotation(id), JPH::EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(id, velocity, JPH::Vec3::sZero());
    impacts_->resync_body(kit_->body_entity(kit::BodyIndex{body}));
}

void Simulation::PhysicsWorld::launch_player_for_test(const JPH::RVec3 at, const JPH::Vec3 velocity) {
    if (grip_ != nullptr) {
        tear_grip(false);   // tests: out of her hand first
    }
    JPH::BodyInterface &bodies = physics_system_.GetBodyInterface();
    release_carry();
    clear_traversal();
    bodies.SetPositionAndRotation(player_id_, at, bodies.GetRotation(player_id_), JPH::EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(player_id_, velocity, JPH::Vec3::sZero());
    grounded_ = false;
}

void Simulation::PhysicsWorld::remove_and_destroy(JPH::BodyInterface &bodies, const JPH::BodyID body_id) {
    // A proving-ground fixture is never built in the game's world.
    if (body_id.IsInvalid()) {
        return;
    }
    bodies.RemoveBody(body_id);
    bodies.DestroyBody(body_id);
}

JPH::Body *Simulation::PhysicsWorld::add_shape_body(JPH::BodyInterface &bodies,
                                                    const JPH::Shape *shape,
                                                    const JPH::RVec3 position,
                                                    const JPH::Quat rotation,
                                                    const JPH::EMotionType motion_type,
                                                    const JPH::ObjectLayer layer,
                                                    const float friction,
                                                    const std::uint64_t entity_id,
                                                    const float mass_kg) {
    JPH::BodyCreationSettings settings(shape, position, rotation, motion_type, layer);
    settings.mFriction = friction;
    settings.mUserData = entity_id;
    settings.mAllowSleeping = false;
    if (mass_kg > 0.0F) {
        settings.mOverrideMassProperties =
            JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = mass_kg;
    }
    JPH::Body *body = bodies.CreateBody(settings);
    const JPH::EActivation activation = motion_type == JPH::EMotionType::Static
                                            ? JPH::EActivation::DontActivate
                                            : JPH::EActivation::Activate;
    bodies.AddBody(body->GetID(), activation);
    return body;
}

JPH::BodyID Simulation::PhysicsWorld::add_box(JPH::BodyInterface &bodies,
                                              const JPH::Vec3 half_extent,
                                              const JPH::RVec3 position,
                                              const JPH::EMotionType motion_type,
                                              const JPH::ObjectLayer layer,
                                              const float friction,
                                              const std::uint64_t entity_id,
                                              const JPH::Quat rotation,
                                              const float mass_kg) {
    return add_shape_body(bodies,
                          new JPH::BoxShape(half_extent),
                          position,
                          rotation,
                          motion_type,
                          layer,
                          friction,
                          entity_id,
                          mass_kg)
        ->GetID();
}

JPH::Vec3 Simulation::PhysicsWorld::normalized_horizontal(const double x, const double z) noexcept {
    const double length = std::hypot(x, z);
    if (!(length > 1.0e-6)) {
        return JPH::Vec3::sZero();
    }
    return JPH::Vec3(static_cast<float>(x / length), 0.0F, static_cast<float>(z / length));
}

JPH::BodyID Simulation::PhysicsWorld::body_id_for_entity(const std::uint64_t entity_id) const noexcept {
    switch (entity_id) {
    case Simulation::kStaticDeckEntityId:
        return deck_id_;
    case Simulation::kTranslatingSupportEntityId:
        return translating_support_id_;
    case Simulation::kRotatingSupportEntityId:
        return rotating_support_id_;
    case Simulation::kVaultRailEntityId:
        return vault_rail_id_;
    case Simulation::kMantleLedgeEntityId:
        return mantle_ledge_id_;
    case Simulation::kHangLedgeEntityId:
        return hang_ledge_id_;
    case Simulation::kMovingLedgeEntityId:
        return moving_ledge_id_;
    case Simulation::kBlockedLedgeEntityId:
        return blocked_ledge_id_;
    case Simulation::kBlockedLedgeCanopyEntityId:
        return blocked_ledge_canopy_id_;
    case Simulation::kTowerEntityId:
        return tower_id_;
    default:
        // A kit body: the rider stands on what it does (plan rule 4), so
        // a deck carried sideways carries them with it.
        if (scraperx::sim::kit::is_kit_entity(entity_id)) {
            const auto body = kit_->body_for_entity(entity_id);
            if (body.valid()) {
                return kit_->body_id(body);
            }
        }
        return {};
    }
}

// Everything the player can see is something the player can hit. The
// table is generated from the Godot builders (godot/presentation/
// solid_export.gd), so the world these tests walk is the world that is
// drawn. Built last, so a drawn mirror of a body already owned here is
// recognized by its bounds and skipped rather than doubled -- a second
// coincident body would split ledge probes and support identity.
void Simulation::PhysicsWorld::build_world_solids(JPH::BodyInterface &bodies) {
    for (const WorldSolidBox &box : kWorldSolidBoxes) {
        build_test_extra_solids(bodies, box.py, false);
        const JPH::Vec3 half(box.hx, box.hy, box.hz);
        const JPH::RVec3 position(box.px, box.py, box.pz);
        const JPH::Quat rotation = JPH::Quat(box.qx, box.qy, box.qz, box.qw).Normalized();
        const JPH::AABox bounds = JPH::AABox(-half, half).Transformed(
            JPH::Mat44::sRotationTranslation(rotation, JPH::Vec3(position)));
        if (mirrors_owned_body(bounds)) {
            ++world_solid_mirrors_;
            continue;
        }
        const float convex_radius = std::min(JPH::cDefaultConvexRadius, half.ReduceMin());
        add_shape_body(bodies, new JPH::BoxShape(half, convex_radius), position, rotation,
                       JPH::EMotionType::Static, object_layers::kStatic, kWorldSolidFriction,
                       Simulation::kWorldSolidEntityId, 0.0F);
        ++world_solid_bodies_;
    }
    build_test_extra_solids(bodies, std::numeric_limits<double>::quiet_NaN(), false);
    JPH::Array<JPH::Vec3> points;
    for (const WorldSolidHull &hull : kWorldSolidHulls) {
        JPH::DVec3 sum = JPH::DVec3::sZero();
        for (std::uint32_t index = 0; index < hull.count; ++index) {
            const float *point = &kWorldSolidHullPoints[(hull.first + index) * 3U];
            sum += JPH::DVec3(point[0], point[1], point[2]);
        }
        const JPH::DVec3 centre = sum / static_cast<double>(std::max<std::uint32_t>(hull.count, 1U));
        points.clear();
        for (std::uint32_t index = 0; index < hull.count; ++index) {
            const float *point = &kWorldSolidHullPoints[(hull.first + index) * 3U];
            points.push_back(JPH::Vec3(static_cast<float>(point[0] - centre.GetX()),
                                       static_cast<float>(point[1] - centre.GetY()),
                                       static_cast<float>(point[2] - centre.GetZ())));
        }
        const JPH::ConvexHullShapeSettings settings(points.data(), static_cast<int>(points.size()));
        const JPH::ShapeSettings::ShapeResult result = settings.Create();
        if (result.HasError()) {
            ++world_solid_rejected_;
            continue;
        }
        add_shape_body(bodies, result.Get().GetPtr(),
                       JPH::RVec3(centre.GetX(), centre.GetY(), centre.GetZ()),
                       JPH::Quat::sIdentity(), JPH::EMotionType::Static, object_layers::kStatic,
                       kWorldSolidFriction, Simulation::kWorldSolidEntityId, 0.0F);
        ++world_solid_bodies_;
    }
}

// Tests only (Simulation::TestExtraSolid). A box is built once: the first
// time the table reaches a box of its 22 m band, else at the table's end;
// an at_end box after every body the world builds. Counted as a world solid.
void Simulation::PhysicsWorld::build_test_extra_solids(JPH::BodyInterface &bodies, const double band_y,
                                                       const bool at_end) {
    constexpr double kBandM = 22.0;
    for (std::size_t index = 0; index < test_extra_solids_.size(); ++index) {
        const Simulation::TestExtraSolid &extra = test_extra_solids_[index];
        if (extra.at_end != at_end || test_extra_built_.count(index) != 0) {
            continue;
        }
        const bool here = at_end || std::isnan(band_y) || (extra.y <= band_y && band_y < extra.y + kBandM);
        if (!here) {
            continue;
        }
        const JPH::Vec3 half(extra.half_x, extra.half_y, extra.half_z);
        const float convex_radius = std::min(JPH::cDefaultConvexRadius, half.ReduceMin());
        add_shape_body(bodies, new JPH::BoxShape(half, convex_radius), JPH::RVec3(extra.x, extra.y, extra.z),
                       JPH::Quat::sIdentity(), JPH::EMotionType::Static, object_layers::kStatic,
                       kWorldSolidFriction, Simulation::kWorldSolidEntityId, 0.0F);
        ++world_solid_bodies_;
        test_extra_built_.insert(index);
    }
}

bool Simulation::PhysicsWorld::mirrors_owned_body(const JPH::AABox &bounds) const {
    JPH::AllHitCollisionCollector<JPH::CollideShapeBodyCollector> collector;
    physics_system_.GetBroadPhaseQuery().CollideAABox(bounds, collector);
    for (const JPH::BodyID id : collector.mHits) {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), id);
        if (!lock.Succeeded() ||
            lock.GetBody().GetUserData() == Simulation::kWorldSolidEntityId) {
            continue;
        }
        const JPH::AABox owned = lock.GetBody().GetWorldSpaceBounds();
        if ((owned.mMin - bounds.mMin).Abs().ReduceMax() < kWorldSolidMirrorTolerance &&
            (owned.mMax - bounds.mMax).Abs().ReduceMax() < kWorldSolidMirrorTolerance) {
            return true;
        }
    }
    return false;
}

// The stack: the tower's lower section, as real static collision. A
// perimeter deck ring per level round an open 34 m shaft, and corner and
// mid-span columns carrying each deck. There is no stair and no ramp: a
// level is gained by a machine or a climb.
void Simulation::PhysicsWorld::build_stack(JPH::BodyInterface &bodies) {
    const auto frame = [&](const JPH::Vec3 half_extent, const JPH::RVec3 position) {
        machine_bodies_.push_back(add_box(bodies, half_extent, position,
                                          JPH::EMotionType::Static, object_layers::kStatic,
                                          0.85F, Simulation::kTowerEntityId));
    };

    const float band_center = kStackHalfExtent - kStackDeckBandDepth * 0.5F;
    const float inner_half = kStackHalfExtent - kStackDeckBandDepth;
    for (int level = 1; level <= kStackLevelCount; ++level) {
        const float slab_y =
            static_cast<float>(level) * kStackLevelHeight - kStackDeckHalfThickness;
        // Deck ring: two full-width bands and two inner bands.
        for (const float side : {1.0F, -1.0F}) {
            if (level == kStackLevelCount && side > 0.0F) {
                // The 154 m deck's north band is cut by the bay (bay.hpp):
                // its parts either side of it.
                const float west = kStackCenterX - kStackHalfExtent;
                const float east = kStackCenterX + kStackHalfExtent;
                const float west_half = 0.5F * (Bay::kWestX - west);
                const float east_half = 0.5F * (east - Bay::kEastX);
                frame(JPH::Vec3(west_half, kStackDeckHalfThickness, kStackDeckBandDepth * 0.5F),
                      JPH::RVec3(west + west_half, slab_y, kStackCenterZ + side * band_center));
                frame(JPH::Vec3(east_half, kStackDeckHalfThickness, kStackDeckBandDepth * 0.5F),
                      JPH::RVec3(Bay::kEastX + east_half, slab_y, kStackCenterZ + side * band_center));
                continue;
            }
            frame(JPH::Vec3(kStackHalfExtent, kStackDeckHalfThickness,
                            kStackDeckBandDepth * 0.5F),
                  JPH::RVec3(kStackCenterX, slab_y, kStackCenterZ + side * band_center));
        }
        for (const float side : {1.0F, -1.0F}) {
            if (level == 7 && side > 0.0F) {
                // Level 7 East band has an equipment hatch / ladder well for C2 from Z = -148.70F to -145.00F:
                const float s_min_z = -145.00F;
                const float s_max_z = -133.00F;
                const float s_half_z = 0.5F * (s_max_z - s_min_z);
                const float s_center_z = 0.5F * (s_min_z + s_max_z);
                frame(JPH::Vec3(kStackDeckBandDepth * 0.5F, kStackDeckHalfThickness, s_half_z),
                      JPH::RVec3(kStackCenterX + side * band_center, slab_y, s_center_z));

                const float n_min_z = -167.00F;
                const float n_max_z = -148.70F;
                const float n_half_z = 0.5F * (n_max_z - n_min_z);
                const float n_center_z = 0.5F * (n_min_z + n_max_z);
                frame(JPH::Vec3(kStackDeckBandDepth * 0.5F, kStackDeckHalfThickness, n_half_z),
                      JPH::RVec3(kStackCenterX + side * band_center, slab_y, n_center_z));
            } else {
                frame(JPH::Vec3(kStackDeckBandDepth * 0.5F, kStackDeckHalfThickness, inner_half),
                      JPH::RVec3(kStackCenterX + side * band_center, slab_y, kStackCenterZ));
            }
        }
    }

    // Columns: corners and edge mid-spans, one run per story.
    for (int level = 0; level < kStackLevelCount; ++level) {
        const float base_y = static_cast<float>(level) * kStackLevelHeight;
        const float column_half = kStackLevelHeight * 0.5F;
        for (const float sx : {1.0F, -1.0F}) {
            for (const float sz : {1.0F, -1.0F}) {
                frame(JPH::Vec3(kStackColumnHalf, column_half, kStackColumnHalf),
                      JPH::RVec3(kStackCenterX + sx * kStackHalfExtent, base_y + column_half,
                                 kStackCenterZ + sz * kStackHalfExtent));
            }
            frame(JPH::Vec3(kStackColumnHalf, column_half, kStackColumnHalf),
                  JPH::RVec3(kStackCenterX + sx * kStackHalfExtent, base_y + column_half,
                             kStackCenterZ));
            frame(JPH::Vec3(kStackColumnHalf, column_half, kStackColumnHalf),
                  JPH::RVec3(kStackCenterX, base_y + column_half,
                             kStackCenterZ + sx * kStackHalfExtent));
        }
    }
}

// track_for_teardown=false hands ownership entirely to the caller (the
// carry, created and removed at runtime): Jolt's ConstraintManager::Remove
// asserts on an already-
// invalidated constraint index, so a constraint that might be removed
// before the destructor runs must never also sit in machine_constraints_,
// which is unconditionally removed there once. Exactly one owner removes
// it, on every path.
JPH::TwoBodyConstraint *Simulation::PhysicsWorld::create_constraint(
    const JPH::TwoBodyConstraintSettings &settings,
    const JPH::BodyID first,
    const JPH::BodyID second,
    const bool track_for_teardown) {
    // Jolt stripes body mutexes across a fixed-size array, so two distinct
    // bodies can share one. Taking two separate BodyLockWrite locks then
    // deadlocks on a non-recursive mutex ("Resource deadlock avoided").
    // BodyLockMultiWrite sorts and dedupes the mutexes, which is exactly
    // what it exists for. Every constraint pair built before this simply
    // happened not to collide.
    const JPH::BodyID ids[2] = {first, second};
    JPH::BodyLockMultiWrite lock(physics_system_.GetBodyLockInterface(), ids, 2);
    JPH::Body *first_body = lock.GetBody(0);
    JPH::Body *second_body = lock.GetBody(1);
    if (first_body == nullptr || second_body == nullptr) {
        return nullptr;
    }
    JPH::TwoBodyConstraint *constraint = settings.Create(*first_body, *second_body);
    if (constraint == nullptr) {
        return nullptr;
    }
    physics_system_.AddConstraint(constraint);
    if (track_for_teardown) {
        machine_constraints_.emplace_back(constraint);
    }
    return constraint;
}

JPH::Vec3 Simulation::PhysicsWorld::current_support_point_velocity(
    const JPH::BodyInterface &bodies) const noexcept {
    const JPH::BodyID support_id = body_id_for_entity(support_entity_id_);
    if (support_id.IsInvalid()) {
        return JPH::Vec3::sZero();
    }
    return bodies.GetPointVelocity(
        support_id,
        JPH::RVec3(support_sample_.contact_point.x,
                   support_sample_.contact_point.y,
                   support_sample_.contact_point.z));
}

void Simulation::PhysicsWorld::update_support_motion(JPH::BodyInterface &bodies,
                                                     const float delta_seconds,
                                                     const double next_time_seconds) noexcept {
    const double translating_x =
        kTranslatingSupportAmplitudeMeters *
        std::sin(kTranslatingSupportAngularFrequency * next_time_seconds);
    bodies.MoveKinematic(translating_support_id_,
                         JPH::RVec3(translating_x, 0.25, 8.0),
                         JPH::Quat::sIdentity(),
                         delta_seconds);

    rotating_support_yaw_radians_ =
        std::fmod(kRotatingSupportAngularSpeed * next_time_seconds, 2.0 * kPi);
    bodies.MoveKinematic(
        rotating_support_id_,
        JPH::RVec3(-8.0, 0.25, 0.0),
        JPH::Quat::sRotation(JPH::Vec3(0.0F, 1.0F, 0.0F),
                             static_cast<float>(rotating_support_yaw_radians_)),
        delta_seconds);

    const double moving_ledge_z =
        kMovingLedgeCenterZ +
        kMovingLedgeAmplitudeMeters *
            std::sin(kMovingLedgeAngularFrequency * next_time_seconds);
    bodies.MoveKinematic(moving_ledge_id_,
                         JPH::RVec3(9.0, 1.8, moving_ledge_z),
                         JPH::Quat::sIdentity(),
                         delta_seconds);

}

// A death leaves the body where it died: its corpse at the body's place,
// moving at `velocity`, and the place and speed of the death for the
// presentation (the view rides the corpse; the blood goes where it hit).
void Simulation::PhysicsWorld::leave_corpse(JPH::BodyInterface &bodies, const JPH::Vec3 velocity, const float impact_mps,
                                            const bool side_on) noexcept {
    const JPH::RVec3 at = bodies.GetPosition(player_id_);
    const JPH::RVec3 soles = at - JPH::Vec3(0.0F, active_half_height(), 0.0F);
    corpses_->spawn(soles, facing_, velocity);
    death_point_ = support_sample_.grounded
                       ? JPH::RVec3(support_sample_.contact_point.x, support_sample_.contact_point.y,
                                    support_sample_.contact_point.z)
                       : soles;
    death_speed_mps_ = impact_mps;
    death_side_on_ = side_on;
}

// Her hands on the player while they cannot take hold, short of fully
// changed: thrown off the feet along `direction` (Matron::kShoveMps, and
// kShoveUpMps up), a stumble to recover from, and a hit the body feels, on the
// side she came from.
void Simulation::PhysicsWorld::shove_player(JPH::BodyInterface &bodies, const JPH::Vec3 direction) noexcept {
    const JPH::Vec3 velocity = direction * Matron::kShoveMps + JPH::Vec3(0.0F, Matron::kShoveUpMps, 0.0F);
    bodies.SetLinearVelocity(player_id_, velocity);
    ++stumble_count_;
    landing_response_ = LandingResponse::Stumble;
    landing_loss_ = 0.6 * kLandingMaxLoss;
    recovery_duration_ = recovery_seconds_left_ = 1.0;
    ++hit_count_;
    hit_speed_mps_ = Matron::kShoveMps;
    hit_toward_ = -direction;
}

} // namespace scraperx::sim

namespace scraperx::sim {
bool Simulation::PhysicsWorld::place_matron_home(const float deck_top) { return matron_->set_home(deck_top); }

void Simulation::PhysicsWorld::launch_matron_for_test(JPH::RVec3 at) {
    Matron::Saved saved; saved.state = Matron::State::Hunting;
    matron_->restore(saved);
    physics_system_.GetBodyInterface().SetPosition(matron_->body(), at, JPH::EActivation::Activate);
}

const traversal::AffordanceMap &Simulation::PhysicsWorld::matron_affordances() {
    if (!matron_affordances_scanned_) {
        // The whole tower, grade to the crown (bounds generous on every side).
        const JPH::AABox tower = physics_system_.GetBounds();   // the whole world, whatever is in it
        matron_affordances_.scan(physics_system_, tower, {matron_->body(), player_id_, matron_->moving_layer()});
        matron_affordances_scanned_ = true;
        matron_->set_routes(&matron_affordances_);
    }
    return matron_affordances_;
}

std::size_t Simulation::PhysicsWorld::matron_grid_walk(const JPH::Vec3 from, const JPH::Vec3 to, const float margin,
                                                       const int max_nodes) const {
    if (max_nodes < 0) {
        // A probe: 0 no floor within a step, 1 floor but her capsule does not fit, 2 she stands there.
        const traversal::Limits limits{};
        const traversal::Ignore ignore{matron_->body(), player_id_, matron_->moving_layer()};
        JPH::RayCastResult hit;
        const JPH::RVec3 top(from.GetX(), from.GetY() + limits.step_max_m + 0.05F, from.GetZ());
        const float depth = 2.0F * limits.step_max_m + 0.1F;
        const JPH::IgnoreMultipleBodiesFilter none;
        if (!physics_system_.GetNarrowPhaseQuery().CastRay(JPH::RRayCast(top, JPH::Vec3(0, -depth, 0)), hit, {}, {}, none)) return 0;
        const float feet = float(top.GetY()) - hit.mFraction * depth;
        const auto p = traversal::grid_walk(physics_system_, ignore, limits, JPH::Vec3(from.GetX(), feet, from.GetZ()),
                                            JPH::Vec3(from.GetX() + 0.4F, feet, from.GetZ()), 0.0F, 0.4F, 2);
        (void)margin;
        (void)to;
        return p.empty() ? 1 : 2;
    }
    return traversal::grid_walk(physics_system_, traversal::Ignore{matron_->body(), player_id_, matron_->moving_layer()}, traversal::Limits{}, from,
                                to, margin, 0.4F, max_nodes)
        .size();
}

double Simulation::PhysicsWorld::matron_gravity_factor() const {
    return static_cast<double>(physics_system_.GetBodyInterface().GetGravityFactor(matron_->body()));
}
} // namespace scraperx::sim
