cmake_minimum_required(VERSION 3.20)

project(ScraperX VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SCRAPERX_BUILD_BRIDGE "Build the Godot GDExtension bridge" ON)
option(SCRAPERX_WITH_JOLT "Compile the pinned Jolt substrate into the native graph" ON)

set(SCRAPERX_GODOT_CPP_COMMIT "507ed9d840c01a3c5b2a39af8bb4000bfac30bf5" CACHE STRING "Pinned godot-cpp commit")
set(SCRAPERX_JOLT_COMMIT "e77f175595e64cb44218cc9d9d56fc365ad0e36a" CACHE STRING "Pinned Jolt commit")
# Monocypher 4.0.3: Ed25519 and SHA-512 for live fabrication's delivery check.
set(SCRAPERX_MONOCYPHER_COMMIT "ab2b16dd619ad5f6979a4fbe69cfa324a6fcc35f" CACHE STRING "Pinned Monocypher commit")

add_library(scraperx_sim STATIC
    src/sim/simulation.cpp
    src/sim/physics_world.cpp
    src/sim/physics_world_probes.cpp
    src/sim/physics_world_movement.cpp
    src/sim/physics_world_water.cpp
    src/sim/physics_world_grab.cpp
    src/sim/physics_world_strike.cpp
    src/sim/physics_world_arm.cpp
    src/sim/matron_hold.cpp
    src/sim/matron_fatality.cpp
    src/sim/physics_world_fatality.cpp
    src/sim/physics_world_one_arm.cpp
    src/sim/matron_moves.cpp
    src/sim/matron_travel.cpp
    src/sim/matron_pipe.cpp
    src/sim/matron_hoist.cpp
    src/sim/matron_line.cpp
    src/sim/matron_climb.cpp
    src/sim/physics_world_traversal.cpp
    src/sim/physics_world_climb.cpp
    src/sim/physics_world_state.cpp
    src/sim/stock_damage.cpp
    src/sim/mechanism_kit.cpp
    src/sim/rope_constraint.cpp
    src/sim/slingshot.cpp
    src/sim/slingshot_model.cpp
    src/sim/swing.cpp
    src/sim/trapeze.cpp
    src/sim/twin_tower.cpp
    src/sim/climb_c6.cpp
    src/sim/pipe_gallery.cpp
    src/sim/crew.cpp
    src/sim/climb_c7.cpp
    src/sim/summit.cpp
    src/sim/shared_survival.cpp
    src/sim/bay.cpp
    src/sim/matron_routecraft.cpp
    src/sim/matron_found_climb.cpp
    src/sim/matron_contact.cpp
    src/sim/matron_shape.cpp
    src/sim/matron_stalk.cpp
    src/sim/matron_perception.cpp
    src/sim/matron.cpp
    src/sim/matron_traversal.cpp
    src/sim/matron_journey.cpp
    src/sim/loose_stock.cpp
    src/sim/fabricated.cpp
    src/sim/physics_world_fabricated.cpp
    src/sim/impacts.cpp
    src/sim/forklift.cpp
    src/sim/yard_crane.cpp
    src/sim/zipline.cpp
    src/sim/corpse.cpp
    src/sim/band_counterweight_well.cpp
    src/sim/band_wet_isolation.cpp
    src/sim/band_plate_shop.cpp
    src/sim/band_facade_crane.cpp
    src/sim/band_midstack_service.cpp
    src/sim/band_stack.cpp
    src/sim/vertical/vertical_machine.cpp
    src/sim/vertical/vertical_route.cpp
    src/sim/vertical/gravity_balance.cpp
    src/sim/vertical/traction_tram.cpp
    src/sim/vertical/rack_climber.cpp
    src/sim/vertical/crown_gondola.cpp
    src/sim/vertical/barrel_helix.cpp
    src/sim/vertical/cascade_mast.cpp
    src/sim/vertical/luffing_derrick.cpp
    src/sim/vertical/pitman_lift.cpp
    src/sim/vertical/slab_incline.cpp
    src/sim/vertical/stone_wheel.cpp
)
target_include_directories(scraperx_sim PUBLIC src)
# A Jolt subclass (the rope constraint, the forklift's vehicle controller)
# must be built as Jolt is, without RTTI: Jolt's base classes have no typeinfo
# for a derived one's to name.
set_source_files_properties(src/sim/rope_constraint.cpp src/sim/forklift.cpp PROPERTIES COMPILE_OPTIONS
    "$<IF:$<CXX_COMPILER_ID:MSVC>,/GR-,-fno-rtti>")
