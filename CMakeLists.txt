cmake_minimum_required(VERSION 3.16)
project(FluxScript VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC needs this to expose M_PI, M_SQRT2 etc. from <cmath>
if(MSVC)
    add_compile_definitions(_USE_MATH_DEFINES)
endif()

# Suppress LLVM narrowing conversion warnings in codegen
if(NOT MSVC)
    add_compile_options(-Wno-narrowing)
endif()

# =============================================================================
# Options
# =============================================================================

option(FLUX_BUILD_TESTS "Build tests" OFF)
option(FLUX_ENABLE_MLIR "Enable optional MLIR integration" OFF)
option(FLUX_VIO_MATRIXC_LIB_DIR "Path to VioMATRIXC libngspice directory" "")

# =============================================================================
# Find Dependencies
# =============================================================================

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
include_directories(${LLVM_INCLUDE_DIRS})
if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_DEFINITIONS}")
else()
    add_definitions(${LLVM_DEFINITIONS})
endif()

# Find CURL for package manager (optional on Windows)
find_package(CURL QUIET)

# Find ngspice for circuit simulation
if(FLUX_VIO_MATRIXC_LIB_DIR)
    message(STATUS "Using VioMATRIXC libngspice from: ${FLUX_VIO_MATRIXC_LIB_DIR}")
    set(NGSPICE_LIBRARY "${FLUX_VIO_MATRIXC_LIB_DIR}/libngspice.so.0" CACHE FILEPATH "VioMATRIXC ngspice library" FORCE)
    set(NGSPICE_INCLUDE_DIR "${FLUX_VIO_MATRIXC_LIB_DIR}/../../../src/include" CACHE PATH "VioMATRIXC ngspice include dir" FORCE)
else()
    find_path(NGSPICE_INCLUDE_DIR ngspice/sharedspice.h /usr/include /usr/local/include)
    find_library(NGSPICE_LIBRARY ngspice /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib)
endif()

if(NGSPICE_INCLUDE_DIR AND NGSPICE_LIBRARY)
    message(STATUS "Found ngspice: ${NGSPICE_LIBRARY}")
    set(NGSPICE_FOUND TRUE)
    include_directories(${NGSPICE_INCLUDE_DIR})
else()
    message(WARNING "ngspice not found - simulation will use stub implementation")
    set(NGSPICE_FOUND FALSE)
endif()

# Find Eigen
find_package(Eigen3 3.4 QUIET)
if(NOT Eigen3_FOUND)
    set(EIGEN3_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/eigen-3.4.0)
    message(STATUS "Using bundled Eigen from: ${EIGEN3_INCLUDE_DIR}")
else()
    message(STATUS "Found Eigen ${EIGEN3_VERSION}")
endif()
add_definitions(-DEIGEN_MPL2_ONLY)

# Use monolithic LLVM shared library on desktop Linux, component libs elsewhere
if(UNIX AND NOT ANDROID)
    find_library(LLVM_LIB LLVM-21 PATHS ${LLVM_LIBRARY_DIR} /usr/lib/llvm-21/lib NO_DEFAULT_PATH)
endif()
if(NOT LLVM_LIB)
    llvm_map_components_to_libnames(LLVM_LIB core support codegen mcjit orcjit aarch64codegen
        analysis bitreader bitwriter linker irreader native asmparser executionengine
    )
    message(STATUS "Using LLVM component libraries")
endif()

# =============================================================================
# Include Directories
# =============================================================================

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# =============================================================================
# Core Compiler Library
# =============================================================================

