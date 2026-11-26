set(UI_WEB_FONT "" CACHE FILEPATH "Font to package at /sunfleck/font.ttf (TTF/OTF)")
if(NOT EXISTS "${UI_WEB_FONT}")
  message(FATAL_ERROR "Set UI_WEB_FONT to a licensed TTF/OTF font; see docs/web.md")
endif()
include(SunfleckWeb.cmake)
sunfleck_add_web_app(sunfleck_web SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
  FONT "${UI_WEB_FONT}" ASSET_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
# The DOM loads the same bytes from the preloaded filesystem; no second font download.

# A filesystem-free Wasm semantic check for component registries and embedded resources.
# Explicit target keeps the normal browser delivery small.
add_executable(component_runtime_check EXCLUDE_FROM_ALL "${PROJECT_SOURCE_DIR}/tests/components.cpp")
target_include_directories(component_runtime_check PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(component_runtime_check PRIVATE sunfleck)
target_link_options(component_runtime_check PRIVATE -sENVIRONMENT=node -sFILESYSTEM=0 -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1)
