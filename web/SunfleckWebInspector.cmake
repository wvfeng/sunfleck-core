include_guard(GLOBAL)

# The host loads the transport on loopback when the page opts into inspection.
function(sunfleck_enable_web_inspector target output_directory)
  if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "sunfleck_enable_web_inspector requires Emscripten")
  endif()
  target_sources(${target} PRIVATE "$<$<CONFIG:Debug>:${CMAKE_CURRENT_FUNCTION_LIST_DIR}/inspector-exports.cpp>")
  target_compile_definitions(${target} PRIVATE "$<$<CONFIG:Debug>:SUNFLECK_WEB_DEBUG>")
  if(CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/inspector.js" "${output_directory}/inspector.js" COPYONLY)
  endif()
endfunction()
