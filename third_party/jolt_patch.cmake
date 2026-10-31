# Applied by FetchContent to the pinned Jolt checkout (CMakeLists.txt, PATCH_COMMAND). Four patches,
# each idempotent on its own file: a patched tree is left as it is, and a tree with neither the
# pinned nor the patched text stops the configure, so a Jolt bump re-checks each instead of
# silently dropping it.
#
# 1. IndependentAxisConstraintPart, the pulley constraint's solver part, writes its lever-arm cross
# products only for a body that is not static, yet SolveVelocityConstraint reads them for both
# bodies (r1 x n1 . w1, with w1 = 0 for a static body). Left as allocation garbage, a NaN or Inf
# there makes 0 * NaN = NaN: a rope made fast to the world tore its load to NaN the first time it
# went taut, or not, depending on what the allocator's reused memory held. Zero is the value the
# solver needs for a static body.
#
#
# 2. ContactConstraintManager (below): the key an island's contacts are sorted by for the solver.

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
if(at_patched LESS 0)
    string(FIND "${text}" "${pinned}" at_pinned)
    if(at_pinned LESS 0)
        message(FATAL_ERROR "jolt_patch.cmake: ${part} no longer holds the pinned lever-arm members; "
                            "re-check that a static body's lever arms are written before bumping Jolt")
    endif()
    string(REPLACE "${pinned}" "${patched}" text "${text}")
    file(WRITE "${part}" "${text}")
    message(STATUS "jolt_patch.cmake: IndependentAxisConstraintPart lever arms initialised")
endif()

# ---------------------------------------------------------------------------------------------
# 3. CCD's secondary AABB prune must sweep the shape bounds center. A shape
# with an offset center of mass has the same bounds extent but a different
# center; sweeping its COM can reject an imminent collision before casting.
# Run before the contact-sort cache guard below so restored caches receive it.
set(system "${JOLT_SOURCE_DIR}/Jolt/Physics/PhysicsSystem.cpp")
file(READ "${system}" text)
set(ccd_pinned "RayAABox(Vec3(mShapeCast.mCenterOfMassStart.GetTranslation()), RayInvDirection(direction), bounds.mMin, bounds.mMax)")
set(ccd_patched "RayAABox(mShapeCast.mShapeWorldBounds.GetCenter(), RayInvDirection(direction), bounds.mMin, bounds.mMax)")
string(FIND "${text}" "${ccd_patched}" at_ccd_patched)
if(at_ccd_patched LESS 0)
    string(FIND "${text}" "${ccd_pinned}" at_ccd_pinned)
    if(at_ccd_pinned LESS 0)
        message(FATAL_ERROR "jolt_patch.cmake: CCD AABB prune no longer matches the pinned source; re-check the shape bounds center")
    endif()
    string(REPLACE "${ccd_pinned}" "${ccd_patched}" text "${text}")
    file(WRITE "${system}" "${text}")
    message(STATUS "jolt_patch.cmake: CCD AABB prune follows the shape bounds center")
endif()

# ---------------------------------------------------------------------------------------------
# 4. Only the Matron needs the earlier CCD activation: the canonical 0.75
# inner-radius trigger is necessary for the incline trolley to throw its rider.
# The Matron's own 0.15 m single-tick steel limit needs 0.15 / 0.30 = 0.5.
# Read her authoritative entity id and limits instead of copying constants
# into Jolt; the injected branch applies only to her tagged dynamic body.
set(matron_header "${CMAKE_CURRENT_LIST_DIR}/../src/sim/matron.hpp")
file(READ "${matron_header}" matron_text)
string(REGEX MATCH "static constexpr std::uint64_t kEntity = ([0-9]+);" matron_match "${matron_text}")
if(NOT matron_match)
    message(FATAL_ERROR "jolt_patch.cmake: cannot find Matron::kEntity for actor-local CCD")
endif()
set(matron_entity "${CMAKE_MATCH_1}")
string(REGEX MATCH "static constexpr float kRadius = ([0-9.]+)F;" matron_match "${matron_text}")
if(NOT matron_match)
    message(FATAL_ERROR "jolt_patch.cmake: cannot find Matron::kRadius for actor-local CCD")
endif()
set(matron_radius "${CMAKE_MATCH_1}")
string(REGEX MATCH "static constexpr float kTunnelM = ([0-9.]+)F;" matron_match "${matron_text}")
if(NOT matron_match)
    message(FATAL_ERROR "jolt_patch.cmake: cannot find Matron::kTunnelM for actor-local CCD")
endif()
set(matron_tunnel "${CMAKE_MATCH_1}")
set(ccd_actor_pinned "float linear_cast_threshold_sq = Square(mPhysicsSettings.mLinearCastThreshold * inner_radius);")
set(ccd_actor_patched
    "float linear_cast_threshold = body.GetUserData() == ${matron_entity} ? min(mPhysicsSettings.mLinearCastThreshold, ${matron_tunnel}f / ${matron_radius}f) : mPhysicsSettings.mLinearCastThreshold;\n\t\t\t\t\tfloat linear_cast_threshold_sq = Square(linear_cast_threshold * inner_radius);")
