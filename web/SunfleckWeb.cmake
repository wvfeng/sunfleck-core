include_guard(GLOBAL)
set(SUNFLECK_WEB_ASSETS "${CMAKE_CURRENT_LIST_DIR}")

# Consume the shared host from an application outside the framework checkout.
function(sunfleck_add_web_app target)
  cmake_parse_arguments(APP "" "SOURCE;FONT;ASSET_DIRECTORY" "" ${ARGN})
  if(NOT EMSCRIPTEN OR NOT EXISTS "${APP_SOURCE}" OR NOT EXISTS "${APP_FONT}")
    message(FATAL_ERROR "A Web app needs Emscripten, SOURCE and a licensed FONT")
  endif()
  add_executable(${target} "${APP_SOURCE}")
  target_link_libraries(${target} PRIVATE sunfleck)
  set_target_properties(${target} PROPERTIES OUTPUT_NAME sunfleck SUFFIX .js
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/web")
  target_link_options(${target} PRIVATE
    -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=web -sALLOW_MEMORY_GROWTH=1
    "-sEXPORTED_RUNTIME_METHODS=['HEAPU8','FS','UTF8ToString']"
    "--preload-file=${APP_FONT}@/sunfleck/font.ttf")
  foreach(asset index.html main.js host.js text-input.js renderer.js workspace.fleck message.txt)
    set(input "${SUNFLECK_WEB_ASSETS}/${asset}")
    if(NOT asset MATCHES "^(host|text-input|renderer)\\.js$" AND EXISTS "${APP_ASSET_DIRECTORY}/${asset}")
      set(input "${APP_ASSET_DIRECTORY}/${asset}")
    endif()
    configure_file("${input}" "${CMAKE_BINARY_DIR}/web/${asset}" COPYONLY)
  endforeach()
  configure_file("${CMAKE_BINARY_DIR}/SheenBidi-LICENSE.txt" "${CMAKE_BINARY_DIR}/web/SheenBidi-LICENSE.txt" COPYONLY)
endfunction()
