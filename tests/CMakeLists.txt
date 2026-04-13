# =============================================================================
# FluxScript Comprehensive Test Suite
# Registers ALL test files as CMake targets + CTest entries
# =============================================================================

# 
#  Phase 2: Behavioral Modeling                                
#  (target defined in main CMakeLists.txt)                     
# 
add_test(NAME flux-test-phase2
    COMMAND flux-test-phase2
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(flux-test-phase2 PROPERTIES
    LABELS "phase2;behavioral"
    TIMEOUT 120
)

# 
#  Phase 5: Component Substitution                             
# 
add_test_executable(flux-test-phase5 test_phase5.cpp)
add_test_entry(flux-test-phase5 "Phase 5: Component Substitution")

# 
#  Phase 7.1: Model Quality & Verification                     
#  (target defined in main CMakeLists.txt)                     
# 
add_test(NAME flux-test-phase7-1
    COMMAND flux-test-phase7-1
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(flux-test-phase7-1 PROPERTIES
    LABELS "phase7-1;model-quality"
    TIMEOUT 120
)

# 
#  Phase 7.4: Tooling & Developer Experience                   
#  (targets defined in main CMakeLists.txt)                    
# 
add_test(NAME flux-test-phase7-4
    COMMAND flux-test-phase7-4
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(flux-test-phase7-4 PROPERTIES
    LABELS "phase7-4;tooling"
    TIMEOUT 120
)

add_test(NAME flux-test-phase7-4b
    COMMAND flux-test-phase7-4b
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(flux-test-phase7-4b PROPERTIES
    LABELS "phase7-4b;tooling"
    TIMEOUT 120
)

# 
#  Advanced Analysis Tools                                     
# 
add_test_executable(flux-test-advanced-analysis test_advanced_analysis.cpp)
add_test_entry(flux-test-advanced-analysis "Advanced Analysis Tools")

add_test_executable(flux-test-advanced-analysis-full test_advanced_analysis_full.cpp)
add_test_entry(flux-test-advanced-analysis-full "Advanced Analysis Full")

# 
#  Auto Documentation                                          
# 
add_test_executable(flux-test-auto-docs test_auto_docs.cpp)
add_test_entry(flux-test-auto-docs "Auto Documentation")

# 
#  Behavioral Sources                                          
# 
add_test_executable(flux-test-behavioral-sources test_behavioral_sources.cpp)
add_test_entry(flux-test-behavioral-sources "Behavioral Sources")

# 
#  Circuit Diff                                                
# 
add_test_executable(flux-test-circuit-diff test_circuit_diff.cpp)
add_test_entry(flux-test-circuit-diff "Circuit Diff")

# 
#  Circuit Doctor                                              
# 
add_test_executable(flux-test-circuit-doctor test_circuit_doctor.cpp)
add_test_entry(flux-test-circuit-doctor "Circuit Doctor")

# 
#  Circuit Fuzz Testing                                        
# 
add_test_executable(flux-test-circuit-fuzz test_circuit_fuzz.cpp)
add_test_entry(flux-test-circuit-fuzz "Circuit Fuzz Testing")

# 
#  Feature Tests (Phases 2-4a)                                 
# 
add_test_executable(flux-test-features-phase2 test_features_phase2.cpp)
add_test_entry(flux-test-features-phase2 "Features Phase 2")

add_test_executable(flux-test-features-phase3 test_features_phase3.cpp)
add_test_entry(flux-test-features-phase3 "Features Phase 3")

add_test_executable(flux-test-features-phase4a test_features_phase4a.cpp)
add_test_entry(flux-test-features-phase4a "Features Phase 4a")

# 
#  Features 4 & 5 (Power Electronics, Op-Amp)                  
# 
add_test_executable(flux-test-features-4-5 test_features_4_5.cpp)
add_test_entry(flux-test-features-4-5 "Features 4 & 5")

# 
#  ngspice Integration (requires ngspice installed)            
# 
add_test_executable(flux-test-ngspice test_ngspice.cpp)
add_test_entry(flux-test-ngspice "ngspice Integration")

# 
#  Instrument Control                                          
# 
add_test_executable(flux-test-instruments test_instruments.cpp)
add_test_entry(flux-test-instruments "Instrument Control")

# 
#  Waveform Extraction                                         
# 
add_test_executable(flux-test-waveform-extraction test_waveform_extraction.cpp)
add_test_entry(flux-test-waveform-extraction "Waveform Extraction")

# 
#  Benchmark Suite (real FluxScript JIT)                       
#  flux-benchmark is defined in main CMakeLists.txt            
# 

# 
#  Module System (import)                                      
# 
add_test_executable(flux-test-module-system test_module_system.cpp)

add_test(NAME flux-test-module-system
    COMMAND flux-test-module-system
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/examples
)
set_tests_properties(flux-test-module-system PROPERTIES
    LABELS "module-system;import;compiler;unit"
    TIMEOUT 60
)

# 
#  Inline .flux file tests                                     
# 
# flux-test discovers and runs # TEST: assertions in .flux files
add_test(NAME flux-test-inline
    COMMAND flux-test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(flux-test-inline PROPERTIES
    LABELS "inline;compiler"
    TIMEOUT 120
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
#  Set common test properties                                  
# 
set_tests_properties(
    flux-test-phase2
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
    PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        TIMEOUT 120
        LABELS "unit"
)
