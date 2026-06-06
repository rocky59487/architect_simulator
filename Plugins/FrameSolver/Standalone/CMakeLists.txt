cmake_minimum_required(VERSION 3.20)
project(FrameStandalone CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(EIGEN_MPL2_ONLY)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# UE-bundled Eigen 3.4.0 (header-only). Override with -DEIGEN_DIR=... if needed.
set(EIGEN_DIR "E:/project/UE_5.7/Engine/Source/ThirdParty/Eigen" CACHE PATH "Eigen include root")

set(CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Source/FrameCore")
file(GLOB CORE_SRC "${CORE_DIR}/Private/*.cpp")
list(FILTER CORE_SRC EXCLUDE REGEX "FrameCoreModule\\.cpp$")   # UE-only TU

add_executable(frametest ${CORE_SRC} "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
target_include_directories(frametest PRIVATE
    "${EIGEN_DIR}"
    "${CORE_DIR}/Public"
    "${CORE_DIR}/Private")

# Build:  cmake -S Standalone -B Standalone/build  &&  cmake --build Standalone/build
# Run:    Standalone/build/frametest    (or .../Debug/frametest.exe with MSVC generator)
