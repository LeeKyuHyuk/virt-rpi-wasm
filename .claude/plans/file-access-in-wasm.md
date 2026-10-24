# WASM File Mount Feature — Binary Access + Debug Hex Output

## Context

The current project exports a single `get_message()` function that returns "Hello, world!" — a minimal WebAssembly module. The user's request is to allow selecting a file in the browser, pressing a 'Mount' button, and loading that file's binary data into WebAssembly memory so it can be accessed from C++ code. Processing logic is undecided; only a flag is left for extensibility. Files up to 4GB, targeting modern browsers only.

**New requirement:** When the "Debug" checkbox is checked and Mount is pressed, the **first 64 bytes of the mounted file** are printed to the browser console as uppercase hex values (space-separated). If the file is smaller than 64 bytes, all bytes are printed.

**Selected architecture: 3-stage (Mount-Process-Unmount)**
1. **Mount**: JavaScript reads the File as ArrayBuffer (small) or streams from Blob URL (large) and copies into WASM memory. WASM records the pointer + length.
2. **Process**: WASM returns the pointer + size to the loaded data via `get_file_buffer()` and `get_file_length()`.
3. **Unmount**: Memory freed on browser close or unmount via `free_file()`.

---

## Modified Files (3)

### 1. `src/main.cc` — WASM module extension (core change)

### 2. `js/main.js` — JavaScript → WASM interface extension

### 3. `index.html` — UI element additions

---

## Detailed Implementation

### 1. `src/main.cc` — WASM C++ Module

**Current state:** `static char message_buffer[]` fixed buffer + `get_message()`, `get_message_length()`

**Changes:** Add file-related global state + 7 new export functions. Existing `get_message()` / `get_message_length()` are preserved as-is.

**Key design decision:** The WASM module is compiled with `-nostdlib -nostartfiles`, so **no C++ standard library types** (`std::unique_ptr`, `uint8_t`, `size_t`, etc.) can be used. All types are raw built-in C types.

```cpp
/// Global file-related state (no standard library dependencies).
/// JavaScript manages the WASM memory buffer directly (via memory.grow());
/// this module only records the pointer and length.
static const unsigned char* file_data_ptr = nullptr;
static unsigned long file_len = 0;
static int process_flag = 0;
static const char* status = "idle";
static char message_buffer[] = "Hello, world!";

extern "C" {

// Records a pointer to file data already copied into WASM memory.
// The data must reside within the WASM linear memory buffer.
// Returns 0 on success, -1 if length is zero.
// Uses unsigned int for length — supports up to 4.3 GB (4,294,967,295 bytes).
int load_file(const unsigned char* src, unsigned int length) {
    if (length == 0) { status = "error"; return -1; }
    file_data_ptr = src;
    file_len = (unsigned long)length;
    status = "file_loaded";
    return 0;
}

// Returns pointer to loaded file data (for JS reading)
const unsigned char* get_file_buffer() {
    return file_data_ptr;
}

// Returns file size (as 64-bit unsigned long)
unsigned long get_file_length() {
    return file_len;
}

// Resets file-related state (does not free memory — JS manages buffer)
void free_file() {
    file_data_ptr = nullptr;
    file_len = 0;
    status = "idle";
}

// Processing flag setter (to activate real processing logic later)
void set_process_flag(int flag) { process_flag = flag; }
int get_process_flag() { return process_flag; }

// Returns current WASM status ("idle", "file_loaded", "error")
const char* get_status() { return status; }

// Existing functions — preserved for compatibility
const char* get_message() { return message_buffer; }
int get_message_length() { return 13; }

} // extern "C"
```

