enable_testing()

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
