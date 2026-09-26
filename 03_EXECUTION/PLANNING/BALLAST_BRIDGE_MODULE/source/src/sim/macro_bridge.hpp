#pragma once

#include "sim/mechanism_kit.hpp"

namespace scraperx::sim::macro {

// AS-016: one gravity-powered, permanently useful route from grade to deck 1.
// Coordinates and parts are authored once, here/native, including the receiver.
struct Bridge final {
    static constexpr std::uint64_t kFrame = 1016;
    static constexpr std::uint64_t kDeck = 2016;
    static constexpr std::uint64_t kBallast = 2017;
    static constexpr std::uint64_t kHandle = 2018;
    static constexpr std::uint64_t kBrakeArm = 2019;
    static constexpr float kAngle = 0.335421936F; // top of nose = 10.75 m
    static constexpr float kDrop = 9.168216F;
    kit::BodyIndex frame, deck, ballast, handle, brake_arm;
    kit::LeverIndex deck_hinge, handle_hinge;
    kit::GuideIndex ballast_guide;
    kit::RopeIndex rope;
    double initial_deck_com_y = 0;
    void build(kit::Kit &kit);
    void pre_step(kit::Kit &kit) const;
};

} // namespace scraperx::sim::macro
