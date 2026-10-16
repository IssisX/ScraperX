#include "sim/warehouse_forklift_yard.hpp"

#include <stdexcept>

namespace scraperx::sim {
namespace {
using kit::Material;
using kit::Part;

Part box(JPH::Vec3 half, JPH::Vec3 at, Material material, float mass = 0) {
    Part part;
    part.half = half;
    part.offset = at;
    part.material = material;
    part.mass_kg = mass;
    part.convex_radius = .005F;
    return part;
}

std::vector<Part> crate_parts() {
    // Hollow 2.4m square steel shipping crate, 1.5m high. Three ground skids
    // leave two genuine front-to-back fork tunnels centered at x=+/-.48m.
    // The truck cannot lift through a painted slot or a solid bottom block.
    std::vector<Part> parts{
        box({1.2F, .06F, 1.2F}, {0, 1.44F, 0}, Material::Yellow, 140),
        box({1.2F, .05F, 1.2F}, {0, .27F, 0}, Material::Steel, 120),
        box({.06F, .555F, 1.2F}, {-1.14F, .845F, 0}, Material::Rust, 70),
        box({.06F, .555F, 1.2F}, {1.14F, .845F, 0}, Material::Rust, 70),
        box({1.08F, .555F, .04F}, {0, .845F, -1.16F}, Material::Steel, 55),
        box({1.08F, .555F, .04F}, {0, .845F, 1.16F}, Material::Steel, 55),
    };
    for (float x : {-1.10F, 0.0F, 1.10F})
        parts.push_back(box({.10F, .11F, 1.2F}, {x, .11F, 0}, Material::Timber, 30));
    return parts; // 600kg explicitly distributed in material parts.
}
} // namespace

WarehouseForkliftYard build_warehouse_forklift_yard(kit::Kit &kit, JPH::RVec3 origin) {
    return build_warehouse_forklift_yard(kit, origin, JPH::Quat::sIdentity());
}

WarehouseForkliftYard build_warehouse_forklift_yard(kit::Kit &kit, JPH::RVec3 origin,
                                                  JPH::Quat orientation) {
    if (!orientation.IsNormalized()) throw std::invalid_argument("Forklift bay frame invalid");
    for (auto entity : {WarehouseForkliftYard::kChassisEntity,
                       WarehouseForkliftYard::kMastEntity,
                       WarehouseForkliftYard::kCarriageEntity,
                       WarehouseForkliftYard::kForkEntity,
                       WarehouseForkliftYard::kFirstCrateEntity,
                       WarehouseForkliftYard::kSecondCrateEntity,
                       WarehouseForkliftYard::kBeamEntity,
                       WarehouseForkliftYard::kParkingCradleEntity,
                       WarehouseForkliftYard::kFrameEntity})
        if (kit.body_for_entity(entity).valid())
            throw std::invalid_argument("WarehouseForkliftYard entity already owned");

    WarehouseForkliftYard yard;
    const auto identity = orientation;
    const auto truck = origin + orientation * JPH::Vec3(0, .65F, 14);
    std::vector<Part> chassis_parts{
        box({.63F, .22F, 1.17F}, {0, -.05F, -.05F}, Material::Yellow, 900),
        box({.60F, .30F, .41F}, {0, .12F, -.76F}, Material::Steel, 1700),
        box({.67F, .06F, .50F}, {0, -.30F, -1.65F}, Material::Steel, 70),
        box({.67F, .18F, .09F}, {0, -.14F, -2.12F}, Material::Yellow, 40),
        box({.38F, .24F, .13F}, {0, .29F, -.72F}, Material::Yellow, 50),
        box({.07F, .38F, .06F}, {-.56F, .37F, -.94F}, Material::Steel, 20),
        box({.07F, .38F, .06F}, {.56F, .37F, -.94F}, Material::Steel, 20),
    };
    // Four open80x110mm lifting eyes, welded to the actual battery/frame
    // faces. Sections meet without overlapping material. Their steel mass
    // joins the authored chassis mass and therefore its COM and inertia.
    constexpr float steel_density = 7800;
    constexpr float leg_mass = 8 * .01F * .055F * .015F * steel_density;
    constexpr float bar_mass = 8 * .06F * .015F * .015F * steel_density;
    for (const auto eye : WarehouseForkliftYard::lifting_points()) {
        const float base_y = eye.GetY() - .145F;
        for (const float side : {-1.F, 1.F})
            chassis_parts.push_back(box({.01F, .055F, .015F},
                {eye.GetX() + side * .05F, base_y + .075F, eye.GetZ()},
                Material::Steel, leg_mass));
        chassis_parts.push_back(box({.06F, .015F, .015F},
            {eye.GetX(), base_y + .005F, eye.GetZ()}, Material::Steel, bar_mass));
        chassis_parts.push_back(box({.06F, .015F, .015F}, eye, Material::Steel, bar_mass));
    }
    // Welded overhead guard, carried by the chassis rather than a cosmetic
    // cab. Actual 60mm square steel tubes have 4mm walls and an open bore.
    // Every wall/plate contributes its own volume-derived mass and inertia.
    float operator_guard_mass = 0;
    const auto steel_part = [&](JPH::Vec3 half, JPH::Vec3 at) {
        const float mass = 8 * half.GetX() * half.GetY() * half.GetZ() * steel_density;
        auto part = box(half, at, Material::Steel, mass);
        // Thin tube walls and roof plates need a valid, small contact bevel.
        part.convex_radius = .001F;
        chassis_parts.push_back(part);
        operator_guard_mass += mass;
    };
    const auto upright = [&](float x, float z, float bottom) {
        const float half_height = (WarehouseForkliftYard::kOperatorRoofUnderside - bottom) / 2;
        const float y = bottom + half_height;
        for (const float side : {-1.F, 1.F}) {
            steel_part({.002F, half_height, .03F}, {x + side * .028F, y, z});
            steel_part({.026F, half_height, .002F}, {x, y, z + side * .028F});
        }
    };
    for (const float x : {-.63F, .63F}) {
        upright(x, -2.10F, -.24F); // existing standing deck
        upright(x < 0 ? -.62F : .62F, -1.02F, .42F); // 10mm bearing on battery top
    }
    // Side entrances between the posts are 1.02m long, with 2.12m vertical
    // clearance above the unchanged deck: the .70m-wide/1.80m-tall standing
    // capsule can enter either side. No side rail crosses that opening.
    const float beam_y = WarehouseForkliftYard::kOperatorRoofUnderside + .03F;
    for (const float z : {-2.10F, -1.02F}) {
        for (const float side : {-1.F, 1.F}) {
            steel_part({.66F, .002F, .03F}, {0, beam_y + side * .028F, z});
            steel_part({.66F, .026F, .002F}, {0, beam_y, z + side * .028F});
        }
    }
    for (const float x : {-.63F, .63F}) {
        for (const float side : {-1.F, 1.F}) {
            steel_part({.002F, .03F, .51F}, {x + side * .028F, beam_y, -1.56F});
            steel_part({.026F, .002F, .51F}, {x, beam_y + side * .028F, -1.56F});
        }
    }
    // Six-mm steel roof stays aft of the rear lifting eyes/slings at z=-.75.
    // All forward sightlines below the overhead beams remain open to the mast.
    steel_part({.70F, .003F, .675F},
        {0, beam_y + .033F, WarehouseForkliftYard::kOperatorRoofFront - .675F});
    // Real control pedestal and dashboard, forward of the standing capsule.
    // Its upper surface is below the actual standing eye (-.24m deck +1.52m).
    // Eight-mm pedestal walls support a six-mm dashboard top. Open cavity
    // remains real empty volume, rather than being assigned steel-block mass.
    for (const float side : {-1.F, 1.F}) {
        steel_part({.004F, .0795F, .07F}, {side * .096F, .4995F, -1.02F});
        steel_part({.092F, .0795F, .004F}, {0, .4995F, -1.02F + side * .066F});
    }
    steel_part({.48F, .003F, .10F}, {0, .582F, -1.02F});
    steel_part({.03F, .0975F, .03F}, {.10F, .6825F, -1.03F});
    // Welded side boarding treads: two ordinary .18m rises from settled
    // grade to the existing operator floor. Six-mm steel plate carries
    // real contact; vertical mounting plates join its inner edge to deck.
    // Both stay within a 2m total width and the actual open doorway.
    for (const float side : {-1.F, 1.F}) {
        steel_part({.165F, .003F, .36F}, {side * .835F, -.423F, -1.65F});
        steel_part({.003F, .09F, .36F}, {side * .673F, -.33F, -1.65F});
        for (const float end : {-1.F, 1.F})
            steel_part({.162F, .03F, .003F},
                {side * .838F, -.456F, -1.65F + end * .27F});
    }
    yard.truck.chassis = kit.add_body(WarehouseForkliftYard::kChassisEntity,
        chassis_parts, truck, identity,
        2800 + 8 * (leg_mass + bar_mass) + operator_guard_mass, .8F);
    yard.truck.mast = kit.add_body(WarehouseForkliftYard::kMastEntity, {
        box({.065F, 1.50F, .075F}, {-.55F, 1.50F, 0}, Material::Steel, 65),
        box({.065F, 1.50F, .075F}, {.55F, 1.50F, 0}, Material::Steel, 65),
        box({.55F, .08F, .08F}, {0, 2.92F, 0}, Material::Yellow, 30),
        box({.57F, .10F, .10F}, {0, .10F, 0}, Material::Steel, 60),
    }, truck + orientation * JPH::Vec3(0, 0, 1.30F), identity, 220, .7F);
    yard.truck.carriage = kit.add_body(WarehouseForkliftYard::kCarriageEntity, {
        box({.62F, .08F, .07F}, {0, .10F, 0}, Material::Steel, 16),
        box({.62F, .08F, .07F}, {0, .62F, 0}, Material::Steel, 16),
        box({.06F, .25F, .08F}, {-.52F, .35F, 0}, Material::Steel, 4),
        box({.06F, .25F, .08F}, {.52F, .35F, 0}, Material::Steel, 4),
    }, truck + orientation * JPH::Vec3(0, -.48F, 1.55F), identity, 40, .7F);
    yard.truck.fork_frame = kit.add_body(WarehouseForkliftYard::kForkEntity, {
        box({.60F, .06F, .06F}, {0, .12F, 0}, Material::Steel, 12),
        box({.60F, .06F, .06F}, {0, .62F, 0}, Material::Steel, 12),
        box({.075F, .29F, .06F}, {-.48F, .29F, 0}, Material::Steel, 3),
        box({.075F, .29F, .06F}, {.48F, .29F, 0}, Material::Steel, 3),
        // The2.4m-deep crates need a real support polygon beyond their COM.
        // 1.3m blades ended at that COM and lost the load on reverse takeoff.
        // Longer industrial blades carry their own additional mass/inertia;
        // neither cargo adhesion nor a stronger actuator hides that geometry.
        box({.08F, .05F, .90F}, {-.48F, 0, .90F}, Material::Steel, 21),
        box({.08F, .05F, .90F}, {.48F, 0, .90F}, Material::Steel, 21),
    }, truck + orientation * JPH::Vec3(0, -.48F, 1.73F), identity, 72, .85F);
    yard.truck.tilt_pivot = truck + orientation * JPH::Vec3(0, -.03F, 1.30F);
    yard.truck.tilt_axis = orientation * -JPH::Vec3::sAxisX();
    yard.truck.tilt_normal = orientation * JPH::Vec3::sAxisZ();
    yard.truck.lift_axis = orientation * JPH::Vec3::sAxisY();
    yard.truck.carriage_mount = {0, .25F, .12F};
    yard.truck.tool_mount = {0, .25F, -.06F};
    kit.disable_collision(yard.truck.chassis, yard.truck.mast);
    kit.disable_collision(yard.truck.mast, yard.truck.carriage);
    kit.disable_collision(yard.truck.carriage, yard.truck.fork_frame);
    for (const auto index : {yard.truck.chassis, yard.truck.mast,
                            yard.truck.carriage, yard.truck.fork_frame})
        kit.set_continuous_collision(index);

    yard.crates[0] = kit.add_body(WarehouseForkliftYard::kFirstCrateEntity,
        crate_parts(), origin + orientation * JPH::Vec3(0, 0, 22), identity, 600, .8F);
    yard.crates[1] = kit.add_body(WarehouseForkliftYard::kSecondCrateEntity,
        crate_parts(), origin + orientation * JPH::Vec3(-5, 0, 22), identity, 600, .8F);
    yard.beam = kit.add_body(WarehouseForkliftYard::kBeamEntity, {
        box({3, .05F, .45F}, {0, .15F, 0}, Material::Yellow, 210),
        box({3, .15F, .045F}, {0, -.05F, 0}, Material::Steel, 180),
        box({3, .05F, .45F}, {0, -.25F, 0}, Material::Steel, 210),
    }, origin + orientation * JPH::Vec3(5, .80F, 26), identity, 600, .85F);
    for (const auto index : {yard.crates[0], yard.crates[1], yard.beam})
        kit.set_continuous_collision(index);

    // A high receiving gallery rewards actual raised footing. A six-meter
    // horizontal beam gap alone would still be sprint-jumpable; it is not
    // falsely treated as a mandatory truck puzzle.
    std::vector<Part> receiving{
        box({3.4F, .12F, 2.5F}, {0, 5.78F, -24}, Material::Steel),
        box({.14F, 2.83F, .14F}, {-3, 2.83F, -26}, Material::Rust),
        box({.14F, 2.83F, .14F}, {3, 2.83F, -26}, Material::Rust),
        box({.14F, 2.83F, .14F}, {-3, 2.83F, -22}, Material::Rust),
        box({.14F, 2.83F, .14F}, {3, 2.83F, -22}, Material::Rust),
        box({3.25F, .22F, .13F}, {0, 5.45F, -26}, Material::Rust),
        box({3.25F, .22F, .13F}, {0, 5.45F, -22}, Material::Rust),
    };
    // Actual beam dunnage; open central fork corridor, no invisible support.
    receiving.push_back(box({.15F, .25F, .55F}, {3, .25F, 26}, Material::Timber));
    receiving.push_back(box({.15F, .25F, .55F}, {7, .25F, 26}, Material::Timber));
    yard.receiving_frame = kit.add_body(WarehouseForkliftYard::kFrameEntity,
        receiving, origin, identity, 0, .85F);
    yard.stack_center = origin + orientation * JPH::Vec3(0, 0, -17);
    yard.receiver_top = origin + orientation * JPH::Vec3(0, 5.9F, -22);
    // Two grade-bearing solid steel dunnage rails form a real fork parking
    // cradle, 12m east of the retained warehouse lane. Top faces support both
    // blade bottom centers; the tool's COM lies inside their support polygon.
    // The truck approaches facing bay-local -Z. Relative to the parked tool,
    // steel occupies X +/- .65m and Z .30..1.50m, ahead of its rear frame and
    // the carriage. No rear stop obstructs withdrawal or return coupling.
    yard.parking_center = origin + orientation * JPH::Vec3(12, 0, 0);
    std::vector<Part> parking_parts;
    for (const float x : {-.48F, .48F}) {
        const JPH::Vec3 half(.17F, WarehouseForkliftYard::kParkingCradleTop * .5F, .60F);
        const float mass = 8 * half.GetX() * half.GetY() * half.GetZ() * steel_density;
        parking_parts.push_back(box(half,
            {x, WarehouseForkliftYard::kParkingCradleTop * .5F, 0}, Material::Steel, mass));
    }
    yard.parking_cradle = kit.add_body(WarehouseForkliftYard::kParkingCradleEntity,
        parking_parts, yard.parking_center, identity, 0, .85F);
    return yard;
}
std::array<JPH::Vec3, 4> WarehouseForkliftYard::lifting_points() noexcept {
    return {JPH::Vec3(-.52F, .315F, .75F), JPH::Vec3(.52F, .315F, .75F),
            JPH::Vec3(-.52F, .565F, -.75F), JPH::Vec3(.52F, .565F, -.75F)};
}
} // namespace scraperx::sim
