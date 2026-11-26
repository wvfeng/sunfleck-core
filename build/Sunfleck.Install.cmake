include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS ${sunfleck_targets} EXPORT SunfleckTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(TARGET fleckc)
  install(TARGETS fleckc EXPORT SunfleckTargets RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/sunfleck" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES "${PROJECT_SOURCE_DIR}/bindings/csharp/Elements.cs" DESTINATION "${CMAKE_INSTALL_DATADIR}/sunfleck/types")
install(FILES "${PROJECT_SOURCE_DIR}/bindings/csharp/Components.cs" DESTINATION "${CMAKE_INSTALL_DATADIR}/sunfleck/types")
if(UI_BUILD_CSHARP)
  # Include the complete managed reference source so ordinary C# navigation
  # can resolve partial classes and continue through the public type hierarchy.
  install(DIRECTORY "${PROJECT_SOURCE_DIR}/bindings/csharp/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/sunfleck/types"
    FILES_MATCHING PATTERN "*.cs" PATTERN "bin" EXCLUDE PATTERN "obj" EXCLUDE)
  install(FILES "${PROJECT_SOURCE_DIR}/build/Sunfleck.Navigation.csproj"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/sunfleck/types")
endif()
foreach(language IN ITEMS c cpp csharp)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/bindings/types/${language}.json")
  file(READ "${PROJECT_SOURCE_DIR}/bindings/types/${language}.json" type_catalog)
  if(language STREQUAL "csharp")
    set(type_source "Elements.cs")
    set(component_source "Components.cs")
  elseif(language STREQUAL "c")
    set(type_source "../../../${CMAKE_INSTALL_INCLUDEDIR}/sunfleck/elements.h")
    set(component_source "../../../${CMAKE_INSTALL_INCLUDEDIR}/sunfleck/component_attributes.h")
  else()
    set(type_source "../../../${CMAKE_INSTALL_INCLUDEDIR}/sunfleck/elements.hpp")
    set(component_source "../../../${CMAKE_INSTALL_INCLUDEDIR}/sunfleck/components.hpp")
  endif()
  string(JSON type_catalog SET "${type_catalog}" source "\"${type_source}\"")
  foreach(index RANGE 0 1)
    string(JSON type_catalog SET "${type_catalog}" componentAttributes ${index} source "\"${component_source}\"")
  endforeach()
  file(WRITE "${PROJECT_BINARY_DIR}/types/${language}.json" "${type_catalog}\n")
  install(FILES "${PROJECT_BINARY_DIR}/types/${language}.json" DESTINATION "${CMAKE_INSTALL_DATADIR}/sunfleck/types")
endforeach()
set(sunfleck_cmake_dir "${CMAKE_INSTALL_LIBDIR}/cmake/Sunfleck")
configure_package_config_file("${PROJECT_SOURCE_DIR}/build/SunfleckConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/SunfleckConfig.cmake" INSTALL_DESTINATION "${sunfleck_cmake_dir}")
write_basic_package_version_file("${PROJECT_BINARY_DIR}/SunfleckConfigVersion.cmake"
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(EXPORT SunfleckTargets NAMESPACE Sunfleck:: DESTINATION "${sunfleck_cmake_dir}")
install(FILES "${PROJECT_BINARY_DIR}/SunfleckConfig.cmake"
  "${PROJECT_BINARY_DIR}/SunfleckConfigVersion.cmake"
  "${PROJECT_SOURCE_DIR}/build/Sunfleck.Development.cmake"
  "${PROJECT_SOURCE_DIR}/build/Sunfleck.MsvcDependencies.cmake" DESTINATION "${sunfleck_cmake_dir}")
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/docs/sdk.md"
  "${PROJECT_SOURCE_DIR}/docs/distribution.md" "${PROJECT_SOURCE_DIR}/docs/performance.md"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/sunfleck")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/tests/sdk/consumer/"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/sunfleck/examples" PATTERN "bin" EXCLUDE PATTERN "obj" EXCLUDE)

if(UI_BUILD_CSHARP)
  set(sdk_managed_output "${PROJECT_BINARY_DIR}/sdk-managed/$<CONFIG>")
  add_custom_target(sunfleck_sdk_managed ALL
    COMMAND "${DOTNET_EXECUTABLE}" build "${PROJECT_SOURCE_DIR}/bindings/csharp/Sunfleck.csproj"
      --configuration "$<IF:$<CONFIG:Debug>,Debug,Release>"
      --artifacts-path "${PROJECT_BINARY_DIR}/sdk-managed/artifacts"
      --output "${sdk_managed_output}" --nologo
    COMMAND "${DOTNET_EXECUTABLE}" build "${PROJECT_SOURCE_DIR}/tools/component-generator/Sunfleck.ComponentGenerator.csproj"
      --configuration "$<IF:$<CONFIG:Debug>,Debug,Release>" --output "${sdk_managed_output}/components" --nologo
    DEPENDS sunfleck_native VERBATIM)
  install(DIRECTORY "${sdk_managed_output}/components/" DESTINATION "${CMAKE_INSTALL_LIBDIR}/sunfleck/components")
  install(FILES "${PROJECT_SOURCE_DIR}/build/Sunfleck.Components.targets" DESTINATION "${CMAKE_INSTALL_DATADIR}/sunfleck")
  install(FILES "${sdk_managed_output}/Sunfleck.dll" DESTINATION "${CMAKE_INSTALL_LIBDIR}/net10.0")
  configure_file("${PROJECT_SOURCE_DIR}/build/Sunfleck.props.in" "${PROJECT_BINARY_DIR}/Sunfleck.props" @ONLY)
  install(FILES "${PROJECT_BINARY_DIR}/Sunfleck.props" DESTINATION "${CMAKE_INSTALL_DATADIR}/sunfleck")
endif()

if(UI_ENABLE_SKIA_D3D12)
  foreach(library skia skcms d3d12allocator spirv_cross)
    install(FILES "${UI_SKIA_LIBRARY_DIR}/${library}.lib" DESTINATION "${CMAKE_INSTALL_LIBDIR}/sunfleck/skia")
  endforeach()
  install(FILES "${PROJECT_BINARY_DIR}/skia-source-candidate.json"
    "${PROJECT_SOURCE_DIR}/tools/development/skia-spike/d3d12-fixes.patch"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/sunfleck/skia")
  install(FILES "${UI_SKIA_ROOT}/LICENSE" DESTINATION "${CMAKE_INSTALL_DATADIR}/sunfleck/licenses/skia")
  install(FILES "${UI_SKIA_ROOT}/modules/skcms/LICENSE" DESTINATION "${CMAKE_INSTALL_DATADIR}/sunfleck/licenses/skcms")
  install(FILES "${UI_SKIA_ROOT}/third_party/externals/d3d12allocator/LICENSE.txt" DESTINATION "${CMAKE_INSTALL_DATADIR}/sunfleck/licenses/d3d12allocator")
  install(FILES "${UI_SKIA_ROOT}/third_party/externals/spirv-cross/LICENSE" DESTINATION "${CMAKE_INSTALL_DATADIR}/sunfleck/licenses/spirv-cross")
  install(DIRECTORY "${UI_SKIA_ROOT}/third_party/externals/spirv-cross/LICENSES/" DESTINATION "${CMAKE_INSTALL_DATADIR}/sunfleck/licenses/spirv-cross/LICENSES")
endif()
