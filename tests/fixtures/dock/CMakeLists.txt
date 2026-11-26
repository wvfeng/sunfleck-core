set(page "${CMAKE_CURRENT_SOURCE_DIR}/workspace.fleck")
set(generated "${CMAKE_CURRENT_BINARY_DIR}/workspace_page.c")
add_custom_command(OUTPUT "${generated}"
  COMMAND fleckc --format c --symbol load_dock_workspace "${page}" "${generated}"
  DEPENDS fleckc "${page}" VERBATIM)

foreach(variant IN ITEMS code fleck)
  add_executable(dock_cpp_${variant} main.cpp)
  target_link_libraries(dock_cpp_${variant} PRIVATE sunfleck)
  target_compile_features(dock_cpp_${variant} PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(dock_cpp_${variant} PRIVATE /utf-8)
  endif()
  if(variant STREQUAL "fleck")
    target_sources(dock_cpp_${variant} PRIVATE "${generated}")
    target_compile_definitions(dock_cpp_${variant} PRIVATE DOCK_EXAMPLE_FLECK)
  endif()
  add_test(NAME dock_cpp_${variant} COMMAND dock_cpp_${variant})
endforeach()
