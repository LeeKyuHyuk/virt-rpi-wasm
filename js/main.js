/// Loads virt-rpi.wasm, reads the exported message from linear memory,
/// and prints it via console.log.
///
/// Extended with file-mount functions:
///  - mountFileToWasm(arrayBuffer, fileName, fileSize): reads file data,
///    grows WASM memory, copies chunks, and mounts it.
///  - mountLargeFileViaBlob(file): for large files, creates a Blob URL to
///    keep the file alive in the browser, then streams from that URL via
///    fetch() chunk by chunk (avoids OS file handle expiration on Linux).

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

    // For large files, read from Blob URL via fetch (streaming, avoids NotReadableError)
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
