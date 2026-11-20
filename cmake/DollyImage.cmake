# Reuse the host libraries in the full build. The standalone build compiles
# only the shared image format and file backend into the same executable.
add_executable(dolly-image
  "${PROJECT_SOURCE_DIR}/src/dolly_image_tool.c"
  "${PROJECT_SOURCE_DIR}/src/dolly_dly_writer.c")
target_include_directories(dolly-image PRIVATE "${PROJECT_SOURCE_DIR}/include")
if(TARGET dollydlyreader)
  target_link_libraries(dolly-image PRIVATE dollydlyreader dollychecksum)
else()
  target_sources(dolly-image PRIVATE
    "${PROJECT_SOURCE_DIR}/src/dolly_block_source.c"
    "${PROJECT_SOURCE_DIR}/src/dolly_file_source.c"
    "${PROJECT_SOURCE_DIR}/src/dolly_checksum.c"
    "${PROJECT_SOURCE_DIR}/src/dolly_dly_codec.c"
    "${PROJECT_SOURCE_DIR}/src/dolly_dly_reader.c")
endif()
if(WIN32)
  target_link_libraries(dolly-image PRIVATE bcrypt)
endif()
if(MSVC)
  target_compile_definitions(dolly-image PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dolly-image PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

install(TARGETS dolly-image RUNTIME DESTINATION bin)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME image_tool_round_trip
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/test_image_tool.py"
      $<TARGET_FILE:dolly-image>)
endif()
