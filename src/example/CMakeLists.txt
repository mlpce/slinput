if (ATARI_TOS_VBCC_ENABLED)
# 8.3 TOS filenames 
set(EXAMPLE_EXECUTABLE_NAME "slinputx.tos")
else()
set(EXAMPLE_EXECUTABLE_NAME "slinputx")
endif()

add_executable(
  ${EXAMPLE_EXECUTABLE_NAME}
  main.c
)

target_link_libraries(${EXAMPLE_EXECUTABLE_NAME} slinput)
