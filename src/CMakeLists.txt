include(CheckTypeSize)

# Support for different OS
if (ATARI_TOS_VBCC_ENABLED)
  # Atari TOS using vbcc compiler
  set(ADAPT_SOURCES "adapt/tos.c")
else()
  # Otherwise, linux
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -pedantic -Wconversion")
  set(ADAPT_SOURCES "adapt/linux.c")
endif()

# Check how big wchar_t is.
check_type_size("wchar_t" SIZEOF_WCHAR_T LANGUAGE C)

# Write out the config header.
file(WRITE ${CMAKE_BINARY_DIR}/slinputc.h
"#ifndef SLINPUT_CONFIG
#define SLINPUT_CONFIG
#define SLI_CHAR_SIZE ${SIZEOF_WCHAR_T}
typedef wchar_t sli_char;
typedef signed short sli_sshort;
typedef unsigned short sli_ushort;
#endif
"
)

# slinput_config.h is written to CMAKE_BINARY_DIR. Add the directory so the
# header file can be included.
include_directories("${CMAKE_BINARY_DIR}")

# Static library
add_library(slinput STATIC
  ${ADAPT_SOURCES}
  slinput.c
)

# Install the headers
# slinput.h header is installed along with slinputc.h
set(SLINPUT_PUBLIC_HEADERS "${CMAKE_SOURCE_DIR}/include/slinput.h" "${CMAKE_BINARY_DIR}/slinputc.h")
set_target_properties(slinput PROPERTIES PUBLIC_HEADER "${SLINPUT_PUBLIC_HEADERS}")

# Install the slinput library
install(TARGETS slinput ARCHIVE DESTINATION lib PUBLIC_HEADER DESTINATION include)

# Example program
add_subdirectory(example)

# If unit tests are enabled then build them.
# This is optional because not all platforms support googletest.
if (UNITTESTS_ENABLED)
  set(CMAKE_CXX_STANDARD 17) # unit tests use std::optional
  add_subdirectory(unittest)
endif()
