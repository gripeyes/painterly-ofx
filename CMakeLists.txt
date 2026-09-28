cmake_minimum_required(VERSION 3.25)
project(Pigment VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING
      "Minimum macOS version for the OFX module and metallib" FORCE)
endif()

option(PIGMENT_BUILD_OFX "Build the OpenFX bundle" ON)
option(PIGMENT_FETCH_OPENFX "Fetch OpenFX when OPENFX_ROOT is not supplied" ON)
option(PIGMENT_ENABLE_METAL "Build the native Apple Metal backend" ${APPLE})
option(PIGMENT_METAL_FAST_MATH "Allow Metal fast relaxed math" OFF)
option(PIGMENT_METAL_PROFILE_SOURCES "Record Metal shader sources for profiling" OFF)
set(OPENFX_ROOT "" CACHE PATH "Path to an OpenFX 1.5.1 source tree")

add_library(pigment_core STATIC
  src/core/ColorSpace.cpp
  src/core/Similarity.cpp
  src/core/Filtering.cpp
  src/core/Masking.cpp
  src/core/DirectionalGaussian.cpp
  src/core/RollingYabMass.cpp
  src/core/AcceleratedYabMass.cpp
  src/core/DetailCollapseResearch.cpp
  src/core/ChromaDiffusion.cpp)
target_include_directories(pigment_core PUBLIC src)
target_compile_features(pigment_core PUBLIC cxx_std_17)
target_compile_options(pigment_core PRIVATE
  $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>)

