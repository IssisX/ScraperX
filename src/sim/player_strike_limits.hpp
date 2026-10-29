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

} // namespace scraperx::sim::strike
