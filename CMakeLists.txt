cmake_minimum_required(VERSION 3.25)

if (UNITTESTS_ENABLED)
  # slinput uses C. Unit tests use C++.
  project(slinput VERSION 1.0 LANGUAGES C CXX)
else()
  # slinput uses C.
  project(slinput VERSION 1.0 LANGUAGES C)
endif()

# Use C90 for compatibility with e.g. "Pure C"
set(CMAKE_C_STANDARD 90)

# slinput headers and source code directories
include_directories(".")
add_subdirectory(src)

# Unit tests are disabled by default because not all platforms support them
set(UNITTESTS_ENABLED OFF CACHE BOOL "Build unit tests")
if (UNITTESTS_ENABLED)
  # Googletest is used for unit tests
  set(CMAKE_CXX_STANDARD 14)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)

  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    URL_HASH SHA1=0ac421f2ec11af38b0fff0f1992184032731a8bc
  )
  FetchContent_MakeAvailable(googletest)
endif()