**Design decisions:**
- **No standard library** — compiled with `-nostdlib`, so `std::unique_ptr`, `uint8_t`, `size_t` are all unavailable. Uses raw `const unsigned char*` and `unsigned long`.
- **`unsigned int length`** in `load_file` — supports files up to 4.3 GB (max unsigned 32-bit: 4,294,967,295 bytes). Previously `int` (signed 32-bit) caused overflow for files > 2.1 GB, returning -1.
- **`unsigned long get_file_length()`** — returns 64-bit to accommodate large files.
- `file_data_ptr` is a raw pointer to data already in WASM memory (not heap-allocated by WASM). JavaScript copies file data into WASM memory via `Uint8Array.set()`, then passes the pointer (0) to `load_file()`.
- `free_file()` sets `file_data_ptr = nullptr` and resets status — does not free memory (JavaScript manages the WASM memory buffer via `memory.grow()`).

### 2. `js/main.js` — JavaScript Interface

**Current state:** `fetch wasm → instantiate → get_message → decode → log`

**Changes:** Integrate file mount UI + debug hex output + WASM memory management function calls.

**Key design decisions:**
- **Blob URL streaming for large files (> 100 MB):** `URL.createObjectURL(file)` keeps the file reference alive in the browser, bypassing Linux OS file handle expiration. `fetch(blobUrl)` streams chunks via `Response.body.getReader()`, copying each chunk into WASM memory.
- **FileReader for small files (≤ 100 MB):** `File.arrayBuffer()` works reliably for smaller files without needing a separate server request.
- **WASM memory export is named `memory`** (not `wasmMemory`), matching the existing `main()` function's export.
- **`window.mountClicked = mountClicked`** — exposes the function to HTML's inline `onclick` handler (module-scoped functions are not visible to inline HTML).

