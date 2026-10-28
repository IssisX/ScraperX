#pragma once

// Her contacts as the simulation exports them (MatronSnapshot contact_*,
// src/sim/matron_contact.cpp; issue #3, S6 "one contact truth"), checked each
// tick of a native run against the rest of what the simulation says. The
// presentation plants a limb, and rides or lifts her body onto a floor, only
// on these, so they must be her real floor and her real grip, this tick:
//  - never a second grip (none of her movers holds two);
//  - her floor where her soles can be on it (under her middle or her feet's
//    span, no lower than her legs reach);
//  - her grip within her reach, and where the mover that has her shows its
//    hand (on a hold that does not move, the same point);
// Counted, not failed: at rest with no grip and no floor exported (a body
// holding her up that walk_toward does not stand on), and, during a
// two-handed move, the old inferences the view drew from before S6 (a second
// hand the simulation does not hold, a floor under her feet it does not have).

#include "sim_tests.hpp"
#include "sim/matron_traversal.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

struct MatronContactAudit {
    long ticks = 0;
    long supported = 0;
    long held = 0;
    long two_handed = 0;
    long invented_hand2 = 0;    // the old climb_hand2_known, a second hold the simulation does not hold
    long invented_feet = 0;     // the old climb_feet_support_known with no floor under her
    long second_grip = 0;       // FAIL: a second grip exported
    long floor_off_her = 0;     // FAIL: the exported floor not one her soles can be on
    long grip_out_of_reach = 0; // FAIL: the exported grip out of her reach
    long grip_off_mover = 0;    // FAIL: the exported grip not where the mover shows her hand
    long resting_unowned = 0;   // counted: still, holding nothing, no floor exported
    double worst_grip_mover_m = 0.0;
    int still_ticks = 0;
    bool have_last = false;
    scraperx::sim::Vector3 last{};

    // Her floor: walk_toward's own rays (Matron::sole_contact) start at her
    // middle and at kFootSpanM to each side of it, kStandCentre + 0.3 m long.
    static constexpr double kStandCenterM = 1.05;
    static constexpr double kRayM = kStandCenterM + 0.3;
    static constexpr double kFootSpanM = 0.24;
    static constexpr double kReachM = 2.5;      // her arm out from her middle, with room for a grip on her pipe
    static constexpr double kMoverHandM = 0.05;

    void tick(const scraperx::sim::MatronSnapshot &m) {
        using scraperx::sim::traversal::MoveKind;
        ++ticks;
        const MoveKind k = m.climb_move;
        const bool two = m.climb_move_active && (k == MoveKind::Mantle || k == MoveKind::JumpCatch ||
                                                 k == MoveKind::PoleUp || k == MoveKind::PoleDown ||
                                                 k == MoveKind::HangDrop);
        if (two) {
            ++two_handed;
            if (m.climb_hand2_known) ++invented_hand2;
            if (m.climb_feet_support_known && !m.contact_supported) ++invented_feet;
        }
        if (m.contact_hand2_held) ++second_grip;
        if (m.contact_supported) {
            ++supported;
            const double below = m.position.y - m.contact_feet.y;
            const double across = std::hypot(m.position.x - m.contact_feet.x, m.position.z - m.contact_feet.z);
            if (below < -0.02 || below > kRayM + 0.02 || across > kFootSpanM + 0.02) ++floor_off_her;
        }
        if (m.contact_hand_held) {
            ++held;
            const double reach = std::hypot(m.contact_hand.x - m.position.x, m.contact_hand.y - m.position.y,
                                            m.contact_hand.z - m.position.z);
            if (reach > kReachM) ++grip_out_of_reach;
            if (m.climbing && !m.on_pipe) {
                const double d = std::hypot(m.contact_hand.x - m.climb_hand.x, m.contact_hand.y - m.climb_hand.y,
                                            m.contact_hand.z - m.climb_hand.z);
                worst_grip_mover_m = std::max(worst_grip_mover_m, d);
                if (d > kMoverHandM) ++grip_off_mover;
            }
        }
        const bool still = have_last && std::hypot(m.position.x - last.x, m.position.y - last.y, m.position.z - last.z) < 0.001;
        still_ticks = still ? still_ticks + 1 : 0;
        if (still_ticks >= 30 && m.alive && !m.contact_hand_held && !m.contact_line && !m.contact_supported) ++resting_unowned;
        last = m.position;
        have_last = true;
    }

    [[nodiscard]] bool clean() const {
        return second_grip == 0 && floor_off_her == 0 && grip_out_of_reach == 0 && grip_off_mover == 0;
    }

    void report(const char *name) const {
        std::cout << "    contacts (" << name << "): " << ticks << " ticks, on a floor " << supported << ", in a grip " << held
                  << ", two-handed moves " << two_handed << " (the old view's invented second hand " << invented_hand2
                  << ", invented floor " << invented_feet << "); second grips " << second_grip << ", floor off her "
                  << floor_off_her << ", grip out of reach " << grip_out_of_reach << ", grip off the mover's hand "
                  << grip_off_mover << " (worst " << worst_grip_mover_m << " m), still with no floor or grip "
                  << resting_unowned << '\n';
    }
};
