cmake_minimum_required(VERSION 3.20)

project(media-capture VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)
include(cmake/Dependencies.cmake)

option(MEDIA_CAPTURE_BUILD_EXAMPLES "Build media-capture examples" ON)
option(MEDIA_CAPTURE_BUILD_TESTS "Build media-capture tests" ON)

add_library(media_capture_miniaudio OBJECT "${MEDIA_CAPTURE_MINIAUDIO_SOURCE_DIR}/miniaudio.c")
target_include_directories(media_capture_miniaudio
  SYSTEM PUBLIC "$<BUILD_INTERFACE:${MEDIA_CAPTURE_MINIAUDIO_SOURCE_DIR}>"
)
set_target_properties(media_capture_miniaudio PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(media_capture_audio STATIC
  src/audio/miniaudio_audio_capture.cpp
  $<TARGET_OBJECTS:media_capture_miniaudio>
)
add_library(media_capture::audio ALIAS media_capture_audio)
target_compile_features(media_capture_audio PUBLIC cxx_std_20)
target_include_directories(media_capture_audio
  PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE "${MEDIA_CAPTURE_MINIAUDIO_SOURCE_DIR}"
)
if(UNIX AND NOT APPLE)
  target_link_libraries(media_capture_audio PRIVATE ${CMAKE_DL_LIBS} pthread m)
endif()
set_target_properties(media_capture_audio PROPERTIES
  EXPORT_NAME audio
  POSITION_INDEPENDENT_CODE ON
)

if(MEDIA_CAPTURE_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(BUILD_TESTING AND MEDIA_CAPTURE_BUILD_TESTS)
  add_subdirectory(tests)
endif()

install(TARGETS media_capture_audio
  EXPORT media-capture-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT media-capture-targets
  FILE media-capture-targets.cmake
  NAMESPACE media_capture::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/media-capture
)

configure_package_config_file(
  cmake/media-capture-config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/media-capture-config.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/media-capture
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/media-capture-config-version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/media-capture-config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/media-capture-config-version.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/media-capture
)
