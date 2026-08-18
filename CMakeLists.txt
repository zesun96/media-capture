cmake_minimum_required(VERSION 3.20)

project(media-capture VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

option(MEDIA_CAPTURE_BUILD_EXAMPLES "Build media-capture examples" ON)
option(MEDIA_CAPTURE_BUILD_TESTS "Build media-capture tests" ON)
if(WIN32)
  set(MEDIA_CAPTURE_BUILD_SCREEN_DEFAULT ON)
else()
  set(MEDIA_CAPTURE_BUILD_SCREEN_DEFAULT OFF)
endif()
option(MEDIA_CAPTURE_BUILD_SCREEN "Build screen and window source enumeration"
  ${MEDIA_CAPTURE_BUILD_SCREEN_DEFAULT}
)

include(cmake/Dependencies.cmake)

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

if(MEDIA_CAPTURE_BUILD_SCREEN)
  set(MEDIA_CAPTURE_SCREEN_SOURCES
    src/screen/screen_source.cpp
    src/screen/screen_capture.cpp
    ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/src_cpp/ScreenCapture.c
    ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/src_cpp/ScreenCapture.cpp
    ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/src_cpp/SCCommon.cpp
    ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/src_cpp/ThreadManager.cpp
  )
  if(WIN32)
    list(APPEND MEDIA_CAPTURE_SCREEN_SOURCES
      ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/src_cpp/windows/DXFrameProcessor.cpp
      ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/src_cpp/windows/GDIFrameProcessor.cpp
      ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/src_cpp/windows/GDIMouseProcessor.cpp
      ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/src_cpp/windows/GetMonitors.cpp
      ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/src_cpp/windows/GetWindows.cpp
      ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/src_cpp/windows/ThreadRunner.cpp
    )
  elseif(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    list(APPEND MEDIA_CAPTURE_SCREEN_SOURCES
      ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/src_cpp/linux/X11FrameProcessor.cpp
      ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/src_cpp/linux/X11MouseProcessor.cpp
      ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/src_cpp/linux/GetMonitors.cpp
      ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/src_cpp/linux/GetWindows.cpp
      ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/src_cpp/linux/ThreadRunner.cpp
    )
  else()
    message(FATAL_ERROR "media_capture::screen is not configured for this platform")
  endif()

  add_library(media_capture_screen STATIC ${MEDIA_CAPTURE_SCREEN_SOURCES})
  add_library(media_capture::screen ALIAS media_capture_screen)
  target_compile_features(media_capture_screen PUBLIC cxx_std_20)
  target_include_directories(media_capture_screen
    PUBLIC
      "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
      ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/include
  )
  if(WIN32)
    target_include_directories(media_capture_screen PRIVATE
      ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/include/windows
    )
    target_compile_definitions(media_capture_screen PRIVATE
      NOMINMAX
      _SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING
    )
    target_link_libraries(media_capture_screen PRIVATE d3d11 dxgi Dwmapi)
  else()
    target_include_directories(media_capture_screen PRIVATE
      ${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/include/linux
      ${X11_INCLUDE_DIR}
    )
    target_link_libraries(media_capture_screen PRIVATE
      ${X11_LIBRARIES}
      ${X11_Xfixes_LIB}
      ${X11_XTest_LIB}
      ${X11_Xinerama_LIB}
    )
  endif()
  set_target_properties(media_capture_screen PROPERTIES
    EXPORT_NAME screen
    POSITION_INDEPENDENT_CODE ON
  )
endif()

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
if(TARGET media_capture_screen)
  install(TARGETS media_capture_screen
    EXPORT media-capture-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES LICENSE THIRD_PARTY_NOTICES.md
  DESTINATION ${CMAKE_INSTALL_DATADIR}/media-capture
)
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
