# Applied by FetchContent to the pinned Jolt checkout (CMakeLists.txt, PATCH_COMMAND).
#
# IndependentAxisConstraintPart, the pulley constraint's solver part, writes its lever-arm cross
# products only for a body that is not static, yet SolveVelocityConstraint reads them for both
# bodies (r1 x n1 . w1, with w1 = 0 for a static body). Left as allocation garbage, a NaN or Inf
# there makes 0 * NaN = NaN: a rope made fast to the world tore its load to NaN the first time it
# went taut, or not, depending on what the allocator's reused memory held. Zero is the value the
# solver needs for a static body.
#
# Idempotent: a patched tree is left as it is. A tree with neither the pinned nor the patched text
# stops the configure, so a Jolt bump re-checks this instead of silently dropping it.

if(NOT DEFINED JOLT_SOURCE_DIR)
    message(FATAL_ERROR "jolt_patch.cmake: pass -DJOLT_SOURCE_DIR=<the Jolt checkout>")
endif()

set(part "${JOLT_SOURCE_DIR}/Jolt/Physics/Constraints/ConstraintPart/IndependentAxisConstraintPart.h")
file(READ "${part}" text)
set(pinned
    "\tVec3\t\t\t\t\t\tmR1xN1;\n\tVec3\t\t\t\t\t\tmInvI1_R1xN1;\n\tVec3\t\t\t\t\t\tmRatioR2xN2;\n\tVec3\t\t\t\t\t\tmInvI2_RatioR2xN2;\n")
set(patched
    "\tVec3\t\t\t\t\t\tmR1xN1 = Vec3::sZero();\n\tVec3\t\t\t\t\t\tmInvI1_R1xN1 = Vec3::sZero();\n\tVec3\t\t\t\t\t\tmRatioR2xN2 = Vec3::sZero();\n\tVec3\t\t\t\t\t\tmInvI2_RatioR2xN2 = Vec3::sZero();\n")

string(FIND "${text}" "${patched}" at_patched)
if(at_patched GREATER_EQUAL 0)
    return()
endif()
string(FIND "${text}" "${pinned}" at_pinned)
if(at_pinned LESS 0)
    message(FATAL_ERROR "jolt_patch.cmake: ${part} no longer holds the pinned lever-arm members; "
                        "re-check that a static body's lever arms are written before bumping Jolt")
endif()
string(REPLACE "${pinned}" "${patched}" text "${text}")
file(WRITE "${part}" "${text}")
message(STATUS "jolt_patch.cmake: IndependentAxisConstraintPart lever arms initialised")
