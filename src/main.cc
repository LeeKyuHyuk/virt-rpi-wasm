/// Minimal WebAssembly module that exports a greeting message.
/// The exported C functions are called from JavaScript,
/// which reads the message from WASM linear memory and
/// prints it via console.log.

// Fixed buffer — simple and works without heap allocation.
static char message_buffer[] = "Hello, world!";

extern "C" {

/// Returns a pointer to the greeting message inside WASM memory.
const char* get_message() {
    return message_buffer;
}

/// Returns the length of the greeting (excluding the null terminator).
int get_message_length() {
    // Length is known at compile time — no std::strlen dependency.
    return 13;
}

} // extern "C"
