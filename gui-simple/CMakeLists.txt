cmake_minimum_required(VERSION 3.16)
project(FluxEditorGUI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt (same as VioSpice)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Include FluxScript headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Create GUI executable
add_executable(flux-editor
    main.cpp
)

# Disable deprecation warnings for Qt 6.10 compatibility
target_compile_options(flux-editor PRIVATE -Wno-deprecated-declarations)

target_link_libraries(flux-editor PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Core
)

message(STATUS "FluxEditorGUI: Using Qt version ${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}")
