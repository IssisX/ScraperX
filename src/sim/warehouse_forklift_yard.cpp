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
    for (auto entity : {WarehouseForkliftYard::kChassisEntity,
                       WarehouseForkliftYard::kMastEntity,
                       WarehouseForkliftYard::kCarriageEntity,
                       WarehouseForkliftYard::kForkEntity,
                       WarehouseForkliftYard::kFirstCrateEntity,
                       WarehouseForkliftYard::kSecondCrateEntity,
                       WarehouseForkliftYard::kBeamEntity,
                       WarehouseForkliftYard::kFrameEntity})
        if (kit.body_for_entity(entity).valid())
            throw std::invalid_argument("WarehouseForkliftYard entity already owned");

    WarehouseForkliftYard yard;
    const auto identity = JPH::Quat::sIdentity();
    const auto truck = origin + JPH::Vec3(0, .65F, 14);
    yard.truck.chassis = kit.add_body(WarehouseForkliftYard::kChassisEntity, {
        box({.63F, .22F, 1.17F}, {0, -.05F, -.05F}, Material::Yellow, 900),
        box({.60F, .30F, .41F}, {0, .12F, -.76F}, Material::Steel, 1700),
        box({.67F, .06F, .50F}, {0, -.30F, -1.65F}, Material::Steel, 70),
        box({.67F, .18F, .09F}, {0, -.14F, -2.12F}, Material::Yellow, 40),
        box({.38F, .24F, .13F}, {0, .29F, -.72F}, Material::Yellow, 50),
        box({.07F, .38F, .06F}, {-.56F, .37F, -.94F}, Material::Steel, 20),
        box({.07F, .38F, .06F}, {.56F, .37F, -.94F}, Material::Steel, 20),
    }, truck, identity, 2800, .8F);
    yard.truck.mast = kit.add_body(WarehouseForkliftYard::kMastEntity, {
        box({.065F, 1.50F, .075F}, {-.55F, 1.50F, 0}, Material::Steel, 65),
        box({.065F, 1.50F, .075F}, {.55F, 1.50F, 0}, Material::Steel, 65),
        box({.55F, .08F, .08F}, {0, 2.92F, 0}, Material::Yellow, 30),
        box({.57F, .10F, .10F}, {0, .10F, 0}, Material::Steel, 60),
    }, truck + JPH::Vec3(0, 0, 1.30F), identity, 220, .7F);
    yard.truck.carriage = kit.add_body(WarehouseForkliftYard::kCarriageEntity, {
        box({.62F, .08F, .07F}, {0, .10F, 0}, Material::Steel, 16),
        box({.62F, .08F, .07F}, {0, .62F, 0}, Material::Steel, 16),
        box({.06F, .25F, .08F}, {-.52F, .35F, 0}, Material::Steel, 4),
        box({.06F, .25F, .08F}, {.52F, .35F, 0}, Material::Steel, 4),
    }, truck + JPH::Vec3(0, -.48F, 1.55F), identity, 40, .7F);
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
    }, truck + JPH::Vec3(0, -.48F, 1.73F), identity, 72, .85F);
    yard.truck.tilt_pivot = truck + JPH::Vec3(0, -.03F, 1.30F);
    yard.truck.tilt_axis = -JPH::Vec3::sAxisX();
    yard.truck.carriage_mount = {0, .25F, .12F};
    yard.truck.tool_mount = {0, .25F, -.06F};
    kit.disable_collision(yard.truck.chassis, yard.truck.mast);
    kit.disable_collision(yard.truck.mast, yard.truck.carriage);
    kit.disable_collision(yard.truck.carriage, yard.truck.fork_frame);
    for (const auto index : {yard.truck.chassis, yard.truck.mast,
                            yard.truck.carriage, yard.truck.fork_frame})
        kit.set_continuous_collision(index);

    yard.crates[0] = kit.add_body(WarehouseForkliftYard::kFirstCrateEntity,
        crate_parts(), origin + JPH::Vec3(0, 0, 22), identity, 600, .8F);
    yard.crates[1] = kit.add_body(WarehouseForkliftYard::kSecondCrateEntity,
        crate_parts(), origin + JPH::Vec3(-5, 0, 22), identity, 600, .8F);
    yard.beam = kit.add_body(WarehouseForkliftYard::kBeamEntity, {
        box({3, .05F, .45F}, {0, .15F, 0}, Material::Yellow, 210),
        box({3, .15F, .045F}, {0, -.05F, 0}, Material::Steel, 180),
        box({3, .05F, .45F}, {0, -.25F, 0}, Material::Steel, 210),
    }, origin + JPH::Vec3(5, .80F, 26), identity, 600, .85F);
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
    yard.stack_center = origin + JPH::Vec3(0, 0, -17);
    yard.receiver_top = origin + JPH::Vec3(0, 5.9F, -22);
    return yard;
}
} // namespace scraperx::sim