set(FLUXSCRIPT_SOURCES
    src/compiler/compiler_instance.cpp
    src/compiler/lexer.cpp
    src/compiler/parser.cpp
    src/compiler/codegen.cpp
    src/compiler/module_loader.cpp
    src/compiler/preprocessor.cpp
    src/compiler/control_flow_parser.cpp
    src/compiler/control_flow_codegen.cpp
    src/compiler/extended_parser.cpp
    src/compiler/extended_codegen.cpp
    src/compiler/parallel_parser.cpp
    src/compiler/parallel_codegen.cpp
    src/compiler/schematic_parser.cpp
    src/compiler/schematic_codegen.cpp
    src/compiler/time_domain_parser.cpp
    src/compiler/time_domain_codegen.cpp
    src/compiler/component_modeling_parser.cpp
    src/compiler/component_modeling_codegen.cpp
    src/compiler/model_quality_parser.cpp
    src/compiler/model_quality_codegen.cpp
    src/compiler/mixed_signal_codegen.cpp
    src/compiler/symbolic_parser.cpp
    src/compiler/symbolic_codegen.cpp
    src/compiler/advanced_features.cpp
    src/compiler/netlist_generator.cpp
    src/compiler/new_features_codegen.cpp
    src/compiler/adevice_codegen.cpp
    src/compiler/codegen_new.cpp
    src/security/security.cpp
    src/runtime/units.cpp
    src/runtime/mixed_signal_runtime.cpp
    src/runtime/autodiff.cpp
    src/jit/flux_jit.cpp
    src/jit/jit_manager.cpp
    src/jit/simulation_hook.cpp
    src/jit/scaling_security.cpp
    src/jit/smart_signal.cpp
    src/jit/zero_copy_data.cpp
    src/jit_engine.cpp
    src/runtime/flux_runtime.cpp
    src/runtime/advanced_runtime.cpp
    src/runtime/time_domain_api.cpp
    src/runtime/model_quality_engine.cpp
    src/runtime/symbolic_engine.cpp
    src/runtime/parallel_runtime.cpp
    src/runtime/ngspice_bridge.cpp
    src/runtime/spice_runtime.cpp
    src/tooling/tooling.cpp
    src/tooling/package_manager.cpp
    src/tooling/repl.cpp
    src/tooling/notebook_kernel.cpp
    src/tooling/doc_generator.cpp
    src/tooling/example_gallery.cpp
    src/tooling/profiler.cpp
    src/tooling/benchmark.cpp
    src/tooling/circuit_diff.cpp
    src/tooling/auto_docs.cpp
    src/tooling/lsp_server.cpp
    src/tooling/source_docs.cpp
    src/analysis/advanced_analysis.cpp
    src/analysis/fft_engine.cpp
    src/doctor/circuit_doctor.cpp
    src/instruments/instrument.cpp
    src/sweep/parametric_sweep.cpp
    src/testing/circuit_fuzz.cpp
    src/toolbox/power.cpp
    src/toolbox/control.cpp
    src/runtime/advanced_math.cpp
    src/ai/surrogate.cpp
    src/phase3_stubs.cpp
    src/phase4a_stubs.cpp
    src/package/http_client.cpp
    src/package/toml_parser.cpp
    src/package/package_manager.cpp
    src/package/dependency_resolver.cpp
    src/analysis/component_substitution.cpp
    src/verification/verification_dx.cpp
    src/verification/property_testing.cpp
    src/verification/cross_simulator_validation.cpp
    src/simulation/monte_carlo.cpp
)

if(FLUX_ENABLE_MLIR)
    find_package(MLIR QUIET CONFIG)
    if(MLIR_FOUND)
        set(FLUX_HAS_MLIR ON)
        list(APPEND FLUXSCRIPT_SOURCES src/mlir/flux_mlir.cpp)
        include_directories(${MLIR_INCLUDE_DIRS})
        add_definitions(${MLIR_DEFINITIONS})
        set(FLUX_MLIR_LIBS MLIRIR MLIRParser MLIRSupport MLIRFuncDialect MLIRArithDialect MLIRTensorDialect)
    else()
        list(APPEND FLUXSCRIPT_SOURCES src/mlir/flux_mlir_stub.cpp)
    endif()
