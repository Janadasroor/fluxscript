# fluxqt — Standalone Qt + FluxScript bridge example
#
# Prerequisites: fluxscript must be built first (make build-debug)
#
# Build:
#   cd examples/fluxqt
#   mkdir -p build && cd build
#   cmake .. -DFLUXSCRIPT_ROOT=/path/to/fluxscript
#   make -j$(nproc)
#
# Run:
#   ./fluxqt ../modern_dashboard.flux

cmake_minimum_required(VERSION 3.20)
project(fluxqt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ── FluxScript build tree ─────────────────────────────────────────────────
set(FLUXSCRIPT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    CACHE PATH "Path to the fluxscript project root (with build/)")

if(NOT EXISTS "${FLUXSCRIPT_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "FLUXSCRIPT_ROOT not found: ${FLUXSCRIPT_ROOT}")
endif()

set(FLUXSCRIPT_BUILD_DIR "${FLUXSCRIPT_ROOT}/build")

# FluxScript library (built in-tree)
add_library(FluxScript SHARED IMPORTED)
set_target_properties(FluxScript PROPERTIES
    IMPORTED_LOCATION "${FLUXSCRIPT_BUILD_DIR}/libFluxScript.so"
    INTERFACE_INCLUDE_DIRECTORIES "${FLUXSCRIPT_ROOT}/include"
)

# LLVM (required by FluxScript)
find_package(LLVM REQUIRED)
llvm_map_components_to_libnames(llvm_libs
    core executionengine orcjit native irreader support
    targetparser passes
)

# Qt
find_package(Qt6 REQUIRED COMPONENTS Widgets SvgWidgets)

# ── Main executable ──────────────────────────────────────────────────────
add_executable(fluxqt
    main.cpp
    bridge.cpp
)

target_link_libraries(fluxqt PRIVATE
    Qt6::Widgets
    Qt6::SvgWidgets
    FluxScript
    ${llvm_libs}
    dl
    pthread
    z
)

target_include_directories(fluxqt PRIVATE
    "${FLUXSCRIPT_ROOT}/include"
    "${LLVM_INCLUDE_DIRS}"
)

# Copy .flux files to build dir for easy access
file(COPY
    "${CMAKE_CURRENT_SOURCE_DIR}/modern_dashboard.flux"
    "${CMAKE_CURRENT_SOURCE_DIR}/demo.flux"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}"
)

# ── Convenience targets ──────────────────────────────────────────────────
add_custom_target(run
    COMMAND ./fluxqt "${CMAKE_CURRENT_BINARY_DIR}/modern_dashboard.flux"
    DEPENDS fluxqt
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    USES_TERMINAL
)

add_custom_target(run-demo
    COMMAND ./fluxqt "${CMAKE_CURRENT_BINARY_DIR}/demo.flux"
    DEPENDS fluxqt
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    USES_TERMINAL
)
