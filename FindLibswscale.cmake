###########################################################################
# Find libavutil.
FIND_PATH(FFMPEG_AVUTIL_INCLUDE_DIR
          NAMES libavutil/avutil.h
          PATHS /usr/local/include
                /usr/pkg/include/ffmpeg2
                /usr/include)
MARK_AS_ADVANCED(FFMPEG_AVUTIL_INCLUDE_DIR)
FIND_LIBRARY(FFMPEG_AVUTIL_LIBRARY
             NAMES avutil
             PATHS ${LIBAVUTILDIR}/lib/
                    /usr/lib/x86_64-linux-gnu/
                    /usr/local/lib64/
                    /usr/lib64/
                    /usr/lib/)
MARK_AS_ADVANCED(FFMPEG_AVUTIL_LIBRARY)

###########################################################################
# Find libswscale.
FIND_PATH(FFMPEG_SWSCALE_INCLUDE_DIR
          NAMES libswscale/swscale.h
          PATHS /usr/local/include
                /usr/pkg/include/ffmpeg2
                /usr/include)
MARK_AS_ADVANCED(FFMPEG_SWSCALE_INCLUDE_DIR)
FIND_LIBRARY(FFMPEG_SWSCALE_LIBRARY
             NAMES swscale
             PATHS ${LIBSWSCALEDIR}/lib/
                    /usr/lib/x86_64-linux-gnu/
                    /usr/local/lib64/
                    /usr/lib64/
                    /usr/lib/)
MARK_AS_ADVANCED(FFMPEG_SWSCALE_LIBRARY)

###########################################################################
IF (FFMPEG_SWSCALE_INCLUDE_DIR
    AND FFMPEG_AVUTIL_INCLUDE_DIR
    AND FFMPEG_SWSCALE_LIBRARY)
    SET(FFMPEGMODULES_FOUND 1)
    SET(FFMPEGMODULES_LIBRARIES ${FFMPEG_SWSCALE_LIBRARY} ${FFMPEG_AVUTIL_LIBRARY})
    SET(FFMPEGMODULES_INCLUDE_DIRS ${FFMPEG_SWSCALE_INCLUDE_DIR} ${FFMPEG_AVUTIL_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(FFMPEGMODULES_LIBRARIES)
MARK_AS_ADVANCED(FFMPEGMODULES_INCLUDE_DIRS)

IF (FFMPEGMODULES_FOUND)
    MESSAGE(STATUS "Found FFMEG modules: ${FFMPEGMODULES_INCLUDE_DIRS}, ${FFMPEGMODULES_LIBRARIES}")
ELSE ()
    MESSAGE(STATUS "Could not find FFMEG modules")
ENDIF()
