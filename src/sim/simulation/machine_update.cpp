#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

void Simulation::PhysicsWorld::update_scoop(JPH::BodyInterface &bodies,
                  const float delta_seconds,
                  const double next_time_seconds) noexcept {
    const double phase = std::fmod(next_time_seconds, kMachineCyclePeriodSeconds);
    machine_cycle_phase_seconds_ = phase;

    float height = kScoopBottomY;
    float tilt = 0.0F;
    if (phase < 7.0) {
        height = kScoopBottomY + (kScoopTopY - kScoopBottomY) *
                                     smoothstep(0.0F, 1.0F, static_cast<float>(phase / 7.0));
    } else if (phase < 9.0) {
        height = kScoopTopY;
    } else if (phase < 12.5) {
        height = kScoopTopY;
        tilt = kScoopDischargeTilt *
               smoothstep(0.0F, 1.0F, static_cast<float>((phase - 9.0) / 3.5));
    } else if (phase < 14.0) {
        height = kScoopTopY;
        tilt = kScoopDischargeTilt;
    } else if (phase < 16.0) {
        height = kScoopTopY + (kScoopBottomY - kScoopTopY) *
                                  smoothstep(0.0F, 1.0F, static_cast<float>((phase - 14.0) / 2.0));
        tilt = kScoopDischargeTilt *
               (1.0F - smoothstep(0.0F, 1.0F, static_cast<float>((phase - 14.0) / 1.6)));
    }

    scoop_height_ = height;
    scoop_tilt_ = tilt;
    const JPH::Quat rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), tilt);
    const JPH::RVec3 origin(kScoopX, height, kScoopZ);
    for (int i = 0; i < 4; ++i) {
        bodies.MoveKinematic(scoop_ids_[i], origin + rotation * scoop_local_[i], rotation,
                             delta_seconds);
    }
}

void Simulation::PhysicsWorld::update_plant(JPH::BodyInterface &bodies, const float delta_seconds) noexcept {
    float lever_angle = kValveShutAngle;
    if (valve_hinge_ != nullptr) {
        const float measured = valve_hinge_->GetCurrentAngle();
        if (std::isfinite(measured)) {
            lever_angle = measured;
        }
    }
    valve_lever_angle_ = lever_angle;
    if (treadle_hinge_ != nullptr) {
        const float measured = treadle_hinge_->GetCurrentAngle();
        if (std::isfinite(measured)) {
            treadle_angle_ = measured;
        }
    }
    const float span = kValveOpenAngle - kValveShutAngle;
    const double fraction =
        span > 1.0e-4F ? static_cast<double>((lever_angle - kValveShutAngle) / span) : 0.0;
    steam_plant_.set_valve_open_fraction(fraction);
    steam_plant_.step(static_cast<double>(delta_seconds));

    const float piston_force = static_cast<float>(steam_plant_.state().piston_force_n);
    if (piston_force > 0.0F) {
        bodies.AddForce(lift_platform_id_, JPH::Vec3(0.0F, piston_force, 0.0F));
    }
}

void Simulation::PhysicsWorld::update_jib(const JPH::BodyInterface &bodies,
                const double slew_input,
                const double hoist_input) noexcept {
    const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
    const float station_dx = static_cast<float>(player_position.GetX()) - kJibStationX;
    const float station_dz = static_cast<float>(player_position.GetZ()) - kJibStationZ;
    const bool at_station =
        (station_dx * station_dx + station_dz * station_dz) <=
        (kJibStationRadius * kJibStationRadius);
    jib_station_active_ = at_station;

    const float slew =
        at_station ? std::clamp(static_cast<float>(slew_input), -1.0F, 1.0F) : 0.0F;
    const float hoist =
        at_station ? std::clamp(static_cast<float>(hoist_input), -1.0F, 1.0F) : 0.0F;

    if (jib_slew_hinge_ != nullptr) {
        jib_slew_hinge_->SetTargetAngularVelocity(slew * kJibSlewMaxRateRadPerSec);
        const float measured = jib_slew_hinge_->GetCurrentAngle();
        if (std::isfinite(measured)) {
            jib_boom_angle_ = measured;
        }
    }
    if (jib_hoist_slider_ != nullptr) {
        if (hoist != 0.0F) {
            jib_hoist_slider_->SetMotorState(JPH::EMotorState::Velocity);
            jib_hoist_slider_->SetTargetVelocity(hoist * kJibHoistMaxRateMetersPerSec);
        } else {
            // Brake at the measured extension, rather than merely request
            // zero speed while the suspended load disturbs the winch.
            // Jolt's damped position motor applies real reciprocal forces
            // through the SAME 20 kN rating; it never assigns a body pose.
            if (jib_hoist_slider_->GetMotorState() != JPH::EMotorState::Position)
                jib_hoist_slider_->SetTargetPosition(jib_hoist_slider_->GetCurrentPosition());
            jib_hoist_slider_->SetMotorState(JPH::EMotorState::Position);
            jib_hoist_slider_->SetTargetVelocity(0.0F);
        }
    }
}

