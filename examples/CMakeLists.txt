add_executable(media_capture_audio_devices audio_devices.cpp)
target_link_libraries(media_capture_audio_devices PRIVATE media_capture::audio)

add_executable(media_capture_record_audio record_audio.cpp)
target_link_libraries(media_capture_record_audio PRIVATE media_capture::audio)

if(TARGET media_capture_camera)
  add_executable(media_capture_camera_devices camera_devices.cpp)
  target_link_libraries(media_capture_camera_devices PRIVATE media_capture::camera)

  add_executable(media_capture_capture_camera capture_camera.cpp)
  target_link_libraries(media_capture_capture_camera PRIVATE media_capture::camera)
endif()

if(TARGET media_capture_screen)
  add_executable(media_capture_screen_sources screen_sources.cpp)
  target_link_libraries(media_capture_screen_sources PRIVATE media_capture::screen)

  add_executable(media_capture_capture_screen capture_screen.cpp)
  target_link_libraries(media_capture_capture_screen PRIVATE media_capture::screen)
endif()
