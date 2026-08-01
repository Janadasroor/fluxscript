cmake_minimum_required(VERSION 3.20)
project(flux_repro LANGUAGES CXX)

# Resolve LLVM exactly the way the main flux build does, so include/lib
# paths cannot drift. Pass -DLLVM_DIR=<archive>/lib/cmake/llvm on the CLI.
find_package(LLVM REQUIRED CONFIG)
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")

llvm_map_components_to_libnames(REPRO_LLVM_LIBS core support)

add_executable(repro repro_fpconstants.cpp)
target_include_directories(repro PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(repro PRIVATE ${REPRO_LLVM_LIBS})
