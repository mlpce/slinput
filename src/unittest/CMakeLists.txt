enable_testing()

# A single executable contains all the tests
add_executable(
  slinput_unittest
  slinput_unittest.cpp
)

# The slinput library is tested
target_link_libraries(
  slinput_unittest
  slinput
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(slinput_unittest)

# Install the unit test
install(TARGETS slinput_unittest RUNTIME DESTINATION bin COMPONENT runtime)
