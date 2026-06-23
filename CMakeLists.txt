cmake_minimum_required(VERSION 3.16)
project(FluxScript VERSION 1.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC needs this to expose M_PI, M_SQRT2 etc. from <cmath>
if(MSVC)
    add_compile_definitions(_USE_MATH_DEFINES)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
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
# Build Optimizations
# =============================================================================

# Export compile_commands.json for tooling (clangd, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ccache (auto-detected)
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache found: ${CCACHE_PROGRAM}")
endif()

# mold linker (Linux x86_64, non-MSVC)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT MSVC)
    find_program(MOLD_LINKER mold)
    if(MOLD_LINKER)
        set(CMAKE_LINKER_TYPE MOLD)
        message(STATUS "mold linker found: ${MOLD_LINKER}")
    endif()
endif()

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

# Find Threads (for static runtime linking)
find_package(Threads REQUIRED)

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

# Use system-installed Eigen (header-only)
set(EIGEN3_INCLUDE_DIR "" CACHE PATH "Path to Eigen3 include directory (parent of Eigen/ directory)")
if(EIGEN3_INCLUDE_DIR STREQUAL "")
    find_package(Eigen3 QUIET)
    if(NOT Eigen3_FOUND)
        find_package(Eigen QUIET) # Eigen >= 5 renamed the config
    endif()
    if(Eigen3_FOUND)
        if(Eigen3_INCLUDE_DIRS)
            set(EIGEN3_INCLUDE_DIR ${Eigen3_INCLUDE_DIRS})
        elseif(Eigen3_INCLUDE_DIR)
            set(EIGEN3_INCLUDE_DIR ${Eigen3_INCLUDE_DIR})
        endif()
    elseif(Eigen_FOUND)
        if(Eigen_INCLUDE_DIRS)
            set(EIGEN3_INCLUDE_DIR ${Eigen_INCLUDE_DIRS})
        endif()
    endif()
endif()
# Fallback: search for Eigen/Dense header
if(EIGEN3_INCLUDE_DIR STREQUAL "")
    find_path(EIGEN3_INCLUDE_DIR Eigen/Dense
        HINTS /usr/include/eigen3 /usr/local/include/eigen3
              /opt/homebrew/include/eigen3 /opt/homebrew/include
              /opt/homebrew/opt/eigen/include
        PATH_SUFFIXES eigen3)
endif()
# macOS brew fallback: Eigen 5 may be keg-only in Cellar
if(EIGEN3_INCLUDE_DIR STREQUAL "")
    execute_process(COMMAND brew --prefix eigen
        OUTPUT_VARIABLE BREW_EIGEN_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(BREW_EIGEN_PREFIX)
        find_path(EIGEN3_INCLUDE_DIR Eigen/Dense
            HINTS ${BREW_EIGEN_PREFIX}/include
            NO_DEFAULT_PATH)
    endif()
endif()
if(EIGEN3_INCLUDE_DIR STREQUAL "")
    message(WARNING "Eigen3 not found - Eigen-dependent features disabled")
else()
    add_definitions(-DFLUX_HAVE_EIGEN)
    message(STATUS "Eigen found: ${EIGEN3_INCLUDE_DIR}")
endif()

# Use monolithic LLVM shared library on desktop Linux, component libs elsewhere
if(UNIX AND NOT ANDROID)
    find_library(LLVM_LIB LLVM-21 PATHS ${LLVM_LIBRARY_DIR} /usr/lib/llvm-21/lib NO_DEFAULT_PATH)
endif()
if(NOT LLVM_LIB)
    llvm_map_components_to_libnames(LLVM_LIB core support codegen mcjit orcjit
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
    # Compiler - Core
    src/compiler/core/compiler_instance.cpp
    src/compiler/core/lexer.cpp
    src/compiler/core/parser.cpp
    src/compiler/core/lifetime_inference.cpp
    src/compiler/core/codegen.cpp
    src/compiler/core/call_codegen.cpp
    src/compiler/core/binary_codegen.cpp
    src/compiler/core/declaration_codegen.cpp
    src/compiler/core/module_loader.cpp
    src/compiler/core/preprocessor.cpp
    src/compiler/core/control_flow_parser.cpp
    src/compiler/core/control_flow_codegen.cpp
    src/compiler/core/extended_parser.cpp
    src/compiler/core/extended_codegen.cpp
    src/compiler/core/parallel_parser.cpp
    src/compiler/core/parallel_codegen.cpp
    src/compiler/core/new_features_codegen.cpp
    src/compiler/core/codegen_new.cpp
    # Compiler - SPICE
    src/compiler/spice/spice_parser.cpp
    src/compiler/spice/spice_codegen.cpp
    src/compiler/spice/adevice_codegen.cpp
    # Compiler - Mixed Signal
    src/compiler/mixed_signal/mixed_signal_parser.cpp
    src/compiler/mixed_signal/mixed_signal_codegen.cpp
    # Compiler - Types
    src/compiler/types/type_parser.cpp
    # Compiler - Domains
    src/compiler/domains/symbolic_parser.cpp
    src/compiler/domains/symbolic_codegen.cpp
    src/compiler/domains/model_quality_parser.cpp
    src/compiler/domains/model_quality_codegen.cpp
    src/compiler/domains/component_modeling_parser.cpp
    src/compiler/domains/component_modeling_codegen.cpp
    src/compiler/domains/schematic_parser.cpp
    src/compiler/domains/schematic_codegen.cpp
    src/compiler/domains/time_domain_parser.cpp
    src/compiler/domains/time_domain_codegen.cpp
    src/compiler/domains/advanced_features.cpp
    src/compiler/domains/netlist_generator.cpp
    # Security
    src/security/security.cpp
    # Runtime - Core
    src/runtime/core/flux_runtime.cpp
    src/runtime/core/registration.cpp
    src/runtime/core/units.cpp
    src/runtime/core/autodiff.cpp
    src/runtime/core/spice_runtime.cpp
    src/runtime/core/ngspice_bridge.cpp
    src/runtime/core/mixed_signal_runtime.cpp
    src/runtime/core/model_quality_engine.cpp
    src/runtime/core/flux_model_bridge.cpp
    src/runtime/core/time_domain_api.cpp
    # Runtime - Matrix
    src/runtime/matrix/matrix_runtime.cpp
    src/runtime/matrix/advanced_math.cpp
    # Runtime - IO
    src/runtime/io/string_runtime.cpp
    src/runtime/io/fileio_runtime.cpp
    src/runtime/io/flux_llvm_bridge.cpp
    # Runtime - Threading
    src/runtime/threading/threading_runtime.cpp
    src/runtime/threading/parallel_runtime.cpp
    # Runtime - Analysis
    src/runtime/analysis/analysis_runtime.cpp
    src/runtime/analysis/advanced_runtime.cpp
    # Runtime - Symbolic
    src/runtime/symbolic/symbolic_runtime.cpp
    src/runtime/symbolic/symbolic_engine.cpp
    # JIT
    src/jit/flux_jit.cpp
    src/jit/jit_manager.cpp
    src/jit/simulation_hook.cpp
    src/jit/scaling_security.cpp
    src/jit/smart_signal.cpp
    src/jit/zero_copy_data.cpp
    src/jit_engine.cpp
    # Tooling - LSP
    src/tooling/lsp/lsp_server.cpp
    src/tooling/lsp/lsp_completions.cpp
    src/tooling/lsp/lsp_hierarchy.cpp
    src/tooling/lsp/lsp_semantic.cpp
    src/tooling/lsp/lsp_utils.cpp
    # Tooling - Core
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
    src/tooling/source_docs.cpp
    # Analysis
    src/analysis/advanced_analysis.cpp
    src/analysis/fft_engine.cpp
    src/analysis/component_substitution.cpp
    # Other
    src/doctor/circuit_doctor.cpp
    src/instruments/instrument.cpp
    src/sweep/parametric_sweep.cpp
    src/testing/circuit_fuzz.cpp
    src/toolbox/power.cpp
    src/toolbox/control.cpp
    src/ai/surrogate.cpp
    src/phase3_stubs.cpp
    src/phase4a_stubs.cpp
    src/package/http_client.cpp
    src/package/toml_parser.cpp
    src/package/package_manager.cpp
    src/package/dependency_resolver.cpp
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

# =============================================================================
# Runtime Static Library (for AOT standalone binaries, no LLVM dependency)
# =============================================================================

set(FLUX_RUNTIME_SOURCES
    src/runtime/core/flux_runtime.cpp
    src/runtime/matrix/matrix_runtime.cpp
    src/runtime/io/string_runtime.cpp
    src/runtime/io/fileio_runtime.cpp
    src/runtime/threading/threading_runtime.cpp
    src/runtime/analysis/analysis_runtime.cpp
    src/runtime/symbolic/symbolic_runtime.cpp
    src/runtime/analysis/advanced_runtime.cpp
    src/runtime/matrix/advanced_math.cpp
    src/runtime/core/spice_runtime.cpp
    src/runtime/threading/parallel_runtime.cpp
    src/runtime/symbolic/symbolic_engine.cpp
    src/runtime/core/units.cpp
    src/runtime/core/autodiff.cpp
    src/runtime/core/mixed_signal_runtime.cpp
    src/runtime/core/time_domain_api.cpp
    src/runtime/core/model_quality_engine.cpp
)

add_library(FluxRuntime STATIC ${FLUX_RUNTIME_SOURCES})
target_compile_definitions(FluxRuntime PRIVATE FLUX_RUNTIME_STANDALONE)
set_target_properties(FluxRuntime PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(FluxRuntime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${EIGEN3_INCLUDE_DIR}
)

if(NGSPICE_FOUND)
    target_link_libraries(FluxRuntime PUBLIC ${NGSPICE_LIBRARY})
    target_compile_definitions(FluxRuntime PUBLIC FLUX_HAS_NGSPICE)
endif()

if(CURL_FOUND)
    target_link_libraries(FluxRuntime PUBLIC ${CURL_LIBRARIES})
    target_compile_definitions(FluxRuntime PUBLIC FLUX_HAS_CURL)
endif()

target_link_libraries(FluxRuntime PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

# Work around -O2 optimizer issues in verification and JIT code
set_source_files_properties(
    src/verification/property_testing.cpp
    src/verification/cross_simulator_validation.cpp
    src/jit/jit_manager.cpp
    src/jit/smart_signal.cpp
    src/jit/simulation_hook.cpp
    src/jit/zero_copy_data.cpp
    src/jit/scaling_security.cpp
    PROPERTIES COMPILE_FLAGS "$<$<CXX_COMPILER_ID:MSVC>:/O1>$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O1>"
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
    # Use module variables directly instead of the CURL::libcurl
    # imported target.  Some Ubuntu 22.04 runners ship a CURL config
    # with stale multiarch library and include paths on the imported
    # target that break the linker even though find_package succeeds.
    target_link_libraries(FluxScript PUBLIC ${CURL_LIBRARIES})
    if(CURL_INCLUDE_DIRS)
        target_include_directories(FluxScript PUBLIC ${CURL_INCLUDE_DIRS})
    endif()
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

# Build stdlib bitcode cache for faster subsequent compilations
set(STDLIB_SOURCE_DIR "${CMAKE_SOURCE_DIR}/stdlib")
set(STDLIB_BC_DIR "${CMAKE_BINARY_DIR}/stdlib_bc")
file(MAKE_DIRECTORY "${STDLIB_BC_DIR}")
file(GLOB STDLIB_FLUX_FILES "${STDLIB_SOURCE_DIR}/*.flux")
foreach(FLUX_FILE ${STDLIB_FLUX_FILES})
    get_filename_component(MODULE_NAME "${FLUX_FILE}" NAME_WE)
    set(BC_FILE "${STDLIB_BC_DIR}/${MODULE_NAME}.flux.bc")
    add_custom_command(
        OUTPUT "${BC_FILE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${STDLIB_BC_DIR}"
        COMMAND $<TARGET_FILE:flux> --emit bc -o "${BC_FILE}" "${FLUX_FILE}"
        DEPENDS flux "${FLUX_FILE}"
        COMMENT "Compiling stdlib/${MODULE_NAME}.flux to bitcode"
    )
    list(APPEND STDLIB_BC_FILES "${BC_FILE}")
endforeach()
if(STDLIB_BC_FILES)
    add_custom_target(stdlib_bc ALL DEPENDS ${STDLIB_BC_FILES})
endif()

add_executable(flux-pkg cli/flux-pkg.cpp)
target_link_libraries(flux-pkg PRIVATE FluxScript)

add_executable(flux-spice cli/flux-spice.cpp src/runtime/core/ngspice_bridge.cpp)
if(NGSPICE_FOUND)
    target_link_libraries(flux-spice PRIVATE ${NGSPICE_LIBRARY})
endif()
target_link_libraries(flux-spice PRIVATE FluxScript)

add_executable(flux-run src/cli/flux_run.cpp src/runtime/core/ngspice_bridge.cpp)
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

install(TARGETS FluxScript FluxRuntime flux flux-pkg flux-spice flux-run flux-doctor flux-sweep flux-fft flux-repl fluxc flux-format flux-doc flux-test flux-instrument flux-optimize flux-lsp flux-worstcase flux-stability flux-monte-carlo
    EXPORT FluxScriptTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/flux DESTINATION include)

install(DIRECTORY stdlib/ DESTINATION share/flux/stdlib
    FILES_MATCHING PATTERN "*.flux")
