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

add_library(scraperx_sim STATIC
    src/sim/simulation.cpp
    src/sim/steam_plant.cpp
)
target_include_directories(scraperx_sim PUBLIC src)
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
    )
    FetchContent_MakeAvailable(jolt)
    set_target_properties(Jolt PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(scraperx_sim PUBLIC Jolt)
    target_compile_definitions(scraperx_sim PUBLIC SCRAPERX_HAS_JOLT=1)
endif()

if(BUILD_TESTING AND NOT ANDROID)
    add_executable(scraperx_sim_tests tests/simulation_tests.cpp)
    target_link_libraries(scraperx_sim_tests PRIVATE scraperx_sim)
    target_compile_options(scraperx_sim_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wpedantic>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>
    )
    add_test(NAME scraperx_sim.athletic_traversal COMMAND scraperx_sim_tests)
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
    target_link_libraries(scraperx_native PRIVATE scraperx_sim godot::cpp)
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
