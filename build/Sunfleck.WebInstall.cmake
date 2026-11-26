include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(sunfleck_cmake_dir "${CMAKE_INSTALL_LIBDIR}/cmake/Sunfleck")
set(sunfleck_web_dir "${CMAKE_INSTALL_DATADIR}/sunfleck/web")
install(TARGETS sunfleck EXPORT SunfleckTargets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/sunfleck" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(EXPORT SunfleckTargets NAMESPACE Sunfleck:: DESTINATION "${sunfleck_cmake_dir}")
configure_package_config_file("${PROJECT_SOURCE_DIR}/build/SunfleckConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/SunfleckConfig.cmake" INSTALL_DESTINATION "${sunfleck_cmake_dir}")
write_basic_package_version_file("${PROJECT_BINARY_DIR}/SunfleckConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMinorVersion)
install(FILES "${PROJECT_BINARY_DIR}/SunfleckConfig.cmake"
  "${PROJECT_BINARY_DIR}/SunfleckConfigVersion.cmake"
  "${PROJECT_SOURCE_DIR}/build/Sunfleck.Development.cmake"
  "${PROJECT_SOURCE_DIR}/build/Sunfleck.MsvcDependencies.cmake" DESTINATION "${sunfleck_cmake_dir}")
install(FILES "${PROJECT_SOURCE_DIR}/web/host.js" "${PROJECT_SOURCE_DIR}/web/renderer.js"
  "${PROJECT_SOURCE_DIR}/web/text-input.js" "${PROJECT_SOURCE_DIR}/web/inspector.js"
  "${PROJECT_SOURCE_DIR}/web/inspector-exports.cpp" "${PROJECT_SOURCE_DIR}/web/SunfleckWebInspector.cmake"
  "${PROJECT_SOURCE_DIR}/web/preview.py" "${PROJECT_BINARY_DIR}/SheenBidi-LICENSE.txt"
  DESTINATION "${sunfleck_web_dir}")
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION "${sunfleck_web_dir}" RENAME Sunfleck-LICENSE.txt)
