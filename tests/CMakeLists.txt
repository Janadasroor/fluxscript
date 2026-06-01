# =============================================================================
# FluxScript Comprehensive Test Suite
# Registers ALL test files as CMake targets + CTest entries
# =============================================================================

# 
#  Phase 5: Component Substitution                             
# 
add_test_executable(flux-test-phase5 unit/test_phase5.cpp)
add_test_entry(flux-test-phase5 "Phase 5: Component Substitution")

# 
#  Phase 7.1: Model Quality & Verification                     
# 
add_test_executable(flux-test-phase7-1 unit/test_phase7_1_model_quality.cpp)
add_test_entry(flux-test-phase7-1 "Phase 7.1: Model Quality & Verification")

# 
#  Phase 7.4: Tooling & Developer Experience                   
# 
add_test_executable(flux-test-phase7-4 unit/test_phase7_4_tooling.cpp)
add_test_entry(flux-test-phase7-4 "Phase 7.4: Tooling & Developer Experience")

add_test_executable(flux-test-phase7-4b unit/test_phase7_4_part2.cpp)
add_test_entry(flux-test-phase7-4b "Phase 7.4 Part 2: Example Gallery & Dep. Resolver")

# 
#  Advanced Analysis Tools                                     
# 
add_test_executable(flux-test-advanced-analysis unit/test_advanced_analysis.cpp)
add_test_entry(flux-test-advanced-analysis "Advanced Analysis Tools")

add_test_executable(flux-test-advanced-analysis-full unit/test_advanced_analysis_full.cpp)
add_test_entry(flux-test-advanced-analysis-full "Advanced Analysis Full")

# 
#  Auto Documentation                                          
# 
add_test_executable(flux-test-auto-docs unit/test_auto_docs.cpp)
add_test_entry(flux-test-auto-docs "Auto Documentation")

# 
#  Behavioral Sources                                          
# 
add_test_executable(flux-test-behavioral-sources unit/test_behavioral_sources.cpp)
add_test_entry(flux-test-behavioral-sources "Behavioral Sources")

# 
#  Circuit Diff                                                
# 
add_test_executable(flux-test-circuit-diff unit/test_circuit_diff.cpp)
add_test_entry(flux-test-circuit-diff "Circuit Diff")

# 
#  Circuit Doctor                                              
# 
add_test_executable(flux-test-circuit-doctor unit/test_circuit_doctor.cpp)
add_test_entry(flux-test-circuit-doctor "Circuit Doctor")

# 
#  Circuit Fuzz Testing                                        
# 
add_test_executable(flux-test-circuit-fuzz unit/test_circuit_fuzz.cpp)
add_test_entry(flux-test-circuit-fuzz "Circuit Fuzz Testing")

# 
#  Feature Tests (Phases 2-4a)                                 
# 
add_test_executable(flux-test-features-phase2 unit/test_features_phase2.cpp)
add_test_entry(flux-test-features-phase2 "Features Phase 2")

add_test_executable(flux-test-features-phase3 unit/test_features_phase3.cpp)
add_test_entry(flux-test-features-phase3 "Features Phase 3")

add_test_executable(flux-test-features-phase4a unit/test_features_phase4a.cpp)
add_test_entry(flux-test-features-phase4a "Features Phase 4a")

# 
#  Features 4 & 5 (Power Electronics, Op-Amp)                  
# 
add_test_executable(flux-test-features-4-5 unit/test_features_4_5.cpp)
add_test_entry(flux-test-features-4-5 "Features 4 & 5")

# 
#  ngspice Integration (requires ngspice installed)            
# 
add_test_executable(flux-test-ngspice unit/test_ngspice.cpp)
add_test_entry(flux-test-ngspice "ngspice Integration")

# 
#  Instrument Control                                          
# 
add_test_executable(flux-test-instruments unit/test_instruments.cpp)
add_test_entry(flux-test-instruments "Instrument Control")

# 
#  Waveform Extraction                                         
# 
add_test_executable(flux-test-waveform-extraction unit/test_waveform_extraction.cpp)
add_test_entry(flux-test-waveform-extraction "Waveform Extraction")

# 
#  A-Device Netlist Generation                                  
# 
add_test_executable(flux-test-adevice-netlist unit/test_adevice_netlist.cpp)
add_test_entry(flux-test-adevice-netlist "A-Device Netlist")

# 
#  SPICE Bridge                                                 
# 
add_test_executable(flux-test-spice-bridge unit/test_spice_bridge.cpp)
add_test_entry(flux-test-spice-bridge "SPICE Bridge")

# 
#  Phase 2 Behavioral Modeling                                  
# 
add_test_executable(flux-test-phase2-behavioral unit/test_phase2_behavioral_modeling.cpp)
add_test_entry(flux-test-phase2-behavioral "Phase 2 Behavioral Modeling")

# 
#  Issue Repro                                                  
# 
add_test_executable(flux-test-repro-issue unit/repro_issue.cpp)
add_test_entry(flux-test-repro-issue "Issue Repro")

