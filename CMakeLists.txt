cmake_minimum_required(VERSION 3.13)
project(dolly C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_library(dollyprobe STATIC src/dolly_probe.c)
target_include_directories(dollyprobe PUBLIC ${PROJECT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyprobe PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(dolly-inspect src/dolly_inspect.c)
target_link_libraries(dolly-inspect PRIVATE dollyprobe)
enable_testing()
add_executable(test-probe tests/test_probe.c)
target_link_libraries(test-probe PRIVATE dollyprobe)
add_test(NAME file_probe COMMAND test-probe)

# POSIX host backend; AmigaOS device backend will be separate.
add_library(dollyblock STATIC src/dolly_block_source.c src/dolly_file_source.c)
target_include_directories(dollyblock PUBLIC ${PROJECT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyblock PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-block-source tests/test_block_source.c)
target_link_libraries(test-block-source PRIVATE dollyblock)
add_test(NAME block_source COMMAND test-block-source)

# Read-only RDB parser and host inspector.
add_library(dollyrdb STATIC src/dolly_rdb.c)
target_include_directories(dollyrdb PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(dollyrdb PUBLIC dollyblock)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyrdb PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-rdb tests/test_rdb.c)
target_link_libraries(test-rdb PRIVATE dollyrdb)
add_test(NAME rdb_read_only COMMAND test-rdb)
add_executable(dolly-rdb src/dolly_rdb_inspect.c)
target_link_libraries(dolly-rdb PRIVATE dollyrdb)

# Safe, read-only MBR primary partition scan and nested 0x76 RDB discovery.
add_library(dollymap STATIC src/dolly_partition_map.c)
target_include_directories(dollymap PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(dollymap PUBLIC dollyrdb)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollymap PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-partition-map tests/test_partition_map.c)
target_link_libraries(test-partition-map PRIVATE dollymap)
add_test(NAME partition_map_read_only COMMAND test-partition-map)

# Plan device reads independently from actual driver backends.
add_library(dollyioplan STATIC src/dolly_io_plan.c)
target_include_directories(dollyioplan PUBLIC ${PROJECT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyioplan PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-io-plan tests/test_io_plan.c)
target_link_libraries(test-io-plan PRIVATE dollyioplan)
add_test(NAME io_64bit_planning COMMAND test-io-plan)

# Fake Exec API catches adapter mistakes on host. This is NOT a 68000 build.
add_executable(test-amiga-mock tests/test_amiga_device_mock.c
  tests/amiga_mock/amiga_mock_exec.c amiga/dolly_amiga_source.c)
target_compile_definitions(test-amiga-mock PRIVATE DOLLY_AMIGA_MOCK)
target_include_directories(test-amiga-mock PRIVATE
  ${PROJECT_SOURCE_DIR}/tests/amiga_mock ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test-amiga-mock PRIVATE dollyblock dollyioplan)
add_test(NAME amiga_exec_mock COMMAND test-amiga-mock)

# Strict streaming IFF FORM DOLY envelope inspection.
add_library(dollyiff STATIC src/dolly_iff.c)
target_include_directories(dollyiff PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(dollyiff PUBLIC dollyblock)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyiff PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-iff tests/test_iff.c)
target_link_libraries(test-iff PRIVATE dollyiff)
add_test(NAME iff_form_doly COMMAND test-iff)
add_executable(dolly-iff src/dolly_iff_inspect.c)
target_link_libraries(dolly-iff PRIVATE dollyiff)

# Read-only host CLI for both MBR and nested RDB.
add_executable(dolly-layout src/dolly_layout.c)
target_link_libraries(dolly-layout PRIVATE dollymap)

# Optional Python integration test constructs synthetic files on demand.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME cli_synthetic_smoke
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/test_cli_smoke.py
      $<TARGET_FILE:dolly-layout> $<TARGET_FILE:dolly-iff>
      $<TARGET_FILE:dolly-inventory>)
endif()

# Read-only analysis of filesystem de_MaxTransfer (not raw device transfer limit).
add_library(dollymaxtransfer STATIC src/dolly_maxtransfer.c)
target_include_directories(dollymaxtransfer PUBLIC ${PROJECT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollymaxtransfer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-maxtransfer tests/test_maxtransfer.c)
target_link_libraries(test-maxtransfer PRIVATE dollymaxtransfer)
add_test(NAME maxtransfer_advisory COMMAND test-maxtransfer)
target_link_libraries(dolly-rdb PRIVATE dollymaxtransfer)

# Full layout CLI uses the same MaxTransfer advisory as dolly-rdb.
target_link_libraries(dolly-layout PRIVATE dollymaxtransfer)

# Additional non-destructive audit: cross-check RDB partition and metadata spans.
add_library(dollyrdbaudit STATIC src/dolly_rdb_audit.c)
target_include_directories(dollyrdbaudit PUBLIC ${PROJECT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyrdbaudit PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-rdb-audit tests/test_rdb_audit.c)
target_link_libraries(test-rdb-audit PRIVATE dollyrdbaudit)
add_test(NAME rdb_collision_audit COMMAND test-rdb-audit)
target_link_libraries(dolly-layout PRIVATE dollyrdbaudit)
target_link_libraries(dolly-rdb PRIVATE dollyrdbaudit)

# Deterministic malformed-input stability test (synthetic images only).
add_executable(test-rdb-mutations tests/test_rdb_mutations.c)
target_link_libraries(test-rdb-mutations PRIVATE dollymap)
add_test(NAME rdb_mutation_stability COMMAND test-rdb-mutations)

# Several M0 test cases use assert() both for setup and validation.
# Force assertions on in all GNU/Clang test executables, even Release builds.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  foreach(dolly_test IN ITEMS test-probe test-block-source test-rdb test-partition-map test-io-plan test-amiga-mock test-iff test-maxtransfer test-rdb-audit test-rdb-mutations)
    target_compile_options(${dolly_test} PRIVATE -UNDEBUG)
  endforeach()
endif()

# Shared field-streaming layer for future Sheep Inventory GUI and host CLI.
add_library(dollyinventory STATIC src/dolly_inventory.c)
target_include_directories(dollyinventory PUBLIC ${PROJECT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyinventory PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-inventory tests/test_inventory.c)
target_link_libraries(test-inventory PRIVATE dollyinventory)
add_test(NAME inventory_streaming COMMAND test-inventory)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test-inventory PRIVATE -UNDEBUG)
endif()

# Host-only Sheep Inventory CLI prototype. Later GadTools UI consumes same field stream.
add_executable(dolly-inventory src/dolly_inventory_inspect.c)
target_link_libraries(dolly-inventory PRIVATE dollyinventory dollymap dollymaxtransfer)

# Unified non-destructive notices used by future Sheep Inventory UI.
add_library(dollyinventorynotices STATIC src/dolly_inventory_notices.c)
target_include_directories(dollyinventorynotices PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(dollyinventorynotices PUBLIC dollymaxtransfer)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyinventorynotices PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-inventory-notices tests/test_inventory_notices.c)
target_link_libraries(test-inventory-notices PRIVATE dollyinventorynotices)
add_test(NAME inventory_advisories COMMAND test-inventory-notices)
target_link_libraries(dolly-inventory PRIVATE dollyinventorynotices)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test-inventory-notices PRIVATE -UNDEBUG)
endif()
