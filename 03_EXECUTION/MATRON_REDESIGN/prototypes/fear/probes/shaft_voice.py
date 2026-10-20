"""Where her voice should seem to come from when she is on another level.

A port of Matron::carry_m's two ways (src/sim/matron.cpp:274-297) with the
constants of src/sim/matron.hpp:148-151 (kDeckMuffle 10, kBendMuffle 1.4,
kShaftHalfM 17, kMidZ -150). The deck count is given here (the sim casts a
ray); everything else is the same arithmetic. When the way round by the open
shaft is the shorter, the presentation should put her voice's emitter at the
shaft's edge nearest the listener (the sim's `in`), not at her body under the
steel: the voice then rises out of the shaft beside the player.
"""
import math

DECK_MUFFLE, BEND_MUFFLE, SHAFT, MID_Z = 10.0, 1.4, 17.0, -150.0
FULL_HEARING_M = 55.0   # matron.cpp:30


def in_shaft(p):
    x, y, z = p
    return (max(-SHAFT, min(SHAFT, x)), y, max(MID_Z - SHAFT, min(MID_Z + SHAFT, z)))


def dist(a, b):
    return math.dist(a, b)


def carry(src, ear, decks):
    straight = dist(src, ear)
    through = straight * DECK_MUFFLE ** decks
    out, inn = in_shaft(src), in_shaft(ear)
    to_shaft, from_shaft = dist(src, out), dist(ear, inn)
    rnd = to_shaft + dist(out, inn) + from_shaft
    rnd *= BEND_MUFFLE if to_shaft > 0.5 else 1.0
    rnd *= BEND_MUFFLE if from_shaft > 0.5 else 1.0
    if decks == 0:
        return straight, "straight", src
    if through <= rnd:
        return through, "through the deck", src
    return rnd, "round by the shaft", inn


cases = [
    # (label, her mouth, player's ear, decks between)
    ("she 1 deck under you, same band spot", (21.5, 143.0 + 1.5, -150.0), (21.5, 154.0 + 1.6, -150.0), 1),
    ("she 1 deck under you, across the shaft", (-21.5, 143.0 + 1.5, -150.0), (21.5, 154.0 + 1.6, -150.0), 1),
    ("she 2 decks under you (stack pitch 11 m)", (21.5, 132.0 + 1.5, -128.5), (21.5, 154.0 + 1.6, -128.5), 2),
    ("she on 154, you on the 176 ring above", (21.5, 154.0 + 1.5, -171.5), (23.09, 176.25 + 1.6, -171.5), 1),
    ("she on 154, you on 220 (3 decks up)", (11.0, 154.0 + 1.5, -128.5), (21.0, 220.25 + 1.6, -150.0), 3),
    ("she at the locker grille, you 2 m off it", (11.0, 155.6, -126.7), (11.0, 155.6, -128.7), 0),
]
for label, her, you, decks in cases:
    m, way, emitter = carry(her, you, decks)
    level = max(0.0, 1.0 - m / FULL_HEARING_M)   # a hum or murmur at loudness 1
    print("%-46s carried %6.1f m  %-18s emitter at (%.1f, %.1f, %.1f)  level at ear %.2f (of a 55 m sound)"
          % (label, m, way, emitter[0], emitter[1], emitter[2], level))
