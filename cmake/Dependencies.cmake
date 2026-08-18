include(FetchContent)

set(MEDIA_CAPTURE_MINIAUDIO_ROOT "" CACHE PATH "Path to a local miniaudio source tree")
set(MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_ROOT "" CACHE PATH
  "Path to a local screen_capture_lite source tree"
)
set(MEDIA_CAPTURE_CAMERA_CAPTURE_ROOT "" CACHE PATH
  "Path to a local CameraCapture source tree"
)

if(MEDIA_CAPTURE_MINIAUDIO_ROOT)
  get_filename_component(MEDIA_CAPTURE_MINIAUDIO_SOURCE_DIR
    "${MEDIA_CAPTURE_MINIAUDIO_ROOT}" ABSOLUTE
  )
  if(NOT EXISTS "${MEDIA_CAPTURE_MINIAUDIO_SOURCE_DIR}/miniaudio.h" OR
     NOT EXISTS "${MEDIA_CAPTURE_MINIAUDIO_SOURCE_DIR}/miniaudio.c")
    message(FATAL_ERROR
      "MEDIA_CAPTURE_MINIAUDIO_ROOT must contain miniaudio.h and miniaudio.c"
    )
  endif()
else()
  FetchContent_Declare(miniaudio_source
    URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.25.tar.gz
    URL_HASH SHA256=b900edcffe979816e2560a0580b9b1216d674b4f17fbadeca8f777a7f8ab0274
    SOURCE_SUBDIR __media_capture_dependency_only
  )
  FetchContent_MakeAvailable(miniaudio_source)
  set(MEDIA_CAPTURE_MINIAUDIO_SOURCE_DIR "${miniaudio_source_SOURCE_DIR}")
endif()

if(MEDIA_CAPTURE_BUILD_CAMERA)
  if(MEDIA_CAPTURE_CAMERA_CAPTURE_ROOT)
    get_filename_component(MEDIA_CAPTURE_CAMERA_CAPTURE_SOURCE_DIR
      "${MEDIA_CAPTURE_CAMERA_CAPTURE_ROOT}" ABSOLUTE
    )
    if(NOT EXISTS "${MEDIA_CAPTURE_CAMERA_CAPTURE_SOURCE_DIR}/include/ccap.h" OR
       NOT EXISTS "${MEDIA_CAPTURE_CAMERA_CAPTURE_SOURCE_DIR}/CMakeLists.txt")
      message(FATAL_ERROR
        "MEDIA_CAPTURE_CAMERA_CAPTURE_ROOT must contain CameraCapture sources"
      )
    endif()
  else()
    FetchContent_Declare(camera_capture_source
      GIT_REPOSITORY https://github.com/wysaid/CameraCapture.git
      GIT_TAG v1.7.4
      GIT_SHALLOW TRUE
      SOURCE_SUBDIR __media_capture_dependency_only
    )
    FetchContent_MakeAvailable(camera_capture_source)
    set(MEDIA_CAPTURE_CAMERA_CAPTURE_SOURCE_DIR
      "${camera_capture_source_SOURCE_DIR}"
    )
  endif()
endif()

if(MEDIA_CAPTURE_BUILD_SCREEN)
  if(MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_ROOT)
    get_filename_component(MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR
      "${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_ROOT}" ABSOLUTE
    )
    if(NOT EXISTS "${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/include/ScreenCapture.h" OR
       NOT EXISTS "${MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR}/src_cpp/SCCommon.cpp")
      message(FATAL_ERROR
        "MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_ROOT must contain screen_capture_lite sources"
      )
    endif()
  else()
    FetchContent_Declare(screen_capture_lite_source
      URL https://github.com/smasherprog/screen_capture_lite/archive/refs/tags/17.1.2745.tar.gz
      URL_HASH SHA256=5dc07bc0e04c4a4121830591d44c2ff69f5bef17f7a96be31f498b3730780ea7
      SOURCE_SUBDIR __media_capture_dependency_only
    )
    FetchContent_MakeAvailable(screen_capture_lite_source)
    set(MEDIA_CAPTURE_SCREEN_CAPTURE_LITE_SOURCE_DIR
      "${screen_capture_lite_source_SOURCE_DIR}"
    )
  endif()
endif()