string(FIND "${text}" "${ccd_actor_patched}" at_ccd_actor_patched)
if(at_ccd_actor_patched LESS 0)
    string(FIND "${text}" "${ccd_actor_pinned}" at_ccd_actor_pinned)
    if(at_ccd_actor_pinned LESS 0)
        # A cached source may contain values from an earlier Matron header.
        set(ccd_actor_previous "float linear_cast_threshold = body.GetUserData[(][)] == [0-9]+ [?] min[(]mPhysicsSettings[.]mLinearCastThreshold, [0-9.]+f / [0-9.]+f[)] : mPhysicsSettings[.]mLinearCastThreshold;")
        string(REGEX MATCH "${ccd_actor_previous}" previous_match "${text}")
        if(NOT previous_match)
            message(FATAL_ERROR "jolt_patch.cmake: actor CCD threshold no longer matches pinned Jolt; inspect the integration seam")
        endif()
        set(ccd_actor_line "float linear_cast_threshold = body.GetUserData() == ${matron_entity} ? min(mPhysicsSettings.mLinearCastThreshold, ${matron_tunnel}f / ${matron_radius}f) : mPhysicsSettings.mLinearCastThreshold;")
        string(REPLACE "${previous_match}" "${ccd_actor_line}" text "${text}")
    else()
        string(REPLACE "${ccd_actor_pinned}" "${ccd_actor_patched}" text "${text}")
    endif()
    file(WRITE "${system}" "${text}")
    message(STATUS "jolt_patch.cmake: Matron-only CCD activation, canonical trigger for other bodies")
endif()

# ---------------------------------------------------------------------------------------------
# 2. Contact solve order independent of body ids (03_EXECUTION/ENGINEERING/BODY_ORDER_STABILITY.md).
#
# ContactConstraintManager sorts an island's contact constraints, before the solver runs them, by
# a byte hash of (body 1 id, sub-shape 1, body 2 id, sub-shape 2). A body created anywhere in the
# world shifts the id of every body created after it, the hashes of their contacts change, and
# the solver runs the contacts of islands the new body never touches in another order: the same
# physics summed in another order, different in the last bits, and from there the run drifts.
# The key here is built from what the bodies are (their user data: the game's entity ids) and the
# sub-shapes in contact; SortContacts already breaks ties on the body ids, whose relative order a
# new body never changes.

set(manager "${JOLT_SOURCE_DIR}/Jolt/Physics/Constraints/ContactConstraintManager.cpp")
file(READ "${manager}" text)
string(FIND "${text}" "sStableContactSortKey" at_patched)
if(at_patched GREATER_EQUAL 0)
    return()
endif()
set(pinned_namespace "JPH_NAMESPACE_BEGIN\n")
set(patched_namespace
    "JPH_NAMESPACE_BEGIN\n\n// ScraperX (third_party/jolt_patch.cmake): the key an island's contacts are sorted by before the\n// solver runs them. Upstream hashes the two body ids, so a body created anywhere in the world\n// reorders the solve of islands it never touches. This key is what the bodies are (their user\n// data) and the sub-shapes in contact; SortContacts breaks ties on the body ids, whose relative\n// order a new body never changes.\nstatic JPH_INLINE uint64 sStableContactSortKey(const Body &inBody1, const SubShapeID &inSubShapeID1, const Body &inBody2, const SubShapeID &inSubShapeID2)\n{\n\tconst uint64 words[4] = { inBody1.GetUserData(), uint64(inSubShapeID1.GetValue()), inBody2.GetUserData(), uint64(inSubShapeID2.GetValue()) };\n\treturn HashBytes(words, sizeof(words));\n}\n")
set(pinned_add
    "CreateConstraint<Type1, Type2>(ioActivateAndLinkBodies, inBody1, inBody2, key_hash, new_manifold_handle,")
set(patched_add
    "CreateConstraint<Type1, Type2>(ioActivateAndLinkBodies, inBody1, inBody2, sStableContactSortKey(inBody1, inManifold.mSubShapeID1, inBody2, inManifold.mSubShapeID2), new_manifold_handle,")
set(pinned_cache
    "CreateConstraint<Type1, Type2>(link_bodies, inBody1, inBody2, input_hash, output_handle,")
set(patched_cache
    "CreateConstraint<Type1, Type2>(link_bodies, inBody1, inBody2, sStableContactSortKey(inBody1, input_key.GetSubShapeID1(), inBody2, input_key.GetSubShapeID2()), output_handle,")

foreach(pinned IN ITEMS pinned_namespace pinned_add pinned_cache)
    string(FIND "${text}" "${${pinned}}" at_pinned)
    if(at_pinned LESS 0)
        message(FATAL_ERROR "jolt_patch.cmake: ${manager} no longer holds the pinned contact sort key text (${pinned}); "
                            "re-check how contacts are sorted for the solver before bumping Jolt")
    endif()
endforeach()
string(REPLACE "${pinned_namespace}" "${patched_namespace}" text "${text}")
string(REPLACE "${pinned_add}" "${patched_add}" text "${text}")
string(REPLACE "${pinned_cache}" "${patched_cache}" text "${text}")
file(WRITE "${manager}" "${text}")
message(STATUS "jolt_patch.cmake: contact solve order keyed on the bodies' user data, not their ids")
