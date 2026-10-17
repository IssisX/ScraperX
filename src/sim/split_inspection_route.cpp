#include "sim/split_inspection_route.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace scraperx::sim {
namespace {
using kit::Material;
using kit::Part;
using JPH::Vec3;

// CHOSEN steel density7850kg/m3, closed structural sections with6mm walls.
// Plates instead use their actual solid volume. Native Kit derives COM/inertia
// from these constituent masses; collision/render share the same envelopes.
void set_steel_mass(Part &part, const bool plate) {
    const auto size = 2.0F * part.half;
    const float volume = size.GetX() * size.GetY() * size.GetZ();
    const float inside = plate ? 0.0F :
        std::max(0.0F, size.GetX() - .012F) *
        std::max(0.0F, size.GetY() - .012F) *
        std::max(0.0F, size.GetZ() - .012F);
    part.mass_kg = 7850.0F * (volume - inside);
    part.convex_radius = 0.0F;
}

void box(std::vector<Part> &parts, const Vec3 low, const Vec3 high,
         const bool plate = false, const Material material = Material::Steel) {
    Part part{(high - low) * .5F, (high + low) * .5F,
              JPH::Quat::sIdentity(), material};
    set_steel_mass(part, plate);
    parts.push_back(part);
}

void beam(std::vector<Part> &parts, const Vec3 first, const Vec3 last,
          const float half_section) {
    const auto delta = last - first;
    Part part{{half_section, half_section, delta.Length() * .5F},
              (first + last) * .5F,
              JPH::Quat::sFromTo(Vec3::sAxisZ(), delta.Normalized()),
              Material::Steel};
    set_steel_mass(part, false);
    parts.push_back(part);
}

kit::BodyIndex body(kit::Kit &kit, const std::uint64_t entity,
                    std::vector<Part> parts, const Vec3 origin) {
    float mass = 0.0F;
    for (auto &part : parts) {
        mass += part.mass_kg;
        part.offset -= origin;
    }
    const auto result = kit.add_body(entity, parts, JPH::RVec3(origin),
                                    JPH::Quat::sIdentity(), mass, .8F);
    kit.set_damping(result, 0.0F, 0.0F);
    return result;
}

void mount(kit::Kit &kit, const kit::BodyIndex structure,
           const JPH::RVec3 point) {
    // CHOSEN facade precedent:2MN/m,40kNs/m;2MNm/rad,40kNms/rad.
    // Passive fixed-neutral attachment, with no changing target or motor work.
    kit.add_elastic_mount({}, structure, point, 2.0e6F, 40000.0F,
                          2.0e6F, 40000.0F);
}
} // namespace