set_target_properties(scraperx_sim PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(scraperx_sim PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wpedantic>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>
)

if(SCRAPERX_WITH_JOLT)
    set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
    set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
    set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
    set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
    set(JPH_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
    set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
    set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
    set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
    set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
    set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        jolt
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG ${SCRAPERX_JOLT_COMMIT}
        GIT_SHALLOW FALSE
        SOURCE_SUBDIR Build
        PATCH_COMMAND ${CMAKE_COMMAND} -DJOLT_SOURCE_DIR=<SOURCE_DIR>
                      -P ${CMAKE_CURRENT_SOURCE_DIR}/third_party/jolt_patch.cmake
    )
    FetchContent_MakeAvailable(jolt)
    # Refresh the cached Jolt patch when the authoritative actor constants change.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/sim/matron.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/jolt_patch.cmake")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -DJOLT_SOURCE_DIR=${jolt_SOURCE_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/third_party/jolt_patch.cmake
        COMMAND_ERROR_IS_FATAL ANY
    )
    set_target_properties(Jolt PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(scraperx_sim PUBLIC Jolt)
    target_compile_definitions(scraperx_sim PUBLIC SCRAPERX_HAS_JOLT=1)
endif()

# Live fabrication's delivery check (src/bridge/fabricated_delivery.hpp): the
# game layer's Ed25519 signature and SHA-512 checks, on the pinned Monocypher
# (C, so C is enabled for it alone), against the public key in
# tools/fab/dev_public_key.hex, compiled in. Linked by the bridge and by the
# native tests, which check it as the bridge runs it.
enable_language(C)
FetchContent_Declare(
    monocypher
    GIT_REPOSITORY https://github.com/LoupVaillant/Monocypher.git
    GIT_TAG ${SCRAPERX_MONOCYPHER_COMMIT}
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(monocypher)
set(SCRAPERX_FAB_PUBLIC_KEY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/tools/fab/dev_public_key.hex")
file(READ "${SCRAPERX_FAB_PUBLIC_KEY_FILE}" SCRAPERX_FAB_PUBLIC_KEY)
string(STRIP "${SCRAPERX_FAB_PUBLIC_KEY}" SCRAPERX_FAB_PUBLIC_KEY)
if(NOT SCRAPERX_FAB_PUBLIC_KEY MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "tools/fab/dev_public_key.hex must hold one Ed25519 public key in lowercase hex")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SCRAPERX_FAB_PUBLIC_KEY_FILE}")
add_library(scraperx_delivery STATIC
    src/bridge/fabricated_delivery.cpp
    ${monocypher_SOURCE_DIR}/src/monocypher.c
    ${monocypher_SOURCE_DIR}/src/optional/monocypher-ed25519.c
)
target_include_directories(scraperx_delivery PUBLIC src
    PRIVATE ${monocypher_SOURCE_DIR}/src ${monocypher_SOURCE_DIR}/src/optional)
target_compile_definitions(scraperx_delivery PRIVATE SCRAPERX_FAB_PUBLIC_KEY="${SCRAPERX_FAB_PUBLIC_KEY}")
set_target_properties(scraperx_delivery PROPERTIES POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)

option(SCRAPERX_BUILD_TESTS "Build simulation tests regardless of platform" OFF)

if((BUILD_TESTING AND NOT ANDROID) OR SCRAPERX_BUILD_TESTS)
    add_executable(scraperx_shared_survival_tests tests/shared_survival_tests.cpp)
    target_link_libraries(scraperx_shared_survival_tests PRIVATE scraperx_sim)
    target_compile_options(scraperx_shared_survival_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)
    add_test(NAME scraperx_sim.shared_survival COMMAND scraperx_shared_survival_tests)

    # The native test files: every tests/sim_tests_*.cpp, found by the glob, so a new
    # slice file edits no list here (J-074, #352). Each group registers itself beside
    # its function (tests/sim_registry.hpp); simulation_tests.cpp runs the registry.
    file(GLOB SCRAPERX_SIM_TEST_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/sim_tests_*.cpp")
    add_executable(scraperx_sim_tests
        tests/simulation_tests.cpp
        tests/matron_exam.cpp
        ${SCRAPERX_SIM_TEST_FILES}
    )
    add_dependencies(scraperx_sim_tests scraperx_shared_survival_tests)
    target_link_libraries(scraperx_sim_tests PRIVATE scraperx_sim scraperx_delivery)
    # Where the fabricated test objects are read from (godot/tests/fabricated),
    # whatever directory the tests run in.
    target_compile_definitions(scraperx_sim_tests PRIVATE SCRAPERX_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(scraperx_sim_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wpedantic>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>
    )
    # J-071 step 2: the default sequence is two SCRAPERX_SLICE CTest tests
    # (athletic-traversal-1 / athletic-traversal-2 in tests/sim_tests_athletic_shards.cpp),
    # each under about 900 s. The bare binary still runs every group once, with TIME lines.
    # Each SCRAPERX_SLICE("<name>", ...) in those files is also its own CTest test,
    # scraperx_sim.<name>, run with SCRAPERX_ONLY=<name>. A file edited re-runs this
    # configure, and the post-build check below holds this list to the registry's.
    set(SCRAPERX_SIM_SLICES)
    foreach(test_file IN LISTS SCRAPERX_SIM_TEST_FILES)
        file(READ "${test_file}" test_text)
        string(REGEX MATCHALL "SCRAPERX_SLICE\\(\"[^\"]+\"" slice_hits "${test_text}")
        foreach(hit IN LISTS slice_hits)
            string(REGEX REPLACE "^SCRAPERX_SLICE\\(\"([^\"]+)\"$" "\\1" slice_name "${hit}")
            list(APPEND SCRAPERX_SIM_SLICES "${slice_name}")
        endforeach()
    endforeach()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SCRAPERX_SIM_TEST_FILES})
    list(SORT SCRAPERX_SIM_SLICES)
    foreach(slice IN LISTS SCRAPERX_SIM_SLICES)
        add_test(NAME scraperx_sim.${slice} COMMAND scraperx_sim_tests)
        set_tests_properties(scraperx_sim.${slice} PROPERTIES ENVIRONMENT "SCRAPERX_ONLY=${slice}")
    endforeach()
    # After each build the binary checks its own registry (a repeated name or key
    # fails the build) and that its slices are the ones registered above.
    if(NOT CMAKE_CROSSCOMPILING)
        string(JOIN "," SCRAPERX_SIM_SLICES_CSV ${SCRAPERX_SIM_SLICES})
        add_custom_command(TARGET scraperx_sim_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} "-DTEST_BINARY=$<TARGET_FILE:scraperx_sim_tests>"
                    "-DEXPECTED_SLICES=${SCRAPERX_SIM_SLICES_CSV}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_registry.cmake"
            VERBATIM)
    endif()
    add_executable(scraperx_parkour_flow_tests tests/parkour_flow_tests.cpp)
    target_link_libraries(scraperx_parkour_flow_tests PRIVATE scraperx_sim)
    target_compile_options(scraperx_parkour_flow_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wpedantic>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>
    )
    add_test(NAME scraperx_sim.parkour_flow COMMAND scraperx_parkour_flow_tests)
endif()

# Measurement probes (tools/probes): standalone executables that print numbers
# for a job's design, never tests (no add_test, no ctest slice). Off by default.
option(SCRAPERX_BUILD_PROBES "Build the measurement probes in tools/probes" OFF)
if(SCRAPERX_BUILD_PROBES)
    # J-038 (#169): the impulses loose things take from the forklift, a 1 m
    # drop and her thrown stock, for the dent and break limits.
    add_executable(scraperx_j038_impulse_probe tools/probes/j038_impulse_probe.cpp)
    target_link_libraries(scraperx_j038_impulse_probe PRIVATE scraperx_sim)
    target_include_directories(scraperx_j038_impulse_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_compile_options(scraperx_j038_impulse_probe PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)
endif()

if(SCRAPERX_BUILD_BRIDGE)
    set(GODOTCPP_API_VERSION "4.7" CACHE STRING "" FORCE)
    set(GODOTCPP_TARGET "template_release" CACHE STRING "" FORCE)
    set(GODOTCPP_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(GODOTCPP_USE_HOT_RELOAD OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        godot_cpp
        GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
        GIT_TAG ${SCRAPERX_GODOT_CPP_COMMIT}
        GIT_SHALLOW FALSE
    )
    FetchContent_MakeAvailable(godot_cpp)

    add_library(scraperx_native SHARED
        src/bridge/register_types.cpp
        src/bridge/scraperx_simulation.cpp
    )
    target_link_libraries(scraperx_native PRIVATE scraperx_sim scraperx_delivery godot::cpp)
    target_include_directories(scraperx_native PRIVATE src)
    set_target_properties(scraperx_native PROPERTIES
        PREFIX "lib"
        OUTPUT_NAME "scraperx_native"
    )

    if(ANDROID)
        set(SCRAPERX_EXTENSION_OUTPUT "${CMAKE_SOURCE_DIR}/godot/addons/scraperx_native/bin/android/arm64")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(SCRAPERX_EXTENSION_OUTPUT "${CMAKE_SOURCE_DIR}/godot/addons/scraperx_native/bin/linux")
    else()
        set(SCRAPERX_EXTENSION_OUTPUT "${CMAKE_BINARY_DIR}/bin")
    endif()

    set_target_properties(scraperx_native PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${SCRAPERX_EXTENSION_OUTPUT}"
        RUNTIME_OUTPUT_DIRECTORY "${SCRAPERX_EXTENSION_OUTPUT}"
    )
endif()