else()
    list(APPEND FLUXSCRIPT_SOURCES src/mlir/flux_mlir_stub.cpp)
endif()

if(MSVC)
    add_library(FluxScript SHARED ${FLUXSCRIPT_SOURCES})
    set_property(TARGET FluxScript PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set_target_properties(FluxScript PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
elseif(MINGW)
    add_library(FluxScript STATIC ${FLUXSCRIPT_SOURCES})
else()
    add_library(FluxScript SHARED ${FLUXSCRIPT_SOURCES})
endif()

# Work around -O2 optimizer issues in verification and JIT code
set_source_files_properties(
    src/verification/property_testing.cpp
    src/verification/cross_simulator_validation.cpp
    src/jit/jit_manager.cpp
    src/jit/smart_signal.cpp
    src/jit/simulation_hook.cpp
    src/jit/zero_copy_data.cpp
    src/jit/scaling_security.cpp
    PROPERTIES COMPILE_FLAGS "-O1"
)

if(ANDROID)
    # Whole-archive only LLVMSupport for cl:: RTTI symbols exported to
    # executables. Other libs linked normally (dead code stripped).
    set(LLVM_WA_LIBS "")
    set(LLVM_NOWA_LIBS "")
    foreach(lib IN LISTS LLVM_LIB)
        if(lib MATCHES "LLVMSupport$")
            list(APPEND LLVM_WA_LIBS ${lib})
        else()
            list(APPEND LLVM_NOWA_LIBS ${lib})
        endif()
    endforeach()
    target_link_libraries(FluxScript PUBLIC
        -Wl,--whole-archive ${LLVM_WA_LIBS} -Wl,--no-whole-archive
        ${LLVM_NOWA_LIBS}
        ${FLUX_MLIR_LIBS})
else()
    target_link_libraries(FluxScript PUBLIC ${LLVM_LIB} ${FLUX_MLIR_LIBS})
endif()
if(MSVC)
    target_link_libraries(FluxScript PUBLIC ws2_32)
    # LLVM Windows archive was built with VS 2019 DIA SDK but
    # the runner may have a different VS version. Locate diaguids.lib.
    find_library(DIA_GUIDS_LIB diaguids.lib PATHS
        "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/DIA SDK/lib/amd64"
        "C:/Program Files/Microsoft Visual Studio/2022/Professional/DIA SDK/lib/amd64"
        "C:/Program Files/Microsoft Visual Studio/2022/Community/DIA SDK/lib/amd64"
        "C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/DIA SDK/lib/amd64"
    )
    if(DIA_GUIDS_LIB)
        get_filename_component(DIA_LIB_DIR "${DIA_GUIDS_LIB}" DIRECTORY)
        set_property(TARGET FluxScript APPEND PROPERTY LINK_DIRECTORIES "${DIA_LIB_DIR}")
        message(STATUS "Found DIA SDK: ${DIA_GUIDS_LIB}")
    endif()
endif()
if(CURL_FOUND)
    target_link_libraries(FluxScript PUBLIC CURL::libcurl)
    target_compile_definitions(FluxScript PUBLIC FLUX_HAS_CURL)
endif()
if(NGSPICE_FOUND)
    target_link_libraries(FluxScript PUBLIC ${NGSPICE_LIBRARY})
    target_compile_definitions(FluxScript PUBLIC FLUX_HAS_NGSPICE)
endif()

if(FLUX_HAS_MLIR)
    target_compile_definitions(FluxScript PUBLIC FLUX_HAS_MLIR)
endif()

target_include_directories(FluxScript PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${LLVM_INCLUDE_DIRS}
    ${EIGEN3_INCLUDE_DIR}
)

# =============================================================================
# CLI Tools
# =============================================================================

# Main CLI (replaces the old Qt-based flux)
add_executable(flux cli/minimal.cpp)
target_link_libraries(flux PRIVATE FluxScript)

add_executable(flux-pkg cli/flux-pkg.cpp)
target_link_libraries(flux-pkg PRIVATE FluxScript)

add_executable(flux-spice cli/flux-spice.cpp src/runtime/ngspice_bridge.cpp)
if(NGSPICE_FOUND)
    target_link_libraries(flux-spice PRIVATE ${NGSPICE_LIBRARY})
endif()
target_link_libraries(flux-spice PRIVATE FluxScript)

add_executable(flux-run src/cli/flux_run.cpp src/runtime/ngspice_bridge.cpp)
if(NGSPICE_FOUND)
    target_link_libraries(flux-run PRIVATE ${NGSPICE_LIBRARY})
    target_compile_definitions(flux-run PRIVATE FLUX_HAS_NGSPICE=1)
endif()
target_link_libraries(flux-run PRIVATE FluxScript)

add_executable(flux-doctor src/cli/flux-doctor.cpp)
target_link_libraries(flux-doctor PRIVATE FluxScript)

add_executable(flux-sweep src/cli/flux-sweep.cpp)
target_link_libraries(flux-sweep PRIVATE FluxScript)

add_executable(flux-fft src/cli/flux_fft.cpp)
target_link_libraries(flux-fft PRIVATE FluxScript)

add_executable(flux-repl cli/repl.cpp)
target_link_libraries(flux-repl PRIVATE FluxScript)

add_executable(fluxc cli/fluxc.cpp)
target_link_libraries(fluxc PRIVATE FluxScript)

add_executable(flux-format cli/flux-format.cpp)
target_link_libraries(flux-format PRIVATE FluxScript)

add_executable(flux-doc cli/flux-doc.cpp)
target_link_libraries(flux-doc PRIVATE FluxScript)

add_executable(flux-test cli/flux-test.cpp)
target_link_libraries(flux-test PRIVATE FluxScript)

add_executable(flux-instrument cli/flux-instrument.cpp)
target_link_libraries(flux-instrument PRIVATE FluxScript)

add_executable(flux-optimize cli/flux-optimize.cpp)
target_link_libraries(flux-optimize PRIVATE FluxScript)

add_executable(flux-lsp cli/flux-lsp.cpp)
target_link_libraries(flux-lsp PRIVATE FluxScript)

add_executable(flux-worstcase cli/flux-worstcase.cpp)
target_link_libraries(flux-worstcase PRIVATE FluxScript)

add_executable(flux-stability cli/flux-stability.cpp)
target_link_libraries(flux-stability PRIVATE FluxScript)

add_executable(flux-monte-carlo cli/flux-monte-carlo.cpp)
target_link_libraries(flux-monte-carlo PRIVATE FluxScript)

# =============================================================================
# Tests (Optional)
# =============================================================================

# Helper function to register test executables
function(add_test_executable target_name source_file)
    add_executable(${target_name} ${CMAKE_CURRENT_SOURCE_DIR}/${source_file})
    target_link_libraries(${target_name} PRIVATE FluxScript)
    target_compile_definitions(${target_name} PRIVATE TESTS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
endfunction()

# Helper function to register test with CTest
function(add_test_entry target_name label)
    string(REPLACE "flux-" "" clean_label ${target_name})
    add_test(NAME ${target_name}
        COMMAND ${target_name}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(${target_name} PROPERTIES
        LABELS "${clean_label}"
        TIMEOUT 120
    )
endfunction()

if(FLUX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# =============================================================================
# Installation
# =============================================================================

install(TARGETS FluxScript flux flux-pkg flux-spice flux-run flux-doctor flux-sweep flux-fft flux-repl fluxc flux-format flux-doc flux-test flux-instrument flux-optimize flux-lsp flux-worstcase flux-stability flux-monte-carlo
    EXPORT FluxScriptTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/flux DESTINATION include)

install(DIRECTORY stdlib/ DESTINATION share/flux/stdlib
    FILES_MATCHING PATTERN "*.flux")
