/// Minimal WebAssembly module that exports a greeting message.
/// The exported C functions are called from JavaScript,
/// which reads the message from WASM linear memory and
/// prints it via console.log.
///
/// Extended with file-mount functions to support binary file access.
/// No standard library dependencies — compiled with -nostdlib.

#include "file_manager.cc"

// Entry point placeholder (WASM module has no main).