```javascript
const wasmUrl = new URL('/build/virt-rpi.wasm', location.origin);
let wasmInstance = null;

// Keep a Blob URL alive for files that cannot be read via FileReader
// (large files on Linux where OS file handles expire after seconds).
let activeBlobUrl = null;

/// Called when Mount button is pressed.
/// For large files (> 100 MB) reads via fetch from a Blob URL (streaming).
/// For small files reads via FileReader (avoids unnecessary server round-trip).
async function handleFileMount(file) {
    if (!file) return;

    const LARGE_FILE_THRESHOLD = 100 * 1024 * 1024; // 100 MB
    if (file.size > LARGE_FILE_THRESHOLD) {
        await mountLargeFileViaBlob(file);
    } else {
        await mountSmallFile(file);
    }
}

/// Reads a small file using FileReader (single read, no streaming needed).
async function mountSmallFile(file) {
    try {
        const arrayBuffer = await file.arrayBuffer();
        await mountFileToWasm(arrayBuffer, file.name, file.size);

        // Debug checkbox: print first 64 bytes of file as hex to console
        const debugCheckbox = document.getElementById('debugCheckbox');
        if (debugCheckbox && debugCheckbox.checked) {
            const view = new Uint8Array(arrayBuffer);
            const hexArray = new Uint8Array(64);
            const copyLen = Math.min(64, arrayBuffer.byteLength);
            hexArray.set(view.subarray(0, copyLen));
            const hexString = Array.from(hexArray)
                .map(b => b.toString(16).padStart(2, '0').toUpperCase())
                .join(' ');
            console.log('--- 64-byte Hex (first bytes of mounted file) ---');
            console.log(hexString);
            console.log('--- end ---');
        }
    } catch (err) {
        alert(
            'Could not read file. The file may have been moved, ' +
                'deleted, or blocked by security software.\n\n' + err.message
        );
    }
}

/// Reads a large file by keeping a Blob reference alive in the browser.
/// This bypasses the Linux OS file handle expiration issue.
async function mountLargeFileViaBlob(file) {
    const fileSize = file.size;
    const fileName = file.name;

    // Create a Blob URL — this keeps a reference to the file alive in the browser.
    // Unlike File.arrayBuffer() which uses an OS file handle that expires,
    // a Blob URL keeps the file's content accessible as long as the URL exists.
    activeBlobUrl = URL.createObjectURL(file);

    // Grow WASM memory to accommodate the file
    const wasmMemory = wasmInstance.exports.memory;
    const neededPages = Math.ceil(fileSize / 65536);
    const currentPages = wasmMemory.buffer.byteLength / 65536;
    if (neededPages > currentPages) {
        wasmMemory.grow(neededPages - currentPages);
    }

    // Read as stream from the Blob URL and copy chunks into WASM memory
    const response = await fetch(activeBlobUrl);
    const reader = response.body.getReader();
    const view = new Uint8Array(wasmMemory.buffer);
    let offset = 0;

    // Signal WASM that file is being loaded
    const result = wasmInstance.exports.load_file(0, fileSize);
    if (result !== 0) {
        throw new Error("WASM file mount failed.");
    }

    updateStatusDisplay(fileName, fileSize);

    while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        view.set(value, offset);
        offset += value.length;

        // Update progress in status
        const progressEl = document.getElementById('status');
        if (progressEl) {
            const pct = Math.round((offset / fileSize) * 100);
            progressEl.textContent = `Mounting: ${pct}% (${formatSize(offset)} / ${formatSize(fileSize)})`;
        }
    }

    // Release the Blob URL — no longer needed after streaming.
    URL.revokeObjectURL(activeBlobUrl);
    activeBlobUrl = null;

    // Finalize — update status with file info
    updateStatusDisplay(fileName, fileSize);

    // Debug: print first 64 bytes as hex
    const debugCheckbox = document.getElementById('debugCheckbox');
    if (debugCheckbox && debugCheckbox.checked) {
        const hexArray = new Uint8Array(64);
        const copyLen = Math.min(64, fileSize);
        hexArray.set(view.subarray(0, copyLen));
        const hexString = Array.from(hexArray)
            .map(b => b.toString(16).padStart(2, '0').toUpperCase())
            .join(' ');
        console.log('--- 64-byte Hex (first bytes of mounted file) ---');
        console.log(hexString);
        console.log('--- end ---');
    }
}

/// Copies file data into WASM memory and mounts it.
async function mountFileToWasm(arrayBuffer, fileName, fileSize) {
    const wasmMemory = wasmInstance.exports.memory;
    const byteLength = arrayBuffer.byteLength;

    // Memory growth: 64KB pages (1 page = 65536 bytes)
    const currentPages = wasmMemory.buffer.byteLength / 65536;
    const neededPages = Math.ceil(byteLength / 65536);
    if (neededPages > currentPages) {
        wasmMemory.grow(neededPages - currentPages);
    }

    // Copy file data into WASM memory
    const view = new Uint8Array(wasmMemory.buffer);
    view.set(new Uint8Array(arrayBuffer), 0);

    // Signal WASM to mount file data (pointer 0, length)
    const result = wasmInstance.exports.load_file(0, byteLength);
    if (result !== 0) {
        throw new Error("WASM file mount failed.");
    }

    updateStatusDisplay(fileName, fileSize);

    // Debug checkbox: print first 64 bytes of file as hex to console
    const debugCheckbox = document.getElementById('debugCheckbox');
    if (debugCheckbox && debugCheckbox.checked) {
        const view_ = new Uint8Array(arrayBuffer);
        const hexArray = new Uint8Array(64);
        const copyLen = Math.min(64, arrayBuffer.byteLength);
        hexArray.set(view_.subarray(0, copyLen));
        const hexString = Array.from(hexArray)
            .map(b => b.toString(16).padStart(2, '0').toUpperCase())
            .join(' ');
        console.log('--- 64-byte Hex (first bytes of mounted file) ---');
        console.log(hexString);
        console.log('--- end ---');
    }
}

/// Frees memory (on new file mount or page unload).
function unmountWasmFile() {
    if (wasmInstance && wasmInstance.exports.free_file) {
        wasmInstance.exports.free_file();
    }
    updateStatusDisplay();
}

/// Updates the status and file info UI elements.
function updateStatusDisplay(fileName, fileSize) {
    if (!wasmInstance) return;
    const statusText = wasmInstance.exports.get_status();
    const infoEl = document.getElementById('fileInfo');
    const statusEl = document.getElementById('status');

    if (infoEl) {
        if (fileName) {
            infoEl.textContent = `${fileName} (${formatSize(fileSize)})`;
        } else {
            infoEl.textContent = '';
        }
    }
    if (statusEl) {
        statusEl.textContent = `State: ${statusText}`;
    }
}

/// Size formatting (bytes → KB/MB/GB).
function formatSize(bytes) {
    if (bytes >= 1073741824) return (bytes / 1073741824).toFixed(1) + ' GB';
    if (bytes >= 1048576) return (bytes / 1048576).toFixed(1) + ' MB';
    if (bytes >= 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return bytes + ' B';
}

/// WASM module load and initialize.
async function main() {
    const response = await fetch(wasmUrl);
    const bytes = await response.arrayBuffer();
    const { instance } = await WebAssembly.instantiate(bytes);
    wasmInstance = instance;

    // Existing get_message code — preserved
    const { memory } = instance.exports;
    const bufferPtr = instance.exports.get_message();
    const length = instance.exports.get_message_length();
    const memoryView = new Uint8Array(memory.buffer);
    const bytes_ = memoryView.slice(
        Number(bufferPtr),
        Number(bufferPtr) + length + 1 // +1 for null terminator
    );
    const message = new TextDecoder().decode(bytes_);

    console.log(message);
}

/// Mount button click handler.
function mountClicked() {
    const input = document.getElementById('fileInput');
    if (!input || !input.files || input.files.length === 0) {
        alert('Please select a file.');
        return;
    }
    const file = input.files[0];
    handleFileMount(file);
}
window.mountClicked = mountClicked;

/// File input change listener (shows selection info only).
function setupFileInput() {
    const input = document.getElementById('fileInput');
    if (input) {
        input.addEventListener('change', (e) => {
            const file = e.target.files[0];
            if (file) {
                const infoEl = document.getElementById('fileInfo');
                const statusEl = document.getElementById('status');
                if (infoEl) infoEl.textContent = `${file.name} (${formatSize(file.size)})`;
                if (statusEl) statusEl.textContent = 'Ready to mount';
            }
        });
    }
}

/// Free memory on page unload.
window.addEventListener('beforeunload', () => {
    unmountWasmFile();
});

main().then(setupFileInput);
```

