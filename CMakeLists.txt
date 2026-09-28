cmake_minimum_required(VERSION 3.25)
project(Pigment VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(PIGMENT_BUILD_OFX "Build the OpenFX bundle" ON)
option(PIGMENT_FETCH_OPENFX "Fetch OpenFX when OPENFX_ROOT is not supplied" ON)
set(OPENFX_ROOT "" CACHE PATH "Path to an OpenFX 1.5.1 source tree")

add_library(pigment_core STATIC
  src/core/ColorSpace.cpp
  src/core/Similarity.cpp
  src/core/Filtering.cpp
  src/core/Masking.cpp
  src/core/DirectionalGaussian.cpp
  src/core/RollingYabMass.cpp
  src/core/DetailCollapseResearch.cpp
  src/core/ChromaDiffusion.cpp)
target_include_directories(pigment_core PUBLIC src)
target_compile_features(pigment_core PUBLIC cxx_std_17)
target_compile_options(pigment_core PRIVATE
  $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>)

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
    configure_file(cmake/Info.plist.in "${PIGMENT_BUNDLE_DIR}/Contents/Info.plist" @ONLY)
    add_custom_command(TARGET Pigment POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${PIGMENT_BUNDLE_MACOS}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "$<TARGET_FILE:Pigment>" "${PIGMENT_BUNDLE_MACOS}/Pigment.ofx"
      VERBATIM)
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