if(APPLE AND PIGMENT_ENABLE_METAL)
  enable_language(OBJCXX)
  find_program(PIGMENT_XCRUN xcrun REQUIRED)
  set(PIGMENT_METAL_AIR "${CMAKE_CURRENT_BINARY_DIR}/Pigment.air")
  set(PIGMENT_METALLIB "${CMAKE_CURRENT_BINARY_DIR}/Pigment.metallib")
  set(PIGMENT_METAL_FLAGS -std=macos-metal2.4
      "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  if(PIGMENT_METAL_FAST_MATH)
    list(APPEND PIGMENT_METAL_FLAGS -ffast-math)
  else()
    list(APPEND PIGMENT_METAL_FLAGS -fno-fast-math)
  endif()
  if(PIGMENT_METAL_PROFILE_SOURCES)
    list(APPEND PIGMENT_METAL_FLAGS -frecord-sources)
  endif()
  add_custom_command(OUTPUT "${PIGMENT_METALLIB}"
    COMMAND "${PIGMENT_XCRUN}" -sdk macosx metal ${PIGMENT_METAL_FLAGS}
            -c "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Pigment.metal"
            -o "${PIGMENT_METAL_AIR}"
    COMMAND "${PIGMENT_XCRUN}" -sdk macosx metallib
            "${PIGMENT_METAL_AIR}" -o "${PIGMENT_METALLIB}"
    DEPENDS shaders/Pigment.metal VERBATIM)
  add_custom_target(pigment_metallib DEPENDS "${PIGMENT_METALLIB}")

  add_library(pigment_metal STATIC src/metal/PigmentMetal.mm)
  target_include_directories(pigment_metal PUBLIC src)
  target_link_libraries(pigment_metal PUBLIC
    "-framework Foundation" "-framework Metal" "-framework MetalPerformanceShaders")
  target_compile_options(pigment_metal PRIVATE -fobjc-arc -Wall -Wextra -Wpedantic)
  target_compile_definitions(pigment_metal PUBLIC PIGMENT_ENABLE_METAL=1)
  add_dependencies(pigment_metal pigment_metallib)
  if(BUILD_TESTING)
    add_executable(pigment_metal_harness tests/MetalHarness.cpp)
    target_link_libraries(pigment_metal_harness PRIVATE pigment_metal pigment_core)
    add_test(NAME pigment_metal_smoke
      COMMAND pigment_metal_harness --verify --warmups 0 --rounds 1)
    set_tests_properties(pigment_metal_smoke PROPERTIES
      ENVIRONMENT "PIGMENT_METAL_RESOURCE_DIR=${CMAKE_CURRENT_BINARY_DIR}")
  endif()
endif()

if(BUILD_TESTING)
  add_executable(pigment_core_tests tests/CoreTests.cpp)
  target_link_libraries(pigment_core_tests PRIVATE pigment_core)
  add_test(NAME pigment_core_tests COMMAND pigment_core_tests)

  add_executable(pigment_mass_research tests/MassFormationHarness.cpp)
  target_link_libraries(pigment_mass_research PRIVATE pigment_core)

  add_executable(pigment_mass_tests tests/MassFormationTests.cpp)
  target_link_libraries(pigment_mass_tests PRIVATE pigment_core)
  add_test(NAME pigment_mass_tests COMMAND pigment_mass_tests)

  add_executable(pigment_detail_collapse_tests tests/DetailCollapseTests.cpp)
  target_link_libraries(pigment_detail_collapse_tests PRIVATE pigment_core)
  add_test(NAME pigment_detail_collapse_tests COMMAND pigment_detail_collapse_tests)

  add_executable(pigment_backend_benchmark tests/BackendBenchmark.cpp)
  target_link_libraries(pigment_backend_benchmark PRIVATE pigment_core)
endif()

if(PIGMENT_BUILD_OFX)
  if(NOT OPENFX_ROOT)
    if(NOT PIGMENT_FETCH_OPENFX)
      message(FATAL_ERROR "Set OPENFX_ROOT or enable PIGMENT_FETCH_OPENFX")
    endif()
    FetchContent_Declare(openfx
      GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openfx.git
      GIT_TAG ab779510b2655b4d11a7e01e5c521f9aa8c88976
      GIT_SHALLOW TRUE
      SOURCE_SUBDIR pigment_dependency_only)
    FetchContent_MakeAvailable(openfx)
    set(OPENFX_ROOT "${openfx_SOURCE_DIR}")
  endif()

  if(NOT EXISTS "${OPENFX_ROOT}/include/ofxCore.h" OR
     NOT EXISTS "${OPENFX_ROOT}/Support/include/ofxsImageEffect.h")
    message(FATAL_ERROR "OPENFX_ROOT does not contain the OpenFX API and support library")
  endif()

  file(GLOB OPENFX_SUPPORT_SOURCES CONFIGURE_DEPENDS
    "${OPENFX_ROOT}/Support/Library/*.cpp")
  add_library(pigment_openfx_support STATIC ${OPENFX_SUPPORT_SOURCES})
  target_include_directories(pigment_openfx_support PUBLIC
    "${OPENFX_ROOT}/include" "${OPENFX_ROOT}/Support/include")
  target_compile_features(pigment_openfx_support PUBLIC cxx_std_17)
  target_compile_options(pigment_openfx_support PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wno-deprecated-declarations>)

  add_library(Pigment MODULE
    src/ofx/ParameterHelpers.cpp
    src/ofx/OfxImageHelpers.cpp
    src/plugins/ChromaDiffusion.cpp
    src/plugins/DetailCollapse.cpp
    src/ofx/PluginMain.cpp)
  target_link_libraries(Pigment PRIVATE pigment_core pigment_openfx_support)
  if(TARGET pigment_metal)
    target_link_libraries(Pigment PRIVATE pigment_metal)
  endif()
  target_include_directories(Pigment PRIVATE src)
  target_compile_options(Pigment PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>)
  set_target_properties(Pigment PROPERTIES
    PREFIX "" SUFFIX ".ofx"
    C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)

  if(APPLE)
    target_link_options(Pigment PRIVATE
      "LINKER:-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/cmake/Pigment.exports")
    set(PIGMENT_BUNDLE_DIR "${CMAKE_BINARY_DIR}/Pigment.ofx.bundle")
    set(PIGMENT_BUNDLE_MACOS "${PIGMENT_BUNDLE_DIR}/Contents/MacOS")
    set(PIGMENT_BUNDLE_RESOURCES "${PIGMENT_BUNDLE_DIR}/Contents/Resources")
    configure_file(cmake/Info.plist.in "${PIGMENT_BUNDLE_DIR}/Contents/Info.plist" @ONLY)
    add_custom_command(TARGET Pigment POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${PIGMENT_BUNDLE_MACOS}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "$<TARGET_FILE:Pigment>" "${PIGMENT_BUNDLE_MACOS}/Pigment.ofx"
      VERBATIM)
    if(TARGET pigment_metallib)
      add_custom_command(TARGET Pigment POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${PIGMENT_BUNDLE_RESOURCES}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${PIGMENT_METALLIB}" "${PIGMENT_BUNDLE_RESOURCES}/Pigment.metallib"
        VERBATIM)
    endif()
    add_custom_target(pigment_bundle ALL DEPENDS Pigment)
    add_custom_target(sign-local
      COMMAND codesign --force --sign - "${PIGMENT_BUNDLE_DIR}"
      DEPENDS pigment_bundle VERBATIM)
    add_custom_target(stage
      COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CMAKE_BINARY_DIR}/stage/Pigment.ofx.bundle"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/stage"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
              "${PIGMENT_BUNDLE_DIR}" "${CMAKE_BINARY_DIR}/stage/Pigment.ofx.bundle"
      DEPENDS pigment_bundle VERBATIM)
    install(DIRECTORY "${PIGMENT_BUNDLE_DIR}"
      DESTINATION "." USE_SOURCE_PERMISSIONS)
  endif()
endif()
