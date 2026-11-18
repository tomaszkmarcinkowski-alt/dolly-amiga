# Experimental cross-build preset for an externally installed m68k AmigaOS compiler.
# Install a compatible AmigaOS NDK and cross toolchain separately.
# Usage:
# cmake -S . -B build-m68k -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/amigaos-m68k.cmake
# cmake --build build-m68k
# Do not use host CTest binaries as a substitute for execution in WinUAE.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR m68k)
set(CMAKE_C_COMPILER m68k-amigaos-gcc)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(DOLLY_NATIVE_ONLY ON CACHE BOOL "Build only AmigaOS-safe smoke target" FORCE)
set(DOLLY_NATIVE_M68000 ON CACHE BOOL "Require MC68000 code generation" FORCE)

# WinUAE / Kickstart 2.04: libnix avoids an unwanted mathieeedoubbas.library
# dependency observed with the default runtime in the first native smoke run.
# INIT variables apply on a fresh CMake configuration, not an existing cache.
set(CMAKE_C_FLAGS_INIT "-noixemul")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-noixemul")
