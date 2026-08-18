include(FetchContent)

set(MEDIA_CAPTURE_MINIAUDIO_ROOT "" CACHE PATH "Path to a local miniaudio source tree")

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