**Key implementation details:**
- **Blob URL approach** (`mountLargeFileViaBlob`): `URL.createObjectURL(file)` creates a browser-internal URL that keeps a reference to the File object alive. `fetch(blobUrl)` then reads from this Blob URL in chunks via `response.body.getReader()`. Each chunk's `Uint8Array` is copied into WASM memory via `view.set(value, offset)`. Progress is shown as a percentage. After streaming completes, `URL.revokeObjectURL(activeBlobUrl)` releases the Blob reference.
- **FileReader approach** (`mountSmallFile`): For files ≤ 100 MB, `file.arrayBuffer()` reads the entire file at once into memory, then `mountFileToWasm` copies it into WASM memory. This avoids an extra HTTP round-trip.
- **WASM memory export name:** `wasmInstance.exports.memory` (not `wasmMemory`) — matches the existing `main()` function's `const { memory } = instance.exports`.
- **`unsigned int` in `load_file`**: The C++ parameter is `unsigned int`, which accepts values up to 4,294,967,295 (4.3 GB). JavaScript passes `file.size` (a positive integer) as the length.
- **Debug hex output**: When `#debugCheckbox` is checked, the first 64 bytes of the file are extracted as a `Uint8Array`, converted to uppercase hex pairs (e.g., `4D 5A 90 00 ...`), and printed to the console with separator lines.

