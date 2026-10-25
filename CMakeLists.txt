cmake_minimum_required(VERSION 3.20)
project(virt-rpi-wasm CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find the installed clang++ (full path required for wasm-ld)
find_program(CLANGXX clang++-22 clang++ clang++)

# Build virt-rpi.wasm using a custom command (avoids CMake's --dependency-file
# which wasm-ld does not support).
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/virt-rpi.wasm
    COMMAND
        ${CLANGXX} --target=wasm32 -O2 -nostdlib -nostartfiles
        -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined
        -o ${CMAKE_BINARY_DIR}/virt-rpi.wasm
        ${CMAKE_SOURCE_DIR}/src/main.cc
    DEPENDS ${CMAKE_SOURCE_DIR}/src/main.cc
    COMMENT "Building WebAssembly module (wasm32)"
    VERBATIM
)

# Custom target that everyone can depend on; ALL runs it by default.
add_custom_target(virt-rpi_wasm ALL DEPENDS ${CMAKE_BINARY_DIR}/virt-rpi.wasm)

# --- Port scanner library (no main, linkable by both binaries) ---
add_library(server_port_lib server/server_port.cc)

# --- Server library (httplib-based server, no port scanning logic) ---
add_library(server_lib server/server.cc)
target_include_directories(server_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/cpp-httplib
)
target_compile_definitions(server_lib PRIVATE
    PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)
target_compile_options(server_lib PRIVATE -O2)
target_link_libraries(server_lib PRIVATE server_port_lib)
add_executable(run-server $<TARGET_OBJECTS:server_lib>)
target_link_libraries(run-server PRIVATE server_port_lib)

# --- Unit test binary (links server_lib for find_free_port) ---
add_executable(run-server-test server/tests/test_server.cc)
target_include_directories(run-server-test PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/cpp-httplib
)
target_compile_definitions(run-server-test PRIVATE
    PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)
target_link_libraries(run-server-test PRIVATE server_lib server_port_lib)

enable_testing()
add_test(NAME server-basic COMMAND run-server-test)
