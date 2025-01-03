enable_testing()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
  # Using GCC
  set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wconversion")
endif()

# A single executable contains all the tests
add_executable(
  slinputt
  unittest.cpp
)

# The slinput library is tested
target_link_libraries(
  slinputt
  slinput
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(slinputt)

# Install the unit test
install(TARGETS slinputt RUNTIME DESTINATION bin COMPONENT runtime)