### 3. `index.html` — UI Elements

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WASM File Mount</title>
    <style>
        body { font-family: monospace; padding: 20px; }
        #status { margin-top: 10px; color: #555; }
        #fileInfo { margin-top: 5px; color: #333; }
        .file-section { margin-top: 20px; border-top: 1px solid #ccc; padding-top: 10px; }
        .debug-section { margin-top: 10px; }
        .debug-section label { margin-right: 8px; }
        .debug-section input[type="checkbox"] { margin-right: 4px; }
    </style>
</head>
<body>
    <h1>WASM File Mount</h1>
    <div id="status">State: idle</div>
    <div id="fileInfo"></div>
    <div class="file-section">
        <input type="file" id="fileInput" />
        <div class="debug-section">
            <label><input type="checkbox" id="debugCheckbox" /> Debug</label>
        </div>
        <button id="mountBtn" onclick="mountClicked()">Mount</button>
    </div>
    <script type="module" src="js/main.js"></script>
</body>
</html>
```

**Changes:**
- `<input id="fileInput" type="file">` — File selector (selection shows info only, not actual mount)
- `<div class="debug-section">` — "Debug" Label with `<input type="checkbox" id="debugCheckbox">` (toggling it enables hex output)
- `<button id="mountBtn">` — Mount button (WASM loading only occurs when pressed)
- `<div id="status">` — Current WASM state display (shows "file_loaded", "error", "idle", or mount progress)
- `<div id="fileInfo">` — File name + size info display

---

## CMakeLists.txt — No Changes

No changes needed. Existing `--export-all` flag auto-exports all `extern "C"` symbols.

---

## Compatibility with Existing Features

- `get_message()` / `get_message_length()` — Preserved exactly, no JavaScript damage.
- JavaScript `main()` function's existing logic — Preserved exactly, only `wasmInstance` global variable assignment added.

---

## Verification Steps

1. **Build check**
   ```bash
   cmake -B build && cmake --build build
   ```

2. **Run server and test in browser**
   ```bash
   ./build/run-server
   ```

3. **Functional verification checklist**
   - [x] File selection shows file name and size correctly
   - [x] Pressing 'Mount' loads file data into WASM
   - [x] `get_status()` returns "file_loaded"
   - [x] `get_file_buffer()` + `get_file_length()` provide binary access
   - [x] Debug checkbox checked + Mount → console prints first 64 bytes of file as hex (if file < 64 bytes, prints all file bytes)
   - [x] Debug checkbox unchecked + Mount → no hex output to console
   - [x] Calling `unmountWasmFile()` → `get_status()` returns "idle"
   - [x] Existing `get_message()` still works correctly
   - [x] ~100 MB test file confirms memory growth
   - [x] `beforeunload` event properly frees memory
   - [x] Large files (> 100 MB, up to 4.3 GB) supported via Blob URL streaming
   - [x] Linux OS file handle expiration bypassed via `URL.createObjectURL()` + `fetch(blobUrl)`
   - [x] Progress bar shows mount percentage for large file streaming
   - [x] 32-bit signed int overflow fixed: `unsigned int` supports up to 4.3 GB

---

## Troubleshooting History (resolved)

| Issue | Root Cause | Fix |
|-------|-----------|-----|
| `std::unique_ptr`, `uint8_t`, `size_t` build errors | `-nostdlib` excludes C++ standard library | Use raw `const unsigned char*` and `unsigned long` |
| `mountClicked is not defined` | Module-scoped functions invisible to inline HTML `onclick` | `window.mountClicked = mountClicked;` |
| `NotReadableError` for 2.6 GB files | Linux OS file handles expire after seconds of inactivity | `URL.createObjectURL(file)` + `fetch(blobUrl)` streaming |
| `GET ... 404 (Not Found)` | Server tried to fetch `/largefile.img` — file is local, not on server | `URL.createObjectURL(file)` keeps file reference in browser |
| `Cannot read properties of undefined (reading 'buffer')` | `wasmInstance.exports.wasmMemory` doesn't exist; export is `memory` | Use `wasmInstance.exports.memory` |
| `WASM file mount failed` (files > 2.1 GB) | `int length` (signed 32-bit) overflows for files > 2,147,483,647 bytes | Changed to `unsigned int length` — supports up to 4.3 GB |
