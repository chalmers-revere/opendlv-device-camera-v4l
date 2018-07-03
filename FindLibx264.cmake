###########################################################################
# Find libx264.
FIND_PATH(X264_INCLUDE_DIR
          NAMES x264.h
          PATHS /usr/local/include
                /usr/include)
MARK_AS_ADVANCED(X264_INCLUDE_DIR)
FIND_LIBRARY(X264_LIBRARY
             NAMES x264
             PATHS ${LIBX264DIR}/lib/
                    /usr/lib/x86_64-linux-gnu/
                    /usr/local/lib64/
                    /usr/lib64/
                    /usr/lib/)
MARK_AS_ADVANCED(X264_LIBRARY)

###########################################################################
IF (X264_INCLUDE_DIR
    AND X264_LIBRARY)
    SET(X264_FOUND 1)
    SET(X264_LIBRARIES ${X264_LIBRARY})
    SET(X264_INCLUDE_DIRS ${X264_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(X264_LIBRARIES)
MARK_AS_ADVANCED(X264_INCLUDE_DIRS)

IF (X264_FOUND)
    MESSAGE(STATUS "Found x264: ${X264_INCLUDE_DIRS}, ${X264_LIBRARIES}")
ELSE ()
    MESSAGE(STATUS "Could not find x264 modules")
ENDIF()
