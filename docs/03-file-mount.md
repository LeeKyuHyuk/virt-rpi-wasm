# File Mount

## Overview

Files selected via the HTML file input are loaded into WASM linear memory and registered. The feature supports two read paths based on file size:

| Size Threshold | Read Method | Streaming |
|----------------|-------------|-----------|
| ≤ 100 MB | `File.arrayBuffer()` | No (single read) |
| > 100 MB | Blob URL + `fetch()` | Yes (chunked) |

## WASM Exports (extern "C")

| Function | Parameters | Return Value | Description |
|----------|------------|--------------|-------------|
| `load_file()` | `(const unsigned char* src, unsigned int length)` | `0` on success, `-1` on zero-length | Records pointer and length in WASM state |
| `get_file_buffer()` | — | `const unsigned char*` | Returns pointer to loaded file data (currently always 0) |
| `get_file_length()` | — | `unsigned long` | Returns file size (unused by JS due to ABI issues) |
| `free_file()` | — | (void) | Resets file state to "idle" |

## Internal State (file_manager.cc)

```cpp
static const unsigned char* file_data_ptr = nullptr;  // Pointer (always unused, JS reads from memory directly)
static unsigned long file_len = 0;                     // File size
static int process_flag = 0;                           // Reserved for future processing logic
static const char* status = "idle";                    // "idle" | "file_loaded" | "error"
```

## `load_file()` Implementation

**Source**: `src/file_manager.cc` (lines 32-39)

```cpp
int load_file(const unsigned char* src, unsigned int length) {
    (void)src;  // src is not used; file data is always at offset 0 in WASM memory
    if (length == 0) { status = "error"; return -1; }  // Decision point: reject zero-length
    file_data_ptr = src;
    file_len = (unsigned long)length;
    status = "file_loaded";
    return 0;
}
```

**Key behaviors**:
- Zero-length files are rejected with `status = "error"`, returns `-1`
- Length zero check is the only error path (decision point)
- `src` parameter is cast to `(void)` — the actual file bytes live in WASM memory at offset 0, not at `src`
- `status` transitions from `"idle"` → `"file_loaded"`

## JavaScript Interaction

### Small Files (≤ 100 MB) — `mountSmallFile()`

```javascript
const arrayBuffer = await file.arrayBuffer();  // Single synchronous read
await mountFileToWasm(arrayBuffer, file.name, file.size);
```

1. `File.arrayBuffer()` reads the entire file synchronously
2. Memory is grown (`wasmMemory.grow()`) to fit the file
3. Data is copied into WASM memory via `Uint8Array.set(view, 0)`
4. `load_file(0, byteLength)` registers the mount

### Large Files (> 100 MB) — `mountLargeFileViaBlob()`

```javascript
activeBlobUrl = URL.createObjectURL(file);  // Keep file alive in browser
const response = await fetch(activeBlobUrl);  // Stream from Blob URL
const reader = response.body.getReader();     // Chunked reader
// ... stream chunks into WASM memory ...
URL.revokeObjectURL(activeBlobUrl);           // Release when done
```

**Why Blob URL?** On Linux, OS file handles used by `File.arrayBuffer()` can expire after a few seconds for large files, causing a `NotReadableError`. A Blob URL keeps the file content accessible in the browser's memory as long as the URL exists.

**Streaming process**:
1. Create Blob URL: `URL.createObjectURL(file)`
2. Grow WASM memory: `wasmMemory.grow(neededPages - currentPages)`
3. Signal WASM: `load_file(0, fileSize)` — status becomes `"file_loaded"`
4. Stream chunks: `reader.read()` → `view.set(value, offset)` per chunk
5. Update UI progress: `Mounting: {pct}% ({size} / {totalSize})`
6. Release: `URL.revokeObjectURL(activeBlobUrl)`

## Auto-Unmount on Double-Mount

If a file is already mounted and the user mounts another file, the existing file is automatically unmounted first:

```javascript
if (fileMounted) { unmountWasmFile(); }
```

This prevents Blob URL leaks — the old Blob URL is revoked during unmount.

## JavaScript-Side State (source of truth)

| Variable | Type | Purpose |
|----------|------|---------|
| `fileMounted` | `boolean` | Tracks mount state; used for UI visibility (not WASM `get_status()`) |
| `fileLength` | `number` | Tracks file size; avoids WASM `get_file_length()` ABI issue |
| `fileBuffer` | `0` (hardcoded) | File data always at memory address 0 |
| `activeBlobUrl` | `string \| null` | Blob URL for large files (null when released) |

## `free_file()` Implementation

```cpp
void free_file() {
    file_data_ptr = nullptr;  // Clear pointer
    file_len = 0;             // Clear length
    status = "idle";          // Reset to idle
}
```

**Note**: This does NOT free WASM memory. The file data remains in linear memory — JavaScript manages the buffer. This function only resets metadata.
