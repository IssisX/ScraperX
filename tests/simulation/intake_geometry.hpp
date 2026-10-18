#pragma once

// Existing test geometry shared by intake, Legal Forty and the loaded Hook5 route.
// Waypoint literals remain independent of the production geometry definitions.
namespace scraperx::tests {

constexpr double kThroatX = 0.0;
// The landing centre clears the flight's end rail before turning onto it.
constexpr double kStairLandingX = 8.0;
constexpr double kHandoffSurfaceY = 24.19;
constexpr double kLegalFortyHallDeckSurfaceY = 40.1872;
constexpr double kLegalFortyHingeX = 7.856;
constexpr double kLegalFortyHingeZ = -112.5;
constexpr double kLegalFortyMidLandingX = 9.350;
constexpr double kLegalFortyMidLandingZ = -115.80;
constexpr double kLegalFortyWellX = -4.51;
constexpr double kLegalFortyUpperFlightZ = -117.0;

} // namespace scraperx::tests
