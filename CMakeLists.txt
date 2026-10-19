cmake_minimum_required(VERSION 3.25)
project(wots4_mod_boss_rush LANGUAGES C CXX)

# When building standalone, pass -DWOTS4_SDK_DIR=<path to wots4-mods>
if(NOT COMMAND wots4_add_mod)
    set(WOTS4_SDK_DIR "" CACHE PATH "Path to wots4-mods (ModLoader)")
    if(NOT EXISTS "${WOTS4_SDK_DIR}/cmake/Wots4.cmake")
        message(FATAL_ERROR "Set WOTS4_SDK_DIR to the path of wots4-mods")
    endif()
    include(${WOTS4_SDK_DIR}/cmake/Wots4.cmake)
    enable_testing()
    set(WOTS4_BUILD_TESTS ON)
endif()

# Use the Mod's folder name in the game folder (mods/<name>/) as <name>
wots4_use_minhook()
wots4_add_mod(boss_rush SOURCES src/mod.cpp src/rush.cpp src/game_actors.cpp src/exit_lock.cpp src/travel.cpp DEF src/mod.def
    LIBS wots4_minhook NO_RELEASE)
target_include_directories(mod_boss_rush PRIVATE src)
target_compile_definitions(mod_boss_rush PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

if(WOTS4_BUILD_TESTS)
    wots4_add_probe(boss_rush_offline_tests SOURCES tests/offline_tests.cpp INCLUDES src)
endif()
