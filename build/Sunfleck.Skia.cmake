# Opt-in Windows integration of the fixed, patched Ganesh D3D12 candidate.
if(NOT WIN32)
  message(FATAL_ERROR "UI_ENABLE_SKIA_D3D12 requires Windows")
endif()
set(UI_SKIA_ROOT "" CACHE PATH "Fixed Skia checkout with the validated D3D12 patch")
set(UI_SKIA_LIBRARY_DIR "${UI_SKIA_ROOT}/out/sunfleck-d3d12" CACHE PATH "Release /MD Skia libraries")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/development/skia-spike/prepare-d3d12.py"
  --skia "${UI_SKIA_ROOT}" RESULT_VARIABLE skia_status OUTPUT_VARIABLE skia_identity ERROR_VARIABLE skia_error)
if(NOT skia_status EQUAL 0)
  message(FATAL_ERROR "Skia D3D12 source candidate rejected: ${skia_error}")
endif()
file(WRITE "${PROJECT_BINARY_DIR}/skia-source-candidate.json" "${skia_identity}")
add_library(Sunfleck::Skia STATIC IMPORTED GLOBAL)
set_target_properties(Sunfleck::Skia PROPERTIES IMPORTED_LOCATION "${UI_SKIA_LIBRARY_DIR}/skia.lib"
  INTERFACE_INCLUDE_DIRECTORIES "${UI_SKIA_ROOT}"
  INTERFACE_COMPILE_DEFINITIONS "SK_GANESH;SK_DIRECT3D;SK_RELEASE"
  INTERFACE_LINK_LIBRARIES "${UI_SKIA_LIBRARY_DIR}/skcms.lib;${UI_SKIA_LIBRARY_DIR}/d3d12allocator.lib;${UI_SKIA_LIBRARY_DIR}/spirv_cross.lib;d3d12;dxgi;d3dcompiler;fontsub;usp10;ole32")
# The pinned official Skia build uses /MD, including when the caller chooses Debug.
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
