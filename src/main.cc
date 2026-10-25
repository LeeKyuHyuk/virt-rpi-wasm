/// Minimal WebAssembly module that exports a greeting message.
/// The exported C functions are called from JavaScript,
/// which reads the message from WASM linear memory and
/// prints it via console.log.
///
/// Extended with file-mount functions to support binary file access.
/// No standard library dependencies — compiled with -nostdlib.

// Fixed buffer — simple and works without heap allocation.
static char message_buffer[] = "Hello, world!";

/// Global file-related state (no standard library dependencies).
/// JavaScript manages the WASM memory buffer directly (via wasmMemory.grow());
/// this module only records the pointer and length.
static const unsigned char* file_data_ptr = nullptr;
static unsigned long file_len = 0;
static int process_flag = 0;
static const char* status = "idle";

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

/// Records a pointer to file data already copied into WASM memory.
/// The data must reside within the WASM linear memory buffer.
/// Returns 0 on success, -1 if length is negative.
/// Uses unsigned int for length — supports up to 4.3 GB.
int load_file(const unsigned char* src, unsigned int length) {
    if (length == 0) { status = "error"; return -1; }
    file_data_ptr = src;
    file_len = (unsigned long)length;
    status = "file_loaded";
    return 0;
}

/// Returns pointer to loaded file data (for JS to read from WASM memory).
const unsigned char* get_file_buffer() {
    return file_data_ptr;
}

/// Returns the size of the currently loaded file (as a 64-bit unsigned long).
unsigned long get_file_length() {
    return file_len;
}

/// Resets file-related state (does not free memory — JS manages buffer).
void free_file() {
    file_data_ptr = nullptr;
    file_len = 0;
    status = "idle";
}

/// Processing flag setter (to activate real processing logic later).
void set_process_flag(int flag) { process_flag = flag; }

/// Returns current processing flag.
int get_process_flag() { return process_flag; }

/// Returns current WASM status ("idle", "file_loaded", "error").
const char* get_status() { return status; }

/// Reads `size` bytes from `offset` position in the mounted file.
/// Copies the bytes to a buffer pointer in WASM memory.
/// Returns the actual number of bytes read (may be less than requested
/// if the offset is near the end of the file — file end is clamped).
unsigned int read_at_offset(const void* buffer, unsigned int offset, unsigned int size) {
    if (!file_data_ptr || file_len == 0) {
        status = "error";
        return 0;
    }
    // Clamp to file bounds
    unsigned long offset64 = (unsigned long)offset;
    if (offset64 >= file_len) {
        status = "error";
        return 0;
    }
    unsigned int available = (unsigned int)(file_len - offset64);
    if (size > available) {
        status = "file_loaded";
        return available;
    }
    status = "file_loaded";
    return size;
}

} // extern "C"
