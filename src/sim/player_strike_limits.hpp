#pragma once

// The player's strike: what his arm can put into a body it lands on. These
// are his limits, the way traversal::Limits are hers. A strike is a force
// held over the contact ticks (force times time), never a velocity written.
//
// DERIVED: a trained adult's straight punch lands ~2.5 to 4 kN peak for
// ~10 ms; spread over the 3 ticks (1/30 s) it is drawn here, ~1.5 kN gives
// ~50 N*s, the measured range for a punch with the body behind it. Power is
// that force against the bodies' closing speed, capped as a human's arm and
// trunk can deliver for that moment.
namespace scraperx::sim::strike {

constexpr float kPlayerStrikeForceN = 1500.0F;
constexpr float kPlayerStrikePowerW = 3000.0F;

// The low sweep (physics_world_strike.cpp): Punch while crouched. A leg
// driven through the ground, not an arm: its force is the spec's 1350 N
// flat (no closing-speed power cap), held 4 ticks for 60 N*s, which over
// her 220 kg is 0.273 m/s. Low, so her balance zeroes its reach and no
// brace holds against it: it downs any upright Matron at any stage.
constexpr float kPlayerSweepForceN = 1350.0F;

} // namespace scraperx::sim::strike