void Simulation::PhysicsWorld::update_needle(const JPH::BodyInterface &bodies, const double hoist_input) noexcept {
    const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
    const float station_dx = static_cast<float>(player_position.GetX()) - kNeedleStationX;
    const float station_dz = static_cast<float>(player_position.GetZ()) - kNeedleStationZ;
    const bool at_station =
        (station_dx * station_dx + station_dz * station_dz) <=
        (kNeedleStationRadius * kNeedleStationRadius);
    needle_station_active_ = at_station;

    const float hoist =
        at_station ? std::clamp(static_cast<float>(hoist_input), -1.0F, 1.0F) : 0.0F;
    if (needle_hoist_slider_ != nullptr) {
        needle_hoist_slider_->SetTargetVelocity(hoist * kNeedleHoistMaxRateMetersPerSec);
    }

    // Gated on "not actively raising": right after unseat_needle() removes
    // the pins, the beam is still sitting exactly at the seat pose with
    // near-zero velocity for at least one tick, since the motor needs real
    // time to accelerate it away. Checking the seat predicate unconditionally
    // would re-seat it that same tick, before a held raise command ever got
    // a chance to move it -- found by direct observation (the unseat proof
    // path never actually left the seated state). A held raise is an
    // unambiguous "not trying to seat" signal, so it suppresses the check
    // entirely rather than racing it.
    if (!needle_seated_ && hoist <= 0.0F) {
        const JPH::RVec3 beam_position = bodies.GetPosition(needle_beam_id_);
        const JPH::Vec3 beam_velocity = bodies.GetLinearVelocity(needle_beam_id_);
        const JPH::Vec3 beam_angular_velocity = bodies.GetAngularVelocity(needle_beam_id_);
        const float height_error =
            std::fabs(static_cast<float>(beam_position.GetY()) - kNeedleSeatedY);
        const bool close_enough = height_error <= kNeedleSeatPositionToleranceMeters;
        const bool settled = beam_velocity.Length() <= kNeedleSeatSpeedToleranceMetersPerSec &&
                             beam_angular_velocity.Length() <= kNeedleSeatSpeedToleranceMetersPerSec;
        if (close_enough && settled) {
            seat_needle();
        }
    } else if (needle_seated_ && hoist > kNeedleUnseatCommandThreshold) {
        unseat_needle();
    }
}

void Simulation::PhysicsWorld::seat_needle() noexcept {
    JPH::PointConstraintSettings approach_settings;
    approach_settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    approach_settings.mPoint1 =
        JPH::RVec3(kNeedlePocketApproachX, kNeedleSeatedY, kNeedleGapCenterZ);
    approach_settings.mPoint2 = approach_settings.mPoint1;
    needle_pin_approach_ = static_cast<JPH::PointConstraint *>(create_constraint(
        approach_settings, needle_pier_approach_id_, needle_beam_id_, false));

    JPH::PointConstraintSettings far_settings;
    far_settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    far_settings.mPoint1 = JPH::RVec3(kNeedlePocketFarX, kNeedleSeatedY, kNeedleGapCenterZ);
    far_settings.mPoint2 = far_settings.mPoint1;
    needle_pin_far_ = static_cast<JPH::PointConstraint *>(
        create_constraint(far_settings, needle_pier_far_id_, needle_beam_id_, false));

    needle_seated_ = true;
}

void Simulation::PhysicsWorld::unseat_needle() noexcept {
    if (needle_pin_approach_ != nullptr) {
        physics_system_.RemoveConstraint(needle_pin_approach_);
        needle_pin_approach_ = nullptr;
    }
    if (needle_pin_far_ != nullptr) {
        physics_system_.RemoveConstraint(needle_pin_far_);
        needle_pin_far_ = nullptr;
    }
    needle_seated_ = false;
}

