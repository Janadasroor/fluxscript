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
add_test_entry(flux-test-benchmark-suite "Comprehensive Benchmark")
# 

# 
#  Module System (import)                                      
# 
add_test_executable(flux-test-module-system unit/test_module_system.cpp)

add_test(NAME flux-test-module-system
    COMMAND flux-test-module-system
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/examples
)
set_tests_properties(flux-test-module-system PROPERTIES
    LABELS "module-system;import;compiler;unit"
    TIMEOUT 60
)

# 
#  Integration Tests (.flux snippets via flux-minimal)         
# 
add_test(NAME flux-test-integration
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/tests/integration/run_tests.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(flux-test-integration PROPERTIES
    LABELS "integration;compiler"
    TIMEOUT 120
)

# 
#  FluxScript Feature Tests (runs all .flux script tests)     
# 
add_test(NAME flux-test-all-features
    COMMAND sh ${CMAKE_SOURCE_DIR}/tests/run_flux_tests.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
)
set_tests_properties(flux-test-all-features PROPERTIES
    ENVIRONMENT "FLUX=${CMAKE_BINARY_DIR}/flux"
)
set_tests_properties(flux-test-all-features PROPERTIES
    LABELS "fluxscript;features"
    TIMEOUT 120
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
    flux-test-benchmark-suite
    flux-test-runtime-autodiff
    flux-test-runtime-units
    flux-test-runtime-mixed-signal
    flux-test-runtime-parallel
    flux-test-runtime-symbolic
    flux-test-advanced-math
    PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        TIMEOUT 120
        LABELS "unit"
)
