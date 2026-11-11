#include "sim/slab_cell.hpp"

#include <Jolt/Math/Quat.h>

#include <vector>

namespace scraperx::sim {

using kit::Material;
using kit::Part;

void SlabCell::build(kit::Kit &kit, const kit::BodyIndex slab_body) {
    slab = slab_body;

    // ---- the frame, bolted under the band ---------------------------------
    // Built about the slab's middle at the band's underside. The hoist beam
    // along z, the hook block hung from it with its eye 9 cm below, the
    // trigger's cheeks off the block's west face either side of the pivot
    // (8 mm clear of the trigger), and the two sheave blocks, each hung from
    // the band with its sheave's bottom on the cord's line.
    const JPH::RVec3 frame_at(kSlabX, kRingUnderside, kSlabZ);
    const float pivot_dx = kTriggerPivotX - kSlabX;
    const float pivot_dy = kTriggerPivotY - kRingUnderside;
    const float sheave_dy = kSheaveY - kRingUnderside + 0.05F;
    std::vector<Part> frame_parts = {
        {JPH::Vec3(0.06F, 0.08F, 0.6F), JPH::Vec3(0.0F, -0.08F, 0.0F), JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.06F, 0.10F, 0.06F), JPH::Vec3(0.0F, -0.26F, 0.0F), JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.05F, 0.05F, 0.04F), JPH::Vec3(pivot_dx - kTriggerTail, sheave_dy, 0.0F), JPH::Quat::sIdentity(),
         Material::Steel},
        {JPH::Vec3(0.05F, 0.05F, 0.04F), JPH::Vec3(kHandleX - kSlabX, sheave_dy, kHandleZ - kSlabZ),
         JPH::Quat::sIdentity(), Material::Steel},
    };
    for (const float side : {-1.0F, 1.0F}) {
        frame_parts.push_back({JPH::Vec3(0.07F, 0.06F, 0.012F),
                               JPH::Vec3(pivot_dx + 0.02F, pivot_dy + 0.01F, side * 0.05F), JPH::Quat::sIdentity(),
                               Material::Rust});
    }
    frame = kit.add_body(kFrameEntity, frame_parts, frame_at, JPH::Quat::sIdentity(), 0.0F, 0.6F);

    // ---- the sling, from the slab's eye to the hook ------------------------
    // Made fast to the frame at the hook's eye, exactly taut with the slab
    // where it was laid: the slab hangs as built.
    const JPH::RVec3 hook_eye(kSlabX, kHookY, kSlabZ);
    const JPH::Vec3 slab_eye(0.0F, 0.17F, 0.0F);   // LooseStock's PlateSlab eye
    const JPH::RVec3 eye_at = kit.body_position(slab) + JPH::RVec3(slab_eye);
    const JPH::Vec3 frame_local(hook_eye - frame_at);
    sling = kit.add_direct_rope(slab, slab_eye, frame, frame_local, JPH::Vec3(hook_eye - eye_at).Length(), 0.0F);

    // ---- the trigger ---------------------------------------------------------
    // Built about its pivot, its tail west; uniform, so its weight sits half
    // the tail out and lays it on its stop. About -z with the tail west, so
    // lifting the tail is a positive angle.
    const JPH::RVec3 pivot(kTriggerPivotX, kTriggerPivotY, kSlabZ);
    trigger_body = kit.add_body(kTriggerEntity,
                                {{JPH::Vec3(0.5F * kTriggerTail, 0.03F, 0.03F), JPH::Vec3(-0.5F * kTriggerTail, 0.0F, 0.0F),
                                  JPH::Quat::sIdentity(), Material::Hazard, Part::Shape::Box, kTriggerKg}},
                                pivot, JPH::Quat::sIdentity(), kTriggerKg, 0.5F);
    trigger = kit.add_lever(trigger_body, pivot, -JPH::Vec3::sAxisZ(), -JPH::Vec3::sAxisX(), 0.0F, kTriggerTravelRad);
    hook = kit.add_slip(sling, trigger, kReleaseRad);
    // Its cheeks hold it on the pivot; the frame never stops it swinging.
    kit.disable_collision(frame, trigger_body);

    // ---- the cord and its handle ---------------------------------------------
    const JPH::RVec3 over_tail(kTriggerPivotX - kTriggerTail, kSheaveY, kSlabZ);
    const JPH::RVec3 over_handle(kHandleX, kSheaveY, kHandleZ);
    const JPH::Vec3 handle_top(0.0F, kHandleHalfY, 0.0F);
    handle = kit.add_body(kHandleEntity,
                          {{JPH::Vec3(0.22F, kHandleHalfY, 0.04F), JPH::Vec3::sZero(), JPH::Quat::sIdentity(),
                            Material::Yellow}},
                          JPH::RVec3(kHandleX, kHandleTopY - kHandleHalfY, kHandleZ), JPH::Quat::sIdentity(),
                          kHandleKg, 0.9F);
    kit.set_carry(handle, kit::CarryKind::Handle, handle_top);
    kit.set_damping(handle, 1.5F, 1.5F);
    (void)kit.add_trip_line(trigger_body, JPH::Vec3(-kTriggerTail, 0.0F, 0.0F), handle, handle_top, over_tail,
                            over_handle);
}

}  // namespace scraperx::sim
