# the name of the target operating system
set(CMAKE_SYSTEM_NAME AtariTOS)
set(CMAKE_SYSTEM_VERSION 1)

# which compiler to use for C.
# vbcc webpage: http://www.compilers.de/vbcc.html
if(NOT DEFINED ENV{VBCC})
  message(FATAL_ERROR "VBCC environment variable must be set")
endif()
# Directory containing vc must also be present in PATH
set(CMAKE_C_COMPILER vc +tos)

# build flags
set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
set(CMAKE_C_FLAGS_MINSIZEREL "-O1 -size")
set(CMAKE_C_FLAGS_RELEASE "-O2")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g")

# where the target environment is located
set(CMAKE_FIND_ROOT_PATH  $ENV{VBCC}/targets/m68k-atari)

# adjust the default behavior of the FIND_XXX() commands:
# search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# search headers and libraries in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Building for atari tos using the VBCC compiler
set(ATARI_TOS_VBCC_ENABLED true)