void Simulation::PhysicsWorld::restore_needle_topology(const bool checkpoint_seated) noexcept {
    if (checkpoint_seated && !needle_seated_) {
        seat_needle();
    } else if (!checkpoint_seated && needle_seated_) {
        unseat_needle();
    }
}

void Simulation::PhysicsWorld::update_water_screw(const JPH::BodyInterface &bodies,
                        const double delta_seconds,
                        const bool toggle_requested) noexcept {
    const JPH::RVec3 player = bodies.GetPosition(player_id_);
    const float dx = static_cast<float>(player.GetX()) - kWaterScrewStationX;
    const float dz = static_cast<float>(player.GetZ()) - kWaterScrewStationZ;
    water_screw_station_active_ =
        dx * dx + dz * dz <= kWaterScrewStationRadius * kWaterScrewStationRadius;
    if (toggle_requested && water_screw_station_active_) water_screw_.toggle_motor();
    water_screw_.step(delta_seconds);
}

void Simulation::PhysicsWorld::update_water_lift(const JPH::BodyInterface &bodies,
                       const double delta_seconds,
                       const bool valve_toggle_requested,
                       const bool release_requested,
                       const bool reset_requested) noexcept {
    const JPH::RVec3 player = bodies.GetPosition(player_id_);
    const auto near_station = [&player](const float x, const float y, const float z) {
        const JPH::Vec3 d = JPH::Vec3(player - JPH::RVec3(x, y, z));
        return d.LengthSq() <= kWaterLiftStationRadius * kWaterLiftStationRadius;
    };
    water_lift_valve_station_active_ =
        near_station(kWaterLiftValveStationX, kWaterLiftValveStationY, kWaterLiftValveStationZ);
    water_lift_release_station_active_ =
        near_station(kWaterLiftReleaseStationX, kWaterLiftReleaseStationY,
                     kWaterLiftReleaseStationZ);
    water_lift_reset_station_active_ =
        near_station(kWaterLiftResetStationX, kWaterLiftResetStationY, kWaterLiftResetStationZ);

    if (valve_toggle_requested && water_lift_valve_station_active_) {
        water_lift_valve_open_ = !water_lift_valve_open_;
    }
    if (release_requested && water_lift_release_station_active_) {
        kit_->release_catch(ground_water_lift_.bucket_top_catch);
    }
    if (reset_requested && water_lift_reset_station_active_) {
        kit_->release_catch(ground_water_lift_.cage_upper_catch);
    }

    const double bucket_travel = kit_->guide_travel(ground_water_lift_.bucket_guide);
    const double cage_travel = kit_->guide_travel(ground_water_lift_.cage_guide);
    if (bucket_travel < -0.20) {
        kit_->arm_catch(ground_water_lift_.bucket_top_catch);
    }
    if (cage_travel < 7.80) {
        kit_->arm_catch(ground_water_lift_.cage_upper_catch);
    }

    water_lift_valve_flow_m3_s_ = 0.0;
    if (water_lift_valve_open_ &&
        kit_->catch_latched(ground_water_lift_.bucket_top_catch) &&
        water_lift_bucket_water_m3_ < kWaterLiftBucketCapacityM3) {
        const double request =
            std::min(kWaterLiftFillRateM3S * delta_seconds,
                     kWaterLiftBucketCapacityM3 - water_lift_bucket_water_m3_);
        const double moved = water_screw_.withdraw_tank_volume_m3(request);
        water_lift_bucket_water_m3_ += moved;
        water_lift_valve_flow_m3_s_ = moved / delta_seconds;
    }

    // Once the bucket reaches its physical lower stop, its bottom drain
    // returns the same water to the ground basin. No water is destroyed.
    if (bucket_travel < -3.92 &&
        kit_->catch_latched(ground_water_lift_.cage_upper_catch) &&
        water_lift_bucket_water_m3_ > 0.0) {
        const double request =
            std::min(kWaterLiftDrainRateM3S * delta_seconds, water_lift_bucket_water_m3_);
        const double moved = water_screw_.return_to_basin_m3(request);
        water_lift_bucket_water_m3_ -= moved;
        water_lift_valve_flow_m3_s_ = -moved / delta_seconds;
    }

    kit_->set_body_mass(
        ground_water_lift_.bucket,
        static_cast<float>(kWaterLiftBucketDryMassKg +
                           kWaterDensityKgM3 * water_lift_bucket_water_m3_));
}

