find_program(DOTNET_EXECUTABLE dotnet REQUIRED)

if(BUILD_TESTING)
  set(refresh_project "${PROJECT_SOURCE_DIR}/tests/development/fixtures/refresh-state/RefreshState.csproj")
  set(refresh_output "${CMAKE_CURRENT_BINARY_DIR}/managed/refresh-state")
  add_custom_target(refresh_state_csharp ALL
    COMMAND "${DOTNET_EXECUTABLE}" build "${refresh_project}"
      --configuration "$<IF:$<CONFIG:Debug>,Debug,Release>"
      --artifacts-path "${CMAKE_CURRENT_BINARY_DIR}/managed/artifacts"
      --output "${refresh_output}" --nologo
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:sunfleck_native>" "${refresh_output}"
    DEPENDS sunfleck_native VERBATIM)
  add_test(NAME refresh_state_csharp COMMAND "${DOTNET_EXECUTABLE}" "${refresh_output}/RefreshState.dll")
endif()
