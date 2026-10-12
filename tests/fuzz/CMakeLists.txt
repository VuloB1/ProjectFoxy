# Fuzzing harnesses. A project of its own (not part of the normal build): it needs Clang, and compiles only what it
# fuzzes, so it builds in a minute and does not need vcpkg.
#
#   cmake -S tests/fuzz -B build/fuzz -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH=<Qt>
#   cmake --build build/fuzz
#   build/fuzz/fuzz_animated_gif -max_total_time=60 tests/fuzz/corpus/gif
#
# Qt itself is not instrumented (only our code is), so a bug INSIDE Qt shows up as a crash, not as a sanitizer report.
cmake_minimum_required(VERSION 3.21)
project(ProjectFoxyFuzz CXX)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "The fuzzers need Clang (libFuzzer): configure with -DCMAKE_CXX_COMPILER=clang++")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC OFF)
find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET libwebp libwebpdemux)

set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core)

function(add_fuzzer name ext)
    add_executable(${name} fuzz_animated_decoder.cpp ${CORE}/decoders/AnimatedDecoder.cpp)
    target_compile_definitions(${name} PRIVATE FUZZ_EXT="${ext}")
    target_include_directories(${name} PRIVATE ${CORE})
    # address + undefined: out-of-range reads and writes, use after free, integer overflow, bad shifts...
    target_compile_options(${name} PRIVATE -g -O1 -fno-omit-frame-pointer -fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=undefined)
    target_link_options(${name} PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(${name} PRIVATE Qt6::Core Qt6::Gui PkgConfig::WEBP)
endfunction()

add_fuzzer(fuzz_animated_gif gif)
add_fuzzer(fuzz_animated_apng png)
add_fuzzer(fuzz_animated_webp webp)
