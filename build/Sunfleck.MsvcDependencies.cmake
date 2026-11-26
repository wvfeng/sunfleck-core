# Preserve the compiler's actual /showIncludes bytes. CMake's AUTO decoding can
# corrupt localized prefixes when the console and system code pages differ,
# causing Ninja to silently lose all header dependencies.
if(MSVC AND CMAKE_GENERATOR MATCHES "^Ninja")
  set(probe_compiler "${CMAKE_C_COMPILER}")
  if(NOT probe_compiler)
    set(probe_compiler "${CMAKE_CXX_COMPILER}")
  endif()
  set(probe_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/SunfleckShowIncludes")
  file(MAKE_DIRECTORY "${probe_dir}")
  file(WRITE "${probe_dir}/dependency.h" "\n")
  file(WRITE "${probe_dir}/probe.c" "#include \"dependency.h\"\nint main(void) { return 0; }\n")
  execute_process(
    COMMAND "${probe_compiler}" /nologo /showIncludes /c probe.c
    WORKING_DIRECTORY "${probe_dir}"
    OUTPUT_VARIABLE probe_output
    ERROR_VARIABLE probe_error
    RESULT_VARIABLE probe_result
    ENCODING NONE)
  if(probe_result EQUAL 0 AND "${probe_output}" MATCHES "(^|\n)([^:\n][^:\n]+:[^:\n]*[^: \n][^: \n]:?[ \t]+)([A-Za-z]:\\\\|\\./|\\.\\\\|/)")
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${CMAKE_MATCH_2}")
  else()
    message(FATAL_ERROR "Could not detect the MSVC /showIncludes prefix: ${probe_error}")
  endif()
endif()
