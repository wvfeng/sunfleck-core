cmake_minimum_required(VERSION 3.25)
project(CounterCpp LANGUAGES C CXX)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  include(CTest)
  set(SUNFLECK_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." CACHE PATH "Sunfleck source directory")
  include("${SUNFLECK_SOURCE_DIR}/build/Sunfleck.MsvcDependencies.cmake")
  block()
    set(UI_BUILD_EXAMPLES OFF)
    set(UI_BUILD_DEMOS OFF)
    set(UI_BUILD_CSHARP OFF)
    set(BUILD_TESTING OFF)
    add_subdirectory("${SUNFLECK_SOURCE_DIR}" sunfleck EXCLUDE_FROM_ALL)
  endblock()
endif()

add_executable(CounterCpp main.cpp)
set(bindings "${CMAKE_CURRENT_BINARY_DIR}/counter_bindings.hpp")
add_custom_command(OUTPUT "${bindings}"
  COMMAND fleckc --format cpp --symbol counter_cpp "${CMAKE_CURRENT_SOURCE_DIR}/main.fleck" "${bindings}"
  DEPENDS fleckc "${CMAKE_CURRENT_SOURCE_DIR}/main.fleck" VERBATIM)
add_custom_target(counter_cpp_bindings DEPENDS "${bindings}")
add_dependencies(CounterCpp counter_cpp_bindings)
target_include_directories(CounterCpp PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(CounterCpp PRIVATE sunfleck)
target_compile_features(CounterCpp PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(CounterCpp PRIVATE /utf-8)
endif()

# The exporter uses the same declarations as the running application, without a window.
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/CounterCpp.fleckmodel.json"
  COMMAND CounterCpp --export "${CMAKE_CURRENT_BINARY_DIR}/CounterCpp.fleckmodel.json"
  DEPENDS CounterCpp VERBATIM)
add_custom_target(counter_cpp_metadata ALL
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/CounterCpp.fleckmodel.json")

if(BUILD_TESTING)
  add_executable(counter_cpp_demo_test tests.cpp)
  add_dependencies(counter_cpp_demo_test counter_cpp_bindings)
  target_include_directories(counter_cpp_demo_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
  target_link_libraries(counter_cpp_demo_test PRIVATE sunfleck)
  target_compile_features(counter_cpp_demo_test PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(counter_cpp_demo_test PRIVATE /utf-8)
  endif()
  add_test(NAME counter_cpp_demo COMMAND counter_cpp_demo_test "${CMAKE_CURRENT_SOURCE_DIR}/main.fleck")
endif()