void build_split_inspection_route(kit::Kit &kit) {
    std::vector<Part> crosshead;
    // Notch around the actual continuing column, whose southeast corner is
    // (-20.349,-129.651). Its existing solid must not penetrate this plate.
    box(crosshead, {-20.349F, 473.234F, -130.2F},
                  {-18.0F, 473.25F, -129.3F}, true, Material::Galvanised);
    box(crosshead, {-21.5F, 473.234F, -130.2F},
                  {-20.349F, 473.25F, -129.651F}, true, Material::Galvanised);
    // The inbound north edge is cut back toZ=-129.3. The real brace at
    // Z=-128.55 stays open overhead until the player earns a supported
    // southward Jump nearX=-19.3; no bearer catches the capsule prematurely.
    box(crosshead, {-20.79F, 472.85F, -130.08F},
                  {-18.0F, 473.234F, -129.88F});
    beam(crosshead, {-20.67F, 471.8F, -129.05F},
                   {-20.25F, 472.98F, -129.98F}, .12F);
    box(crosshead, {-20.79F, 471.65F, -129.2F},
                  {-20.75F, 471.95F, -128.9F}, true);
    beam(crosshead, {-20.67F, 472.8F, -129.05F},
                   {-20.25F, 472.98F, -129.98F}, .12F);
    box(crosshead, {-20.79F, 472.65F, -129.2F},
                  {-20.75F, 472.95F, -128.9F}, true);
    const auto crosshead_body = body(kit, kSplitInspectionCrossheadEntity,
                                     crosshead, {-19.75F, 473.0F, -129.2F});
    mount(kit, crosshead_body, {-20.79, 472.8, -129.05});
    mount(kit, crosshead_body, {-20.79, 471.8, -129.05});

    std::vector<Part> receiver;
    // Only the hinge-side exit strip is fixed. Its east end clears the
    // rotating truss nose. The front hatch remains the actual walking
    // transfer onto the girder; no floor is added across that moving seam.
    box(receiver, {-20.7F, 473.234F, -134.6F},
                  {-18.9F, 473.25F, -134.3F}, true, Material::Galvanised);
    for (const float x : {-20.50F, -18.20F}) {
        box(receiver, {x - .12F, 472.85F, -134.6F},
                      {x + .12F, 473.234F, -132.3F});
        // Two finite posts and baseplates on the real462.25m west roof.
        // The intentional short-jump lane atX=-19.3 remains open below.
        box(receiver, {x - .12F, 462.274F, -134.42F},
                      {x + .12F, 472.85F, -134.18F});
        box(receiver, {x - .22F, 462.25F, -134.52F},
                      {x + .22F, 462.274F, -134.08F}, true);
        beam(receiver, {x, 470.5F, -134.3F},
                       {x, 472.98F, -132.5F}, .08F);
    }
    box(receiver, {-20.5F, 472.85F, -134.42F},
                  {-18.9F, 473.234F, -134.18F});
    // A finite central waiting pad clears the moving leaf's footprint and
    // both rear ribs. It cantilevers400mm from the existing fixed strip;
    // the real16mm plate carries the rider back into that receiver.
    box(receiver,{-19.75F,473.234F,-135.0F},{-18.95F,473.25F,-134.3F},true,Material::Galvanised);
    // The narrow hinge strip can move slightly on its compliant supports.
    // A shallow storage tray retains the loose ballast by real side contact;
    // its top remains exposed for the existing ordinary pickup/lift owner.
    // CHOSEN15mm walls,100mm tall,340x280mm outer footprint.
    for(const float x:{-20.22F,-19.895F})
        box(receiver,{x,473.25F,-134.59F},{x+.015F,473.35F,-134.31F},true,Material::Yellow);
    for(const float z:{-134.59F,-134.325F})
        box(receiver,{-20.205F,473.25F,z},{-19.895F,473.35F,z+.015F},true,Material::Yellow);
    const auto receiver_body = body(kit, kSplitInspectionReceiverEntity,
                                    receiver, {-19.35F, 473.0F, -133.45F});
    kit.set_accurate_integration(receiver_body);
    for (const float x : {-20.5F, -18.2F})
        mount(kit, receiver_body, {x, 462.25, -134.3});

    // Optional removable ballast on the fixed hinge strip. The25kg block
    // supplies the coupled hatch/span load through the actual cable levers;
    // its useful pocket range must cover that load as well as hinge friction.
    // Lifting it off restores the ordinary self-closing floor.
    // DERIVED solid steel envelope:160mm square, height m/(rho*w*w).
    const float block_half_y = kSplitInspectionBalanceBlockMassKg / (7850.0F*.16F*.16F*2);
    Part balance_block{{.08F,block_half_y,.08F},Vec3::sZero(),
                       JPH::Quat::sIdentity(),Material::Yellow};
    balance_block.mass_kg=kSplitInspectionBalanceBlockMassKg;
    balance_block.convex_radius=0;
    const auto block_body=kit.add_body(kSplitInspectionBalanceBlockEntity,{balance_block},
        JPH::RVec3(-20.05,473.25+block_half_y,-134.45),JPH::Quat::sIdentity(),
        kSplitInspectionBalanceBlockMassKg,.8F);
    kit.set_carry(block_body,kit::CarryKind::Load,Vec3(0,block_half_y,0));
    kit.set_damping(block_body,0,0);
    kit.set_accurate_integration(block_body);

    std::vector<Part> leaf;
    // CHOSEN6mm reinforced steel hatch skin; deep ribs carry landing load.
    box(leaf, {-20.7F, 473.244F, -134.3F},
              {-18.0F, 473.25F, -132.3F}, true, Material::Galvanised);
    for (const float x : {-20.50F, -18.20F}) {
        box(leaf, {x - .06F, 473.0F, -135.3F},
                  {x + .06F, 473.244F, -132.3F});
    }
    // Short physical recovery holds at the west rear corner only. Their load
    // enters the moving leaf; the front landing and girder remain exposed.
    for (const float x : {-20.65F}) {
        beam(leaf, {x, 473.20F, -134.25F},
                   {x, 474.05F, -134.25F}, .025F);
        beam(leaf, {x, 474.05F, -134.25F},
                   {x, 474.05F, -133.85F}, .025F);
    }
    // The open-backed pocket receives a freely falling, rotating block,
    // not just its gravity-projected center. The carried load drops about
    // 1.2m and can reach the front rim at4m/s: a150mm lip only catches its
    // bottom corner. A350mm wall intercepts the actual side before it can
    // tip out. The900mm width keeps the side walls outside the ordinary
    // standing capsule while allowing lateral drop drift. Pickup stays open
    // above and behind; all added steel enters the counterbalance below.
    box(leaf,{-19.80F,473.25F,-132.315F},{-18.90F,473.60F,-132.30F},true,Material::Yellow);
    for(float x:{-19.80F,-18.915F})
        box(leaf,{x,473.25F,-132.90F},{x+.015F,473.60F,-132.30F},true,Material::Yellow);
    // A real steel lug joins the raised counterweight to its cable eye.
    // CHOSEN1m rear lever gives stroke margin for receiver compliance; the
    // same mass/moment closure includes every gram of this attachment.
    beam(leaf,{-19.35F,473.15F,-135.2F},
         Vec3(kSplitInspectionHatchPivot)+kSplitInspectionHatchCableLocal,.03F);
    // DERIVED counterweight from the authored leaf mass moments. At the
    // least favorable accepted pocket lever1.54m and10deg pitch, the25kg
    // block supplies >=371.95Nm. Actual cable levers~1.044/2.102m add
    // ~69.54Nm for the110Nm span bias+30Nm bearing loss; hatch loss is20Nm.
    // A declared20% source-torque reserve permits <=21.53kg*m restoration.
    // CHOSEN21kg*m changes real rear steel, not an actuator/solver force.
    // The previous35kg*m covered one forward placement but not this range.
    // An85kg rider at1.6m still supplies136kg*m forward.
    // At the hinge-side exit (0.15m lever), the rider contributes12.75kg*m:
    // stepping back releases the pitch and gravity returns the floor.
    float forward_mass_moment = 0.0F;
    float vertical_mass_moment = 0.0F;
    for (const auto &part : leaf) {
        forward_mass_moment += part.mass_kg * (part.offset.GetZ() + 134.3F);
        vertical_mass_moment += part.mass_kg * (part.offset.GetY() - 473.2F);
    }
    constexpr float weight_lever = .9F;
    constexpr float restoring_mass_moment = 21.0F; // kg*m, coupled load bound above
    const float weight_mass = (forward_mass_moment + restoring_mass_moment) / weight_lever;
    const float weight_depth = weight_mass / (7850.0F * 2.7F * .2F);
    // Close the vertical first moment as well. A low rear weight previously
    // added144kg*m of pitch-dependent restoration, overwhelming the block
    // before the10deg stop despite the correct neutral horizontal balance.
    const float weight_y = 473.2F - vertical_mass_moment / weight_mass;
    box(leaf, {-20.7F, weight_y-.1F, -135.2F - weight_depth * .5F},
              {-18.0F, weight_y+.1F, -135.2F + weight_depth * .5F},
              true, Material::Yellow);
    const auto leaf_body = body(kit, kSplitInspectionHatchEntity,
                                leaf, {-19.35F, 473.2F, -134.3F});
    kit.set_accurate_integration(leaf_body);
    // CHOSEN20Nm dry hinge friction: finite passive dissipation, no motor.
    (void)kit.add_hinge(receiver_body, leaf_body, kSplitInspectionHatchPivot,
                       Vec3::sAxisX(), 20.0F);

    std::vector<Part> stops;
    // CHOSEN10deg travel. The fixed stop's FRONT edge is1.9m from
    // the hinge in worldZ. Intersect its vertical line with the tilted rib
    // underside plane; a local1.8m-point estimate stopped the leaf early.
    constexpr float open_angle=JPH::JPH_PI/18;
    const float front_stop_top=473.2F-(.2F+1.9F*std::sin(open_angle))/std::cos(open_angle);
    for (const float x : {-20.50F, -18.20F}) {
        // Rear arm rests at zero pitch; positive pitch lifts away from it.
        box(stops, {x - .10F, 472.75F, -134.70F},
                   {x + .10F, 473.0F, -134.50F});
        // DERIVED at the actual fixed front edgeZ=-132.4, not an
        // arbitrary local rib point: front_stop_top≈472.66189m.
        box(stops, {x - .10F, 472.44F, -132.60F},
                   {x + .10F, front_stop_top, -132.40F});
        box(stops, {x - .10F, 472.44F, -134.60F},
                   {x + .10F, 472.60F, -132.40F});
    }
    const auto stops_body = body(kit, kSplitInspectionStopsEntity,
                                 stops, {-19.35F, 472.6F, -133.5F});
    // Hinge filters ONLY pedestal/leaf. Independent welded stops retain
    // leaf collision, so both pitch limits are actual solid reactions.
    kit.add_fixed_joint(receiver_body, stops_body);

    std::vector<Part> truss;
    const auto along = kSplitInspectionGirderExit - kSplitInspectionGirderToe;
    const auto rotation = JPH::Quat::sRotation(Vec3::sAxisZ(),
        std::atan2(along.GetY(), along.GetX()));
    // Endpoints describe the walking TOP. Subtract rotated half-depth to
    // keep the full.55m face on that exact473.25→484.25m line.
    // A600mm solid steel wedge preserves the walking top and tapers the
    // underside into the leaf seam. A blunt300mm box end otherwise lands
    // on the hatch before the span can reach its own lower seat.
    constexpr float nose_length=.6F;
    const auto tangent=along.Normalized();
    const auto normal=rotation*Vec3::sAxisY();
    const auto nose_end=kSplitInspectionGirderToe+tangent*nose_length;
    Part nose;
    nose.shape=Part::Shape::ConvexHull;
    nose.material=Material::Rust;
    // Centre the actual solid triangular prism at its analytical centroid;
    // part mass moments and cooked hull COM therefore describe the same steel.
    nose.offset=kSplitInspectionGirderToe+tangent*(2*nose_length/3)-normal*.1F;
    nose.mass_kg=7850.F*.5F*nose_length*.3F*kSplitInspectionGirderWidth;
    nose.convex_radius=0;
    for(float side:{-.5F,.5F}) {
        const auto lateral=Vec3(0,0,side*kSplitInspectionGirderWidth);
        for(const auto point:{kSplitInspectionGirderToe,nose_end,nose_end-normal*.3F})
            nose.points.push_back(point+lateral-nose.offset);
    }
    truss.push_back(nose);
    Part top{{(along.Length()-nose_length) * .5F, .15F,
              kSplitInspectionGirderWidth * .5F},
             (nose_end + kSplitInspectionGirderExit) * .5F -
                 rotation * Vec3(0, .15F, 0),
             rotation, Material::Rust};
    set_steel_mass(top, false);
    truss.push_back(top);
    // CHOSEN1.2m-deep welded truss, two lower chords and ten web bays.
    // All web steel remains below the narrow walking face; no gap bridge,
    // elevated side rail or wider alternate walking strip is introduced.
    for (const float side : {-1.0F, 1.0F}) {
        const Vec3 lower_offset(0, -1.2F,
                                side * (kSplitInspectionGirderWidth * .5F - .08F));
        // A1.5m closed-box cantilever nose clears the fixed receiver while
        // lowering. Deep chords/webs begin beyond its swept support envelope;
        // the real550x300mm6mm-wall walking member carries the short tip.
        const Vec3 deep_start=kSplitInspectionGirderToe+along.Normalized()*1.5F;
        beam(truss, deep_start + lower_offset,
                    kSplitInspectionGirderExit + lower_offset, .08F);
        for (unsigned bay = 0; bay < 10; ++bay) {
            const Vec3 first = bay==0 ? deep_start : kSplitInspectionGirderToe +
                               along * (static_cast<float>(bay) / 10.0F);
            const Vec3 last = kSplitInspectionGirderToe +
                              along * (static_cast<float>(bay + 1) / 10.0F);
            const Vec3 upper_offset(0, -.24F, lower_offset.GetZ());
            beam(truss, first + upper_offset, first + lower_offset, .05F);
            beam(truss, first + lower_offset, last + upper_offset, .05F);
        }
        beam(truss, kSplitInspectionGirderExit + Vec3(0, -.24F, lower_offset.GetZ()),
                    kSplitInspectionGirderExit + lower_offset, .05F);
    }
    // A maintenance conduit crosses the exposed inspection girder. Readable
    // from the receiver, its actual 1.65m clearance requires compact posture;
    // the narrow footing makes stance/timing matter instead of a long walk.
    const float conduit_foot = kSplitInspectionGirderToe.GetY() +
        (kSplitInspectionDuckX-kSplitInspectionGirderToe.GetX()) *
        along.GetY()/along.GetX();
    const float conduit_y = conduit_foot+kSplitInspectionDuckClearance+.08F;
    beam(truss, {kSplitInspectionDuckX, conduit_y, -134.60F},
                {kSplitInspectionDuckX, conduit_y, -133.70F}, .08F);
    // The side uprights carry the conduit back into the real truss, and are
    // reachable physical holds for an input-driven recovery; no automatic catch.
    for (const float z : {-134.60F, -133.70F}) {
        beam(truss, {kSplitInspectionDuckX, conduit_foot-.60F, z},
                    {kSplitInspectionDuckX, conduit_y, z}, .06F);
        beam(truss, {kSplitInspectionDuckX, conduit_foot-.60F, z},
                    {kSplitInspectionDuckX, conduit_foot-.60F,
                     kSplitInspectionGirderZ}, .06F);
    }
    // The upper receiving shoe stays fixed to the existing roof. Its original
    // under-slab bracket cannot rotate with the span without entering concrete.
    std::vector<Part> bearing;
    box(bearing, {9.45F,484.234F,-134.45F},
                 {10.55F,484.25F,-133.46F},true,Material::Galvanised);
    box(bearing, {9.45F,483.59F,-134.45F},
                 {10.55F,483.75F,-132.65F});
    for(const float x:{9.57F,10.43F})
        box(bearing,{x-.08F,483.75F,-134.35F},{x+.08F,484.234F,-133.54F});
    for(const float z:{-134.75F,-133.60F})
        box(bearing,{9.72F,483.75F,z-.10F},{10.28F,484.35F,z+.10F});
    Part axle{{.12F,.675F,.12F},Vec3(kSplitInspectionSpanPivot),
        JPH::Quat::sRotation(Vec3::sAxisX(),JPH::JPH_PI*.5F),Material::Steel};
    axle.shape=Part::Shape::Cylinder;axle.mass_kg=7850*JPH::JPH_PI*.12F*.12F*1.35F;
    axle.convex_radius=0;bearing.push_back(axle);
    // These fixed bodies are visible extensions of the permitted static
    // backbone. They react loads without supplying work or moving a target.
    const auto fixed_body=[&](std::uint64_t entity,std::vector<Part> parts){
        return kit.add_body(entity,parts,JPH::RVec3::sZero(),JPH::Quat::sIdentity(),0,.8F);
    };
    const auto bearing_body=fixed_body(kSplitInspectionBearingEntity,bearing);

    // A rigid side lug takes the line load into the existing truss. Both its
    // mass and its off-axis inertia participate in the compound balance.
    beam(truss,{7.9F,483.225F,-134.15F},kSplitInspectionSpanCablePoint,.06F);
    const Vec3 pivot(kSplitInspectionSpanPivot);
    const auto moments=[&](const std::vector<Part>&parts){
        Vec3 result=Vec3::sZero();
        for(const auto&part:parts)result+=part.mass_kg*(part.offset-pivot);
        return result;
    };
    const auto original_moment=moments(truss);
    // DERIVED opposite arm slope preserves the complete XY moment direction,
    // including webs/conduit/lug, instead of balancing only horizontal COM.
    const Vec3 tail(16,pivot.GetY()+6*original_moment.GetY()/original_moment.GetX(),pivot.GetZ());
    // Keep the low pivot dogleg west of the existing grapple departure
    // (X>=10.8), then rise above its standing capsule, not merely its deck.
    // At the lowest raised pose the upper arm underside over the bay clears
    // standing top486.05 by at least0.4m. Every real segment enters the
    // complete mass/XY-moment closure below; no hidden counterbalance force.
    for(float side:{-.30F,.30F}) {
        const Vec3 knee(10.4F,484.7F,pivot.GetZ()+side);
        const Vec3 high_knee(10.4F,487.0F,pivot.GetZ()+side);
        beam(truss,pivot+Vec3(0,0,side),knee,.09F);
        beam(truss,knee,high_knee,.09F);
        beam(truss,high_knee,tail+Vec3(0,0,side),.09F);
    }
    beam(truss,tail+Vec3(0,0,-.55F),tail+Vec3(0,0,.55F),.09F);
    const auto unbalanced=moments(truss);
    // CHOSEN110Nm unloaded raising bias,30Nm passive bearing loss. Vertical
    // first moment is zero: large opposing masses cannot hide a costly rise.
    const float counter_mass=(110.F/9.81F-unbalanced.GetX())/6.F;
    const Vec3 counter_center(16,pivot.GetY()-unbalanced.GetY()/counter_mass,pivot.GetZ());
    const float counter_half_y=counter_mass/(7850.F*.8F*1.2F*2);
    Part counter{{.4F,counter_half_y,.6F},counter_center,
                 JPH::Quat::sIdentity(),Material::Hazard};
    counter.mass_kg=counter_mass;counter.convex_radius=0;truss.push_back(counter);
    float span_mass=0;
    for(auto&part:truss){span_mass+=part.mass_kg;part.offset-=pivot;}
    const auto raised=JPH::Quat::sRotation(Vec3::sAxisZ(),kSplitInspectionSpanRaisedAngle);
    const auto truss_body=kit.add_body(kSplitInspectionGirderEntity,truss,
        kSplitInspectionSpanPivot,raised,span_mass,.8F);
    kit.set_damping(truss_body,0,0);kit.set_continuous_collision(truss_body);
    kit.set_accurate_integration(truss_body);
    // No receiver weld or fixed-neutral truss mount remains. The bearing
    // filters only its mating body; independent seats retain real contacts.
    (void)kit.add_hinge(bearing_body,truss_body,kSplitInspectionSpanPivot,Vec3::sAxisZ(),30);

    // Lower seat meets the full-depth box beyond the600mm beveled nose,
    // before deep chords begin at1.5m. CHOSEN25mm visible shim makes this
    // structural seat react first, clearing the adjoining hatch floor.
    // Its bracket transfers rider load into the receiver.
    const auto up=rotation*Vec3::sAxisY();
    Part seat{{.30F,.07F,.06F},kSplitInspectionGirderToe+
        along.Normalized()*1.0F-up*.345F,rotation,Material::Steel};
    set_steel_mass(seat,true);
    std::vector<Part> seats{seat};
    beam(seats,{ -18.2F,472.8F,-134.3F},seat.offset-Vec3(0,.12F,0),.06F);
    const auto seat_body=body(kit,kSplitInspectionSpanSeatEntity,seats,{-18,472.8F,-134.15F});
    kit.set_accurate_integration(seat_body);
    kit.add_fixed_joint(receiver_body,seat_body);

    // Upper rest is another actual seat under the opposite arm in its raised
    // pose. It carries the small residual empty moment, not an angle latch.
    std::vector<Part> cable_frame;
    // Match the actual counterweight bottom face, not the much shallower
    // arm envelope. Every support finishes below this independent seat.
    const auto raised_counter=pivot+raised*(counter_center-pivot);
    Part rest{{.35F,.09F,.55F},raised_counter-
        raised*Vec3(0,counter_half_y+.09F,0),raised,Material::Timber};
    set_steel_mass(rest,true);cable_frame.push_back(rest);
    for(float side:{-1.F,1.F}) {
        if(side<0) continue; // The bearing-side load path runs below the bay.
        // The north shoe bears on the inner roof. Its previous near-edge
        // diagonal crossed the standing approach to the grapple bay even
        // though it cleared the deck's steel. Keep this leg north of that
        // capsule corridor; the rest endpoint and south bearing leg stay put.
        const float z=side>0 ? -131.0F : pivot.GetZ()+side*.9F;
        // The previous direct diagonal cut through the existing grapple
        // departure and recovery steel. Rise beforeX10.8, then clear them.
        const Vec3 knee(10,484.9F,z);
        // The north leg lands on the existing roof top; the outboard leg
        // starts at the bearing bracket. Keep each diagonal outside the
        // counterarm's full square-section envelope, then approach the rest
        // from below after the arm has risen clear.
        beam(cable_frame,{10,side>0?484.35F:483.8F,z},knee,.10F);
        beam(cable_frame,knee,rest.offset+Vec3(0,-.15F,side*.5F),.10F);
    }
    // The old south diagonal also crossed the standing ingress. Carry its
    // real bearing-cheek reaction through the front underfloor notch instead.
    // Y483.2 clears the departure's actual diagonal under-braces; Z-133.65
    // clears concrete and the western recovery deck's standing headroom.
    // The riser finishes before the eastern roof/recovery footprint begins.
    const Vec3 under_knee(10,483.2F,-133.65F);
    const Vec3 under_rest(rest.offset.GetX(),483.2F,-133.65F);
    const auto north_rest=rest.offset+Vec3(0,-.15F,.5F);
    // Cross below both rotating pivot webs, not diagonally through them.
    // Their lowest neutral steel is482.82m; the0.1m half-section crosspiece
    // tops at482.7m and its outboard downleg stays clear of the span width.
    const Vec3 below_south(10,482.6F,pivot.GetZ()-.9F);
    const Vec3 below_north(10,482.6F,-133.65F);
    beam(cable_frame,{10,483.8F,pivot.GetZ()-.9F},below_south,.10F);
    beam(cable_frame,below_south,below_north,.10F);
    beam(cable_frame,below_north,under_knee,.10F);
    beam(cable_frame,under_knee,under_rest,.10F);
    beam(cable_frame,under_rest,north_rest,.10F);
    beam(cable_frame,north_rest,rest.offset+Vec3(0,-.15F,-.5F),.10F);
    // Finite shoes close the rest stand's visible load path: north leg onto
    // the actual484.25m roof; south leg into the aft bearing cheek.
    box(cable_frame,{9.85F,484.25F,-131.15F},{10.15F,484.35F,-130.85F});
    box(cable_frame,{9.85F,483.75F,-135.20F},{10.15F,483.85F,-134.65F});
    // The fixed guide supports stay outside the walking/swept truss corridor.
    // Independent fixed guide mast lands on the actual462.25m west roof.
    // Static guide supports must not begin inside the compliant receiver
    // posts: those are separate bodies and would be pushed apart at startup.
    box(cable_frame,{-21.30F,462.25F,-135.75F},{-20.80F,462.274F,-135.25F},true);
    beam(cable_frame,{-21.05F,462.274F,-135.5F},{-21.05F,471.8F,-135.5F},.09F);
    beam(cable_frame,{-21.05F,471.8F,-135.5F},Vec3(kSplitInspectionLowerSheave),.09F);
    beam(cable_frame,{-21.05F,470.8F,-135.5F},Vec3(kSplitInspectionLowerSheave),.07F);
    beam(cable_frame,{10,483.8F,-134.8F},Vec3(kSplitInspectionUpperSheave),.10F);
    for(const auto sheave:{kSplitInspectionLowerSheave,kSplitInspectionUpperSheave}){
        Part wheel{{.18F,.06F,.18F},Vec3(sheave),
            JPH::Quat::sRotation(Vec3::sAxisX(),JPH::JPH_PI*.5F),Material::Yellow};
        wheel.shape=Part::Shape::Cylinder;wheel.mass_kg=7850*JPH::JPH_PI*.18F*.18F*.12F;
        wheel.convex_radius=0;cable_frame.push_back(wheel);
    }
    (void)fixed_body(kSplitInspectionCableFrameEntity,cable_frame);
    const auto span_local=kSplitInspectionSpanCablePoint-pivot;
    const auto initial_anchor=kSplitInspectionSpanPivot+JPH::RVec3(raised*span_local);
    const auto hatch_anchor=kSplitInspectionHatchPivot+JPH::RVec3(kSplitInspectionHatchCableLocal);
    const float line_length=Vec3(initial_anchor-kSplitInspectionUpperSheave).Length()+
        Vec3(hatch_anchor-kSplitInspectionLowerSheave).Length();
    (void)kit.add_rope(truss_body,span_local,kSplitInspectionUpperSheave,
        leaf_body,kSplitInspectionHatchCableLocal,kSplitInspectionLowerSheave,1,line_length,12000);
}

} // namespace scraperx::sim