void Simulation::PhysicsWorld::update_intake(const JPH::BodyInterface &bodies,
                   const double slew_input,
                   const double hoist_input) noexcept {
    const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
    const float station_dx = static_cast<float>(player_position.GetX()) - kIntakePendantX;
    const float station_dz = static_cast<float>(player_position.GetZ()) - kIntakePendantZ;
    const bool at_station =
        (station_dx * station_dx + station_dz * station_dz) <=
        (kIntakeStationRadius * kIntakeStationRadius);
    intake_station_active_ = at_station;

    const float slew =
        at_station ? std::clamp(static_cast<float>(slew_input), -1.0F, 1.0F) : 0.0F;
    const float hoist =
        at_station ? std::clamp(static_cast<float>(hoist_input), -1.0F, 1.0F) : 0.0F;

    if (intake_slew_hinge_ != nullptr) {
        intake_slew_hinge_->SetTargetAngularVelocity(slew * kIntakeJibSlewRateRadPerSec);
        const float measured = intake_slew_hinge_->GetCurrentAngle();
        if (std::isfinite(measured)) {
            intake_boom_angle_ = measured;
        }
    }
    if (intake_hoist_slider_ != nullptr) {
        intake_hoist_slider_->SetTargetVelocity(hoist * kIntakeJibHoistRateMetersPerSec);
    }

    // Derived predicates, for the HUD and the falsifiers only. Nothing in
    // this simulation branches on either of them: the dog is held by the
    // pack's mass through the solver, and the throat is open or shut
    // because a real body is or is not standing in it.
    if (intake_dog_hinge_ != nullptr) {
        const float measured = intake_dog_hinge_->GetCurrentAngle();
        if (std::isfinite(measured)) {
            intake_dog_angle_ = measured;
        }
    }
    intake_throat_clear_ = intake_dog_angle_ >= kIntakeThroatClearAngle;

    // "In the dog's way" means overlapping the quarter-disc its plate
    // sweeps: within the swing radius of the hinge, and low enough that
    // the plate would strike it.
    const JPH::RVec3 pack = bodies.GetPosition(intake_pack_id_);
    const float pack_dx = static_cast<float>(pack.GetX()) - kIntakeDogHingeX;
    const float pack_dz = static_cast<float>(pack.GetZ()) - kIntakeBayFrontZ;
    const float swing_radius = 2.0F * kIntakeDogHalfWidth + kIntakePackHalfZ;
    intake_pack_pins_dog_ =
        (pack_dx * pack_dx + pack_dz * pack_dz) <= (swing_radius * swing_radius) &&
        static_cast<float>(pack.GetY()) - kIntakePackHalfY < kIntakeThroatHeight;
}

void Simulation::PhysicsWorld::update_legal_forty(const JPH::BodyInterface &bodies,
                        const bool release_requested,
                        const bool attach_requested) noexcept {
    if (intake_swing_hinge_ != nullptr) {
        const float measured = intake_swing_hinge_->GetCurrentAngle();
        if (std::isfinite(measured)) {
            // Jolt reports this NEGATIVE as the flight opens -- see
            // kIntakeSwingHingeTravelRadians -- so travel is its negation.
            intake_swing_travel_ =
                std::clamp(-measured, 0.0F, kIntakeSwingHingeTravelRadians);
        }
    }

    if (!intake_station_active_) {
        return;
    }
    if (release_requested && intake_pack_slung_) {
        release_pack_to_cradle(bodies);
    }
    if (attach_requested && !intake_pack_slung_) {
        sling_pack(bodies);
    }
}

bool Simulation::PhysicsWorld::intake_pack_seated_on_cradle(const JPH::BodyInterface &bodies) const noexcept {
    const JPH::RVec3 pack_position = bodies.GetPosition(intake_pack_id_);
    const JPH::RVec3 cradle_position = bodies.GetPosition(intake_cw_cradle_id_);
    // The pack's CENTRE when resting on the cradle floor sits one pack
    // half-height above the cradle's own top face -- not at the top face
    // itself. Found by direct observation: comparing pack.y against the
    // bare top face read a settled, correctly-resting pack as 0.96 m out
    // of tolerance.
    const JPH::RVec3 cradle_seat(
        cradle_position.GetX(),
        cradle_position.GetY() + kIntakeCwCradleHalfY + kIntakePackHalfY,
        cradle_position.GetZ());
    const auto dx = static_cast<float>(pack_position.GetX() - cradle_seat.GetX());
    const auto dz = static_cast<float>(pack_position.GetZ() - cradle_seat.GetZ());
    const auto dy = static_cast<float>(pack_position.GetY() - cradle_seat.GetY());
    const bool xz_in_tolerance =
        (dx * dx + dz * dz) <=
        (kLegalFortyReleaseXZToleranceMeters * kLegalFortyReleaseXZToleranceMeters);
    const bool y_in_tolerance = std::fabs(dy) <= kLegalFortyReleaseYToleranceMeters;
    return xz_in_tolerance && y_in_tolerance;
}

