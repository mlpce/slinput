# Support for different OS
if (ATARI_TOS_ENABLED)
  # Atari TOS using cross compiler
  set(ADAPT_SOURCES "adapt/tos.c")
else()
  # Otherwise, linux
  set(ADAPT_SOURCES "adapt/linux.c")
endif()

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" )
  # Using GCC
  set(CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -pedantic -Wconversion")

  if (ATARI_TOS_ENABLED)
    set(LIBCMINI_ENABLED OFF CACHE BOOL "Enable libcmini")
    if (LIBCMINI_ENABLED)
      # Using libcmini
      set(LIBCMINI_INCLUDE_PATH ${CMAKE_INSTALL_PREFIX}/libcmini/include CACHE STRING "Install path of libcmini headers")
      include_directories(BEFORE SYSTEM ${LIBCMINI_INCLUDE_PATH})
      if (ATARI_TOS_16BIT_INT)
        set(LIBCMINI_LIBRARY_PATH_SUBDIRECTORY "/mshort")
      endif()
      set(LIBCMINI_LIBRARY_PATH ${CMAKE_INSTALL_PREFIX}/libcmini/lib${LIBCMINI_LIBRARY_PATH_SUBDIRECTORY} CACHE STRING "Install path of libcmini libraries")
      set(LIBCMINI_STARTUP_PATH ${CMAKE_INSTALL_PREFIX}/libcmini/startup CACHE STRING "Install path of libcmini startup")
      set(LIBCMINI_LIBRARIES gcc cmini gcc)
      set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib ${LIBCMINI_STARTUP_PATH}/crt0.o -L${LIBCMINI_LIBRARY_PATH}")
    endif()
  endif()
endif()

# Static library
add_library(slinput STATIC
  ${ADAPT_SOURCES}
  slinput.c
)

# slinput.h is the public header
set(SLINPUT_PUBLIC_HEADERS "${CMAKE_SOURCE_DIR}/include/slinput.h")
set_target_properties(slinput PROPERTIES PUBLIC_HEADER "${SLINPUT_PUBLIC_HEADERS}")

# Install the slinput library and public header
install(TARGETS slinput ARCHIVE DESTINATION lib PUBLIC_HEADER DESTINATION include)

# Example program
add_subdirectory(example)

# If unit tests are enabled then build them.
# This is optional because not all platforms support googletest.
if (UNITTESTS_ENABLED)
  set(CMAKE_CXX_STANDARD 17) # unit tests use std::optional
  add_subdirectory(unittest)
endif()
