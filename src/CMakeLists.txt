# Support for different OS
if (ATARI_TOS_VBCC_ENABLED)
  # Atari TOS using vbcc compiler
  set(ADAPT_SOURCES "adapt/tos.c")
else()
  # Otherwise, linux
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -pedantic -Wconversion")
  set(ADAPT_SOURCES "adapt/linux.c")
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
