add_executable(media_capture_audio_api_test audio_capture_api_test.cpp)
target_link_libraries(media_capture_audio_api_test PRIVATE media_capture::audio)
add_test(NAME media_capture.audio_api COMMAND media_capture_audio_api_test)

if(TARGET media_capture_camera)
  add_executable(media_capture_camera_api_test camera_capture_api_test.cpp)
  target_link_libraries(media_capture_camera_api_test PRIVATE media_capture::camera)
  add_test(NAME media_capture.camera_api COMMAND media_capture_camera_api_test)
endif()

if(TARGET media_capture_screen)
  add_executable(media_capture_screen_source_test
    cursor_compositor_test.cpp
    screen_source_test.cpp
  )
  target_include_directories(media_capture_screen_source_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src/screen
  )
  target_link_libraries(media_capture_screen_source_test PRIVATE media_capture::screen)
  add_test(NAME media_capture.screen_source COMMAND media_capture_screen_source_test)
endif()