# 
#  Benchmark Suite (FluxScript JIT)                             
# 
add_test_executable(flux-test-benchmark-jit benchmarks/benchmark_fluxscript_jit.cpp)
add_test_entry(flux-test-benchmark-jit "JIT Benchmark")

# 
#  Benchmark Suite (Comprehensive)                              
# 
add_test_executable(flux-test-benchmark-suite benchmarks/benchmark_suite.cpp)
add_test(NAME flux-test-benchmark-suite
    COMMAND flux-test-benchmark-suite
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(flux-test-benchmark-suite PROPERTIES LABELS "unit")
set_tests_properties(flux-test-benchmark-suite PROPERTIES TIMEOUT 600)
# 

# 
#  Module System (import)                                      
# 
add_test_executable(flux-test-module-system unit/test_module_system.cpp)

add_test(NAME flux-test-module-system
    COMMAND flux-test-module-system
)
set_tests_properties(flux-test-module-system PROPERTIES
    LABELS "module-system;import;compiler;unit"
    TIMEOUT 60
)

find_program(BASH_EXECUTABLE NAMES bash sh)

# Fallback for Windows where Git Bash is installed but not on standard PATH
if(NOT BASH_EXECUTABLE AND WIN32)
    find_program(BASH_EXECUTABLE NAMES bash sh
        PATHS
        "C:/Program Files/Git/bin"
        "C:/Program Files/Git/usr/bin"
        "C:/Program Files (x86)/Git/bin"
        "C:/Program Files (x86)/Git/usr/bin"
        "C:/msys64/usr/bin"
    )
endif()

# If bash/sh is not found, use a platform fallback
if(NOT BASH_EXECUTABLE)
    if(CMAKE_HOST_UNIX)
        set(BASH_EXECUTABLE "sh")
    endif()
endif()

# 
#  Integration Tests (.flux snippets via flux-minimal)         
# 
if(BASH_EXECUTABLE)
    add_test(NAME flux-test-integration
        COMMAND ${BASH_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/integration/run_tests.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
else()
    add_test(NAME flux-test-integration
        COMMAND ${CMAKE_SOURCE_DIR}/tests/integration/run_tests.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
set_tests_properties(flux-test-integration PROPERTIES
    LABELS "integration;compiler"
    TIMEOUT 300
)

# 
#  Individual Integration Tests (.flux snippets)              
# 
# Each test case from run_tests.sh is registered as its own CTest test
# for granular CI reporting. The .flux and .meta files are generated
# by tests/integration/extract_tests.py.
# 

# Helper: register a single .flux integration test
function(add_flux_integration_test test_name flux_file is_check)
    set(flux_bin "$<TARGET_FILE:flux>")
    if(NOT TARGET flux)
        set(flux_bin "${CMAKE_BINARY_DIR}/flux")
    endif()
    if(is_check)
        add_test(NAME ${test_name}
            COMMAND ${CMAKE_BINARY_DIR}/flux --cache=0 --emit=check ${flux_file}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    else()
        add_test(NAME ${test_name}
            COMMAND ${CMAKE_BINARY_DIR}/flux --cache=0 ${flux_file}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    endif()
    set_tests_properties(${test_name} PROPERTIES
        LABELS "integration;fluxscript"
        TIMEOUT 30
    )
endfunction()

# Tests that use ref/copy syntax (&mut, *x) — self-hosted compiler only
set(ref_test_exclusions
    test_068_Ref_Borrow_Deref
    test_069_Ref_Multiple_Borrows
    test_070_Ref_Mut_Assign
    test_071_Ref_Param
    test_072_Ref_Complex
    test_073_Copy_Struct
    test_074_Copy_Enum
    test_075_Copy_Func_Move
    test_076_Copy_Moved_Error
    test_077_Ref_Field_Access
    test_078_Ref_Field_Mut
    test_079_Ref_Field_Valid
    test_080_Elision_Multi_Ref
    test_081_Elision_Assign_Borrowed
    test_082_Elision_Method_Self
    test_083_Field_Borrow
    test_084_Field_Assign_Borrow
    test_085_Multi_Ref_Field
    test_086_Field_Borrow_Conflict
)

# Discover individual test files
file(GLOB flux_test_files
    "${CMAKE_SOURCE_DIR}/tests/integration/tests/test_*.flux"
)

foreach(f IN LISTS flux_test_files)
    get_filename_component(basename ${f} NAME_WE)
    get_filename_component(filename ${f} NAME)

    # Skip tests that require the self-hosted compiler (ref/copy syntax)
    if(${basename} IN_LIST ref_test_exclusions)
        continue()
    endif()

    set(meta_file "${CMAKE_SOURCE_DIR}/tests/integration/tests/${basename}.meta")

    if(EXISTS ${meta_file})
        file(STRINGS ${meta_file} meta_lines LIMIT_COUNT 2)
        list(GET meta_lines 0 test_type)
        list(GET meta_lines 1 display_name)

        string(STRIP ${test_type} test_type)
        string(STRIP ${display_name} display_name)

        if(test_type STREQUAL "check")
            add_flux_integration_test(
                "flux-test-integration-${basename}"
                "${f}"
                TRUE
            )
        elseif(test_type STREQUAL "expect_error")
            # expect_error tests: flux returns 0 on error, non-zero on success
            add_test(NAME "flux-test-integration-${basename}"
                COMMAND ${CMAKE_BINARY_DIR}/flux --cache=0 ${f}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties("flux-test-integration-${basename}" PROPERTIES
                WILL_FAIL TRUE
                LABELS "integration;fluxscript;individual"
                TIMEOUT 30
            )
            continue()
        else()
            add_flux_integration_test(
                "flux-test-integration-${basename}"
                "${f}"
                FALSE
            )
        endif()

        set_tests_properties("flux-test-integration-${basename}" PROPERTIES
            LABELS "integration;fluxscript;individual"
            TIMEOUT 30
        )
    endif()
endforeach()

# 
#  FluxScript Feature Tests (runs all .flux script tests)     
# 
if(BASH_EXECUTABLE)
    add_test(NAME flux-test-all-features
        COMMAND ${BASH_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/run_flux_tests.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
else()
    add_test(NAME flux-test-all-features
        COMMAND sh ${CMAKE_SOURCE_DIR}/tests/run_flux_tests.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
set_tests_properties(flux-test-all-features PROPERTIES
    ENVIRONMENT "FLUX=${CMAKE_BINARY_DIR}/flux"
)
set_tests_properties(flux-test-all-features PROPERTIES
    LABELS "fluxscript;features"
    TIMEOUT 600
)

# 
#  Set common test properties                                  
# 
# 
#  Runtime Subsystem Tests                                             
# 
add_test_executable(flux-test-runtime-autodiff unit/test_runtime_autodiff.cpp)
add_test_entry(flux-test-runtime-autodiff "Runtime: Autodiff")

add_test_executable(flux-test-runtime-units unit/test_runtime_units.cpp)
add_test_entry(flux-test-runtime-units "Runtime: Units")

add_test_executable(flux-test-runtime-mixed-signal unit/test_runtime_mixed_signal.cpp)
add_test_entry(flux-test-runtime-mixed-signal "Runtime: Mixed-Signal")

add_test_executable(flux-test-runtime-parallel unit/test_runtime_parallel.cpp)
add_test_entry(flux-test-runtime-parallel "Runtime: Parallel")

add_test_executable(flux-test-runtime-symbolic unit/test_runtime_symbolic.cpp)
add_test_entry(flux-test-runtime-symbolic "Runtime: Symbolic Engine")

add_test_executable(flux-test-advanced-math unit/test_advanced_math.cpp)
add_test_entry(flux-test-advanced-math "Advanced Math")

#
#  Tiered JIT
#
add_test_executable(flux-test-tiered-jit unit/test_tiered_jit.cpp)
add_test_entry(flux-test-tiered-jit "Tiered JIT")

add_test_executable(flux-test-type-system unit/test_type_system.cpp)
add_test_entry(flux-test-type-system "Type System")

add_test_executable(flux-test-persistent-cache unit/test_persistent_cache.cpp)
add_test_entry(flux-test-persistent-cache "Persistent JIT Cache")

#
#  LSP Server
#
add_test_executable(flux-test-lsp unit/test_lsp_server.cpp)
add_test_entry(flux-test-lsp "LSP Server")

add_test_executable(flux-test-vector-ops unit/test_vector_operations.cpp)
add_test_entry(flux-test-vector-ops "Vector Operations")

set_tests_properties(
    flux-test-phase5
    flux-test-phase7-1
    flux-test-phase7-4
    flux-test-phase7-4b
    flux-test-advanced-analysis
    flux-test-advanced-analysis-full
    flux-test-auto-docs
    flux-test-behavioral-sources
    flux-test-circuit-diff
    flux-test-circuit-doctor
    flux-test-circuit-fuzz
    flux-test-features-phase2
    flux-test-features-phase3
    flux-test-features-phase4a
    flux-test-features-4-5
    flux-test-ngspice
    flux-test-instruments
    flux-test-waveform-extraction
    flux-test-adevice-netlist
    flux-test-spice-bridge
    flux-test-phase2-behavioral
    flux-test-repro-issue
    flux-test-benchmark-jit
    flux-test-runtime-autodiff
    flux-test-runtime-units
    flux-test-runtime-mixed-signal
    flux-test-runtime-parallel
    flux-test-runtime-symbolic
    flux-test-advanced-math
    flux-test-tiered-jit
    flux-test-type-system
    flux-test-persistent-cache
    flux-test-lsp
    flux-test-vector-ops
    PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        TIMEOUT 120
        LABELS "unit"
)
