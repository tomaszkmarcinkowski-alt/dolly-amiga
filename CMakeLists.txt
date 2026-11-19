cmake_minimum_required(VERSION 3.13)
project(dolly C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Native 68000 smoke-only mode: NO POSIX backend, NO disk access.
# Cross-build with the separate AmigaOS toolchain file and an installed SDK.
option(DOLLY_NATIVE_ONLY "Build only native AmigaOS smoke executable" OFF)
option(DOLLY_NATIVE_M68000 "Pass -m68000 for classic Motorola 68000" OFF)
# Optional NDK compile target for the AmigaDOS preferences notification adapter.
# This does not yet create a working GadTools event loop or plugin loader.
option(DOLLY_NATIVE_GUI_WATCH "Compile AmigaDOS screen/font notify adapter (NDK required)" OFF)
# Portable operation state is independent of every GUI and disk adapter.
# Both host and Amiga cross-builds compile the same C99 implementation.
enable_testing()
add_library(dollyoperation STATIC src/dolly_operation_session.c)
target_include_directories(dollyoperation PUBLIC ${PROJECT_SOURCE_DIR}/include)
add_executable(test-operation-session tests/test_operation_session.c)
target_link_libraries(test-operation-session PRIVATE dollyoperation)
add_test(NAME operation_session_hide_does_not_cancel COMMAND test-operation-session)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyoperation PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(test-operation-session PRIVATE -UNDEBUG)
  if(DOLLY_NATIVE_M68000)
    target_compile_options(dollyoperation PRIVATE -m68000)
    target_compile_options(test-operation-session PRIVATE -m68000)
    target_link_options(test-operation-session PRIVATE -m68000)
  endif()
endif()

# Bounded write/flush/readback policy, independent of the target transport.
add_library(dollyblocktarget STATIC src/dolly_block_target.c)
target_include_directories(dollyblocktarget PUBLIC ${PROJECT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyblocktarget PRIVATE -Wall -Wextra -Wpedantic -Werror)
  if(DOLLY_NATIVE_M68000)
    target_compile_options(dollyblocktarget PRIVATE -m68000)
  endif()
endif()

add_library(dollyrestoresource STATIC src/dolly_restore_source.c src/dolly_restore_transfer.c)
target_include_directories(dollyrestoresource PUBLIC ${PROJECT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyrestoresource PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
if(DOLLY_NATIVE_M68000)
  target_compile_options(dollyrestoresource PRIVATE -m68000)
endif()

# Shared content-based minimum size preflight (pure C99; host + m68k).
add_library(dollyguimin STATIC src/dolly_gui_minimum.c)
target_include_directories(dollyguimin PUBLIC ${PROJECT_SOURCE_DIR}/include)
add_executable(test-gui-minimum tests/test_gui_minimum.c)
target_link_libraries(test-gui-minimum PRIVATE dollyguimin)
add_test(NAME gui_content_minimum COMMAND test-gui-minimum)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyguimin PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(test-gui-minimum PRIVATE -UNDEBUG)
  if(DOLLY_NATIVE_M68000)
    target_compile_options(dollyguimin PRIVATE -m68000)
    target_compile_options(test-gui-minimum PRIVATE -m68000)
    target_link_options(test-gui-minimum PRIVATE -m68000)
  endif()
endif()

# Endpoint roles, whole-unit/partition/file selections and safe target gating.
add_library(dollyendpoint STATIC src/dolly_endpoint.c)
target_include_directories(dollyendpoint PUBLIC ${PROJECT_SOURCE_DIR}/include)
add_executable(test-endpoint tests/test_endpoint.c)
target_link_libraries(test-endpoint PRIVATE dollyendpoint)
add_test(NAME endpoint_selection_safety COMMAND test-endpoint)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyendpoint PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(test-endpoint PRIVATE -UNDEBUG)
  if(DOLLY_NATIVE_M68000)
    target_compile_options(dollyendpoint PRIVATE -m68000)
    target_compile_options(test-endpoint PRIVATE -m68000)
    target_link_options(test-endpoint PRIVATE -m68000)
  endif()
endif()

if(DOLLY_NATIVE_ONLY)
  enable_testing()
  add_executable(dolly-native-smoke
    amiga/dolly_native_smoke.c
    src/dolly_block_source.c src/dolly_rdb.c src/dolly_rdb_audit.c
    src/dolly_inventory.c src/dolly_io_plan.c src/dolly_maxtransfer.c
    src/dolly_dma_policy.c src/dolly_dma_advice.c src/dolly_paths.c
    src/dolly_selfcheck_embedded.c src/dolly_selfcheck.c
    src/dolly_checksum.c src/dolly_version.c)
  target_include_directories(dolly-native-smoke PRIVATE
    ${PROJECT_SOURCE_DIR}/include)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dolly-native-smoke PRIVATE
      -Wall -Wextra -Wpedantic -Werror)
    if(DOLLY_NATIVE_M68000)
      target_compile_options(dolly-native-smoke PRIVATE -m68000)
      target_link_options(dolly-native-smoke PRIVATE -m68000)
      target_compile_definitions(dolly-native-smoke PRIVATE DOLLY_AMIGAOS_MEMORY_PROFILING)
    endif()
  endif()
  # Real Intuition screen smoke, AmigaOS 2.x+ only. No device I/O.
  if(DOLLY_NATIVE_M68000)
    add_library(dollyamigawrite STATIC amiga/dolly_amiga_write.c src/dolly_dma_policy.c)
    target_include_directories(dollyamigawrite PUBLIC ${PROJECT_SOURCE_DIR}/amiga)
    target_compile_options(dollyamigawrite PRIVATE -m68000 -Wall -Wextra -Wpedantic -Werror)
    target_link_libraries(dollyamigawrite PUBLIC dollyblocktarget)
    add_library(dollyamigarestore STATIC amiga/dolly_amiga_restore_source.c
      src/dolly_block_source.c)
    target_include_directories(dollyamigarestore PUBLIC ${PROJECT_SOURCE_DIR}/amiga)
    target_compile_options(dollyamigarestore PRIVATE -m68000 -Wall -Wextra -Wpedantic -Werror)
    target_link_libraries(dollyamigarestore PUBLIC dollyrestoresource)
    add_executable(dolly-screen-smoke amiga/dolly_screen_smoke.c)
    target_compile_options(dolly-screen-smoke PRIVATE
      -m68000 -Wall -Wextra -Wpedantic -Werror)
    target_link_options(dolly-screen-smoke PRIVATE -m68000)
    target_link_libraries(dolly-screen-smoke PRIVATE amiga)
    add_executable(dolly-sheep-demo amiga/dolly_sheep_demo.c)
    target_compile_options(dolly-sheep-demo PRIVATE
      -m68000 -Wall -Wextra -Wpedantic -Werror)
    target_link_options(dolly-sheep-demo PRIVATE -m68000)
    target_link_libraries(dolly-sheep-demo PRIVATE amiga)
    # Default Sheep Counter UI: an Intuition window on the public screen.
    add_executable(dolly-sheep-window amiga/dolly_sheep_window.c)
    target_compile_options(dolly-sheep-window PRIVATE
      -m68000 -Wall -Wextra -Wpedantic -Werror)
    target_link_options(dolly-sheep-window PRIVATE -m68000)
    target_link_libraries(dolly-sheep-window PRIVATE amiga)
    # Two concurrent views share a single C99 OperationSession.
    add_executable(dolly-two-window-demo amiga/dolly_two_window_demo.c
      amiga/dolly_amiga_endpoint_picker.c
      amiga/dolly_amiga_dos_devices.c
      amiga/dolly_amiga_physical_catalog.c
      amiga/dolly_amiga_device_query.c
      amiga/dolly_amiga_partition_scan.c
      amiga/dolly_amiga_inventory_view.c
      amiga/dolly_amiga_surface_scan.c amiga/dolly_amiga_source.c
      amiga/dolly_amiga_scan_options.c src/dolly_scan_range.c
      amiga/dolly_surface_log.c amiga/dolly_amiga_log_view.c
      amiga/dolly_amiga_clipboard.c amiga/dolly_amiga_image.c
      amiga/dolly_amiga_io_settings.c amiga/dolly_amiga_prefs.c
      amiga/dolly_amiga_transfer_guard.c amiga/dolly_amiga_restore_confirm.c
      amiga/dolly_amiga_destructive.c amiga/dolly_operation_pointer.c src/dolly_dlrp.c
      amiga/dolly_amiga_restore_source.c
      src/dolly_restore_check.c src/dolly_restore_source.c src/dolly_rdb.c
      src/dolly_block_source.c src/dolly_io_plan.c src/dolly_dma_policy.c
      src/dolly_dly_writer.c src/dolly_dly_codec.c src/dolly_checksum.c)
    target_compile_options(dolly-two-window-demo PRIVATE
      -m68000 -Wall -Wextra -Wpedantic -Werror)
    target_link_options(dolly-two-window-demo PRIVATE -m68000)
    target_link_libraries(dolly-two-window-demo PRIVATE dollyoperation dollyguimin dollyendpoint dollyamigawrite amiga)
    # M1.23 independent diagnostic: enumerate without device I/O by
    # default; optional "--probe" sends only read-only queries.
    add_executable(dolly-physical-probe amiga/dolly_amiga_physical_probe.c)
    target_compile_options(dolly-physical-probe PRIVATE
      -m68000 -Wall -Wextra -Wpedantic -Werror)
    target_link_options(dolly-physical-probe PRIVATE -m68000)
    target_link_libraries(dolly-physical-probe PRIVATE amiga)
  endif()
  # Portable diagnostic helper: can also be cross-built for AmigaOS.
  add_executable(dolly-dma-advice
    src/dolly_dma_advice_cli.c src/dolly_dma_advice.c src/dolly_dma_policy.c)
  target_include_directories(dolly-dma-advice PRIVATE ${PROJECT_SOURCE_DIR}/include)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dolly-dma-advice PRIVATE -Wall -Wextra -Wpedantic -Werror)
    if(DOLLY_NATIVE_M68000)
      target_compile_options(dolly-dma-advice PRIVATE -m68000)
      target_link_options(dolly-dma-advice PRIVATE -m68000)
    endif()
  endif()
  if(DOLLY_NATIVE_GUI_WATCH)
    add_library(dolly-native-gui-watch STATIC
      amiga/dolly_amiga_pref_notify.c src/dolly_screen_watch.c)
    target_include_directories(dolly-native-gui-watch PRIVATE
      ${PROJECT_SOURCE_DIR}/include)
    if(DOLLY_NATIVE_M68000 AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options(dolly-native-gui-watch PRIVATE -m68000)
    endif()
  endif()
  # Device-independent assign path checks are available in native builds.
  add_library(dollypaths STATIC src/dolly_paths.c)
  target_include_directories(dollypaths PUBLIC ${PROJECT_SOURCE_DIR}/include)
  add_executable(test-paths tests/test_paths.c)
  target_link_libraries(test-paths PRIVATE dollypaths)
  add_test(NAME dolly_assign_resource_paths COMMAND test-paths)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dollypaths PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(test-paths PRIVATE -UNDEBUG)
  endif()
  # Portable no-I/O capability and profile checks, suitable for 68000 smoke.
  add_library(dollyresources STATIC src/dolly_resources.c)
  target_include_directories(dollyresources PUBLIC ${PROJECT_SOURCE_DIR}/include)
  target_link_libraries(dollyresources PUBLIC dollypaths)
  add_library(dollydeviceprofiles STATIC src/dolly_device_profiles.c)
  target_include_directories(dollydeviceprofiles PUBLIC ${PROJECT_SOURCE_DIR}/include)
  add_executable(test-resources tests/test_resources.c)
  target_link_libraries(test-resources PRIVATE dollyresources)
  add_test(NAME optional_resources_graceful_degradation COMMAND test-resources)
  add_executable(test-device-profiles tests/test_device_profiles.c)
  target_link_libraries(test-device-profiles PRIVATE dollydeviceprofiles)
  add_test(NAME device_profile_suggestions_only COMMAND test-device-profiles)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dollyresources PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(dollydeviceprofiles PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(test-resources PRIVATE -UNDEBUG)
    target_compile_options(test-device-profiles PRIVATE -UNDEBUG)
  endif()
  add_library(dollydmadiagnostics STATIC src/dolly_dma_diagnostics.c)
  target_include_directories(dollydmadiagnostics PUBLIC ${PROJECT_SOURCE_DIR}/include)
  target_link_libraries(dollydmadiagnostics PUBLIC dollydeviceprofiles)
  # The native-only build keeps the advice helper optional; no device is opened.
  add_library(dollychecksum STATIC src/dolly_checksum.c)
  target_include_directories(dollychecksum PUBLIC ${PROJECT_SOURCE_DIR}/include)
  add_executable(test-checksum tests/test_checksum.c)
  target_link_libraries(test-checksum PRIVATE dollychecksum)
  add_test(NAME crc32_integrity_stream COMMAND test-checksum)
  add_library(dollyprofileconfig STATIC src/dolly_profile_config.c)
  target_include_directories(dollyprofileconfig PUBLIC ${PROJECT_SOURCE_DIR}/include)
  target_link_libraries(dollyprofileconfig PUBLIC dollydeviceprofiles)
  add_executable(test-profile-config tests/test_profile_config.c)
  target_link_libraries(test-profile-config PRIVATE dollyprofileconfig)
  add_test(NAME device_local_config_proposals COMMAND test-profile-config)
  add_library(dollyselfcheck STATIC
    src/dolly_selfcheck.c src/dolly_selfcheck_embedded.c
    src/dolly_checksum.c src/dolly_version.c)
  target_include_directories(dollyselfcheck PUBLIC ${PROJECT_SOURCE_DIR}/include)
  add_executable(test-selfcheck tests/test_selfcheck_embedded.c)
  target_link_libraries(test-selfcheck PRIVATE dollyselfcheck)
  add_test(NAME self_executable_crc32_check COMMAND test-selfcheck)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dollyselfcheck PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(test-selfcheck PRIVATE -UNDEBUG)
  endif()
  # Read-only RDB vs independently observed media/container capacity.
  add_library(dollycapacity STATIC src/dolly_capacity_audit.c)
  target_include_directories(dollycapacity PUBLIC ${PROJECT_SOURCE_DIR}/include)
  add_executable(test-capacity-audit tests/test_capacity_audit.c)
  target_link_libraries(test-capacity-audit PRIVATE dollycapacity)
  add_test(NAME rdb_media_capacity_audit COMMAND test-capacity-audit)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dollycapacity PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(test-capacity-audit PRIVATE -UNDEBUG)
  endif()
  # This also allows a host smoke build; host success is not Amiga validation.
  enable_testing()
  add_test(NAME native_memory_smoke COMMAND dolly-native-smoke --selftest)
  return()
endif()
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
add_executable(test-block-target tests/test_block_target.c)
target_link_libraries(test-block-target PRIVATE dollyblocktarget)
add_test(NAME verified_bounded_write COMMAND test-block-target)

add_executable(test-restore-source tests/test_restore_source.c src/dolly_restore_check.c)
target_link_libraries(test-restore-source PRIVATE dollyrestoresource dollyrdb)
add_test(NAME restore_source_geometry COMMAND test-restore-source)

add_executable(test-dlrp tests/test_dlrp.c src/dolly_dlrp.c)
target_include_directories(test-dlrp PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME dlrp_iff_serialization COMMAND test-dlrp)

# Fake Exec API catches adapter mistakes on host. This is NOT a 68000 build.
add_executable(test-amiga-mock tests/test_amiga_device_mock.c
  tests/amiga_mock/amiga_mock_exec.c amiga/dolly_amiga_source.c)
target_compile_definitions(test-amiga-mock PRIVATE DOLLY_AMIGA_MOCK)
target_include_directories(test-amiga-mock PRIVATE
  ${PROJECT_SOURCE_DIR}/tests/amiga_mock ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test-amiga-mock PRIVATE dollyblock dollyioplan dollydma)
add_test(NAME amiga_exec_mock COMMAND test-amiga-mock)
add_executable(test-amiga-write tests/test_amiga_write_mock.c amiga/dolly_amiga_write.c)
target_compile_definitions(test-amiga-write PRIVATE DOLLY_AMIGA_MOCK)
target_include_directories(test-amiga-write PRIVATE
  ${PROJECT_SOURCE_DIR}/tests/amiga_mock ${PROJECT_SOURCE_DIR}/amiga)
target_link_libraries(test-amiga-write PRIVATE dollyblocktarget dollydma)
add_test(NAME amiga_write_exec_mock COMMAND test-amiga-write)


add_executable(test-surface-log tests/test_surface_log.c amiga/dolly_surface_log.c src/dolly_scan_range.c)
target_include_directories(test-surface-log PRIVATE ${PROJECT_SOURCE_DIR}/amiga)
target_link_libraries(test-surface-log PRIVATE dollyblock)
add_test(NAME surface_log COMMAND test-surface-log)

# Strict streaming IFF FORM DOLY envelope inspection.
add_library(dollyiff STATIC src/dolly_iff.c)
target_include_directories(dollyiff PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(dollyiff PUBLIC dollyblock)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyiff PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-iff tests/test_iff.c src/dolly_dly_writer.c src/dolly_dly_codec.c)
target_link_libraries(test-iff PRIVATE dollyiff dollychecksum)
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
  foreach(dolly_test IN ITEMS test-probe test-block-source test-rdb test-partition-map test-io-plan test-block-target test-restore-source test-dlrp test-amiga-mock test-amiga-write test-iff test-maxtransfer test-rdb-audit test-rdb-mutations)
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
target_link_libraries(dollyinventorynotices PUBLIC dollymaxtransfer dollyrdbaudit)
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

# Pure C99 DMA safety policy: necessary mask/range/alignment checks.
add_library(dollydma STATIC src/dolly_dma_policy.c src/dolly_dma_advice.c)
target_include_directories(dollydma PUBLIC ${PROJECT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollydma PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-dma-policy tests/test_dma_policy.c)
target_link_libraries(test-dma-policy PRIVATE dollydma)
add_test(NAME dma_mask_alignment COMMAND test-dma-policy)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test-dma-policy PRIVATE -UNDEBUG)
endif()

# Actionable, localized (legacy ASCII PL/EN) troubleshooting hints.
add_executable(test-dma-advice tests/test_dma_advice.c)
target_link_libraries(test-dma-advice PRIVATE dollydma)
add_test(NAME dma_troubleshooting_catalog COMMAND test-dma-advice)
add_executable(dolly-dma-advice src/dolly_dma_advice_cli.c)
target_link_libraries(dolly-dma-advice PRIVATE dollydma)
if(Python3_Interpreter_FOUND)
  add_test(NAME dma_advice_cli_smoke
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/test_dma_advice_cli.py
      $<TARGET_FILE:dolly-dma-advice>)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test-dma-advice PRIVATE -UNDEBUG)
  target_compile_options(dolly-dma-advice PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Portable layout preflight for the built-in classic GadTools GUI.
# This only validates geometry; it does not open Intuition windows.
add_library(dollyguilayout STATIC src/dolly_gui_layout.c)
target_include_directories(dollyguilayout PUBLIC ${PROJECT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyguilayout PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-gui-layout tests/test_gui_layout.c)
target_link_libraries(test-gui-layout PRIVATE dollyguilayout)
add_test(NAME gadtools_640x256_font_fallback COMMAND test-gui-layout)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test-gui-layout PRIVATE -UNDEBUG)
endif()

# Optional screen/font change coalescing: portable core used by all GUI backends.
# The native DOS StartNotify adapter is in amiga/dolly_amiga_pref_notify.c
# and requires an actual AmigaOS NDK (not part of host builds).
add_library(dollyscreenwatch STATIC src/dolly_screen_watch.c)
target_include_directories(dollyscreenwatch PUBLIC ${PROJECT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyscreenwatch PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-screen-watch tests/test_screen_watch.c)
target_link_libraries(test-screen-watch PRIVATE dollyscreenwatch)
add_test(NAME screen_preferences_relayout COMMAND test-screen-watch)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test-screen-watch PRIVATE -UNDEBUG)
endif()

# M0: classic AmigaDOS DOLLY: resource path conventions, OS-independent.
add_library(dollypaths STATIC src/dolly_paths.c)
target_include_directories(dollypaths PUBLIC ${PROJECT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollypaths PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-paths tests/test_paths.c)
target_link_libraries(test-paths PRIVATE dollypaths)
add_test(NAME dolly_assign_resource_paths COMMAND test-paths)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test-paths PRIVATE -UNDEBUG)
endif()

# Optional files may disable only their dependent features.
# Device database entries are untrusted suggestions, NEVER DMA authorization.
add_library(dollyresources STATIC src/dolly_resources.c)
target_include_directories(dollyresources PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(dollyresources PUBLIC dollypaths)
add_library(dollydeviceprofiles STATIC src/dolly_device_profiles.c)
target_include_directories(dollydeviceprofiles PUBLIC ${PROJECT_SOURCE_DIR}/include)
add_executable(test-resources tests/test_resources.c)
target_link_libraries(test-resources PRIVATE dollyresources)
add_test(NAME optional_resources_graceful_degradation COMMAND test-resources)
add_executable(test-device-profiles tests/test_device_profiles.c)
target_link_libraries(test-device-profiles PRIVATE dollydeviceprofiles)
add_test(NAME device_profile_suggestions_only COMMAND test-device-profiles)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyresources PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(dollydeviceprofiles PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(test-resources PRIVATE -UNDEBUG)
  target_compile_options(test-device-profiles PRIVATE -UNDEBUG)
endif()

# Controller-aware DMA guidance (never activates raw I/O by itself).
add_library(dollydmadiagnostics STATIC src/dolly_dma_diagnostics.c)
target_include_directories(dollydmadiagnostics PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(dollydmadiagnostics PUBLIC dollydeviceprofiles dollydma)
add_executable(test-dma-diagnostics tests/test_dma_diagnostics.c)
target_link_libraries(test-dma-diagnostics PRIVATE dollydmadiagnostics)
add_test(NAME sourced_controller_dma_guidance COMMAND test-dma-diagnostics)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollydmadiagnostics PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(test-dma-diagnostics PRIVATE -UNDEBUG)
endif()

# M0 integrity is exact release/version plus a streaming CRC32 checksum.
add_library(dollychecksum STATIC src/dolly_checksum.c)
target_include_directories(dollychecksum PUBLIC ${PROJECT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollychecksum PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-checksum tests/test_checksum.c)
target_link_libraries(test-checksum PRIVATE dollychecksum)
add_test(NAME crc32_integrity_stream COMMAND test-checksum)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test-checksum PRIVATE -UNDEBUG)
endif()
# The database may suggest a conservative Dolly-local transfer limit,
# never alter system configuration or authorize physical DMA access.
add_library(dollyprofileconfig STATIC src/dolly_profile_config.c)
target_include_directories(dollyprofileconfig PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(dollyprofileconfig PUBLIC dollydeviceprofiles)
add_executable(test-profile-config tests/test_profile_config.c)
target_link_libraries(test-profile-config PRIVATE dollyprofileconfig)
add_test(NAME device_local_config_proposals COMMAND test-profile-config)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyprofileconfig PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(test-profile-config PRIVATE -UNDEBUG)
endif()

# Release staging tool verifies both executable and optional asset checksums.
if(Python3_Interpreter_FOUND)
  add_test(NAME release_crc32_manifest
    COMMAND ${Python3_EXECUTABLE}
      ${PROJECT_SOURCE_DIR}/tests/test_release_manifest.py
      ${PROJECT_SOURCE_DIR}/tools/make_release_manifest.py
      ${PROJECT_SOURCE_DIR}/tools/verify_release_manifest.py)
endif()

# In-process executable self-check runs inside Dolly, not a separate launcher.
add_library(dollyselfcheck STATIC
  src/dolly_selfcheck.c src/dolly_selfcheck_embedded.c src/dolly_version.c)
target_include_directories(dollyselfcheck PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(dollyselfcheck PUBLIC dollychecksum)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollyselfcheck PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-selfcheck tests/test_selfcheck_embedded.c)
target_link_libraries(test-selfcheck PRIVATE dollyselfcheck)
add_test(NAME self_executable_crc32_check COMMAND test-selfcheck)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test-selfcheck PRIVATE -UNDEBUG)
endif()

# Post-link executable CRC slot test; no runtime manifest/launcher required.
if(Python3_Interpreter_FOUND)
  add_test(NAME embedded_crc32_stamp
    COMMAND ${Python3_EXECUTABLE}
      ${PROJECT_SOURCE_DIR}/tests/test_stamp_selfcheck.py
      ${PROJECT_SOURCE_DIR}/tools/stamp_selfcheck.py)
endif()

# M0 read-only comparison of RDB-declared geometry against actual media
# or MBR 0x76 container size, obtained independently by the caller.
add_library(dollycapacity STATIC src/dolly_capacity_audit.c)
target_include_directories(dollycapacity PUBLIC ${PROJECT_SOURCE_DIR}/include)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dollycapacity PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(test-capacity-audit tests/test_capacity_audit.c)
target_link_libraries(test-capacity-audit PRIVATE dollycapacity)
add_test(NAME rdb_media_capacity_audit COMMAND test-capacity-audit)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(test-capacity-audit PRIVATE -UNDEBUG)
endif()

# Same audited capacity model is displayed in the read-only CLI tools.
target_link_libraries(dolly-layout PRIVATE dollycapacity)
target_link_libraries(dolly-rdb PRIVATE dollycapacity)
target_link_libraries(dolly-inventory PRIVATE dollycapacity)