void Simulation::PhysicsWorld::release_pack_to_cradle(const JPH::BodyInterface &bodies) noexcept {
    const bool settled = bodies.GetLinearVelocity(intake_pack_id_).Length() <=
                         kLegalFortyReleaseSpeedToleranceMps;
    if (!intake_pack_seated_on_cradle(bodies) || !settled) {
        return;
    }
    remove_sling_pin();
}

void Simulation::PhysicsWorld::sling_pack(const JPH::BodyInterface &bodies) noexcept {
    const JPH::RVec3 hook_position = bodies.GetPosition(intake_hook_id_);
    const JPH::RVec3 pack_position = bodies.GetPosition(intake_pack_id_);
    const JPH::RVec3 padeye(pack_position.GetX(), pack_position.GetY() + kIntakePackHalfY,
                            pack_position.GetZ());
    const auto distance = static_cast<float>(JPH::Vec3(hook_position - padeye).Length());
    const bool in_tolerance = distance <= kLegalFortyAttachToleranceMeters;
    const bool settled = bodies.GetLinearVelocity(intake_hook_id_).Length() <=
                         kLegalFortyAttachSpeedToleranceMps;
    if (!in_tolerance || !settled) {
        return;
    }
    create_sling_pin(hook_position);
}

void Simulation::PhysicsWorld::create_sling_pin(const JPH::RVec3 point) noexcept {
    JPH::PointConstraintSettings sling_settings;
    sling_settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    sling_settings.mPoint1 = point;
    sling_settings.mPoint2 = point;
    intake_sling_pin_ = static_cast<JPH::PointConstraint *>(
        create_constraint(sling_settings, intake_hook_id_, intake_pack_id_, false));
    intake_pack_slung_ = true;
}

void Simulation::PhysicsWorld::remove_sling_pin() noexcept {
    if (intake_sling_pin_ != nullptr) {
        physics_system_.RemoveConstraint(intake_sling_pin_);
        intake_sling_pin_ = nullptr;
    }
    intake_pack_slung_ = false;
}

void Simulation::PhysicsWorld::restore_legal_forty_topology(const bool checkpoint_slung) noexcept {
    if (checkpoint_slung && !intake_pack_slung_) {
        create_sling_pin(physics_system_.GetBodyInterface().GetPosition(intake_hook_id_));
    } else if (!checkpoint_slung && intake_pack_slung_) {
        remove_sling_pin();
    }
}

void Simulation::PhysicsWorld::update_sump(JPH::BodyInterface &bodies, const float delta_seconds,
                 const bool valve_toggle_requested) noexcept {
    const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
    const float station_dx = static_cast<float>(player_position.GetX()) - kSumpStationX;
    const float station_dz = static_cast<float>(player_position.GetZ()) - kSumpStationZ;
    const bool at_station = (station_dx * station_dx + station_dz * station_dz) <=
                            (kSumpStationRadius * kSumpStationRadius);
    sump_station_active_ = at_station;

    if (valve_toggle_requested && at_station) {
        sump_isolated_ = !sump_isolated_;
    }

    const float inflow = sump_isolated_ ? 0.0F : kSumpInflowKgPerSec;
    sump_volume_kg_ += (inflow - kSumpDrainKgPerSec) * delta_seconds;
    sump_volume_kg_ = std::clamp(sump_volume_kg_, 0.0F, kSumpCapacityKg);

    const bool grate_safe = sump_volume_kg_ <= 0.0F;
    // Set every tick, not just on transition: a plain bool flag, cheap to
    // reassert, and it removes any chance of a missed-edge desync between
    // grate_safe_ and the body's actual sensor state.
    bodies.SetIsSensor(sump_grate_id_, !grate_safe);
    grate_safe_ = grate_safe;
}

} // namespace scraperx::sim
