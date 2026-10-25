# Read at Offset

## Overview

After a file is mounted, the user can read a specific number of bytes from a specified offset position within the file. The read section appears only when a file is mounted.

## WASM Export

| Function | Parameters | Return Value | Description |
|----------|------------|--------------|-------------|
| `read_at_offset()` | `(const void* buffer, unsigned int offset, unsigned int size)` | `unsigned int` — bytes actually read (may be clamped), or `0` on error | Reads `size` bytes from `offset` position |

## Parameters

| Parameter | Type | Notes |
|-----------|------|-------|
| `buffer` | `const void*` | Not used (cast to `void`); file data always at offset 0 |
| `offset` | `unsigned int` | Byte position to start reading from |
| `size` | `unsigned int` | Number of bytes to read (chosen by UI: 1, 2, 4, or 8) |

## `read_at_offset()` Implementation

**Source**: `src/file_manager.cc` (lines 71-90)

```cpp
unsigned int read_at_offset(const void* buffer, unsigned int offset, unsigned int size) {
    (void)buffer;  // buffer is unused; file data at offset 0
    if (!file_data_ptr || file_len == 0) {
        status = "error";
        return 0;  // Fail when no file is mounted
    }
    unsigned long offset64 = (unsigned long)offset;
    if (offset64 >= file_len) {
        status = "error";
        return 0;  // Fail when offset is at or past file end
    }
    unsigned int available = (unsigned int)(file_len - offset64);
    if (size > available) {
        status = "file_loaded";
        return available;  // Clamp to available bytes
    }
    status = "file_loaded";
    return size;  // Return requested size
}
```

### Decision Flow

```
read_at_offset(buffer, offset, size)
    │
    ├─ file_data_ptr is null OR file_len == 0?
    │   └─ YES → status = "error", return 0
    │
    ├─ offset >= file_len?
    │   └─ YES → status = "error", return 0 (offset past end)
    │
    ├─ size > available (file_len - offset)?
    │   └─ YES → status = "file_loaded", return available (clamp)
    │
    └─ size <= available
        └─ status = "file_loaded", return size (normal read)
```

### Error States

| Condition | `status` | Return Value |
|-----------|----------|--------------|
| No file mounted (`file_data_ptr == nullptr` or `file_len == 0`) | `"error"` | `0` |
| Offset at or past file end (`offset >= file_len`) | `"error"` | `0` |
| Read would exceed file bounds (but starts in bounds) | `"file_loaded"` | `available` (clamped) |
| Normal read within bounds | `"file_loaded"` | `size` (requested) |

## JavaScript Interaction

### Offset Parsing

`parseOffset(str)` in `js/main.js` supports two formats:
- **Decimal**: `"1024"` → `1024`
- **Hex**: `"0x400"` → `1024`
- Invalid / empty → returns `-1` (triggers alert)

### Size Selection

UI provides a `<select>` dropdown with options: 1Byte, 2Byte, 4Byte, 8Byte.

### Read Flow

```javascript
const offset = parseOffset(offsetInput);  // Parse offset (decimal or 0x-hex)
const size = parseInt(sizeValue, 10);      // 1, 2, 4, or 8

if (offset >= fileLength) {
    alert('Offset ' + offset + ' is beyond file end (' + fileLength + ' bytes).');
    return;  // Fail before calling WASM
}

// Clamp actualSize so we don't read past file end
if (offset + size > fileLength) {
    actualSize = fileLength - offset;
    clamped = true;  // Flag for clamping alert
}

// Read from WASM memory
const wasmMemory = wasmInstance.exports.memory;
const memoryView = new Uint8Array(wasmMemory.buffer);
const result = new Uint8Array(actualSize);
result.set(memoryView.slice(fileBuffer + actualOffset, fileBuffer + actualOffset + actualSize));

// Format as hex for display
const hexString = bytesToHex(result);  // Space-separated uppercase hex pairs
```

### Clamping Behavior

When the requested read extends past the file end:
- `actualSize` is clamped to `fileLength - offset`
- JavaScript sets `clamped = true` and shows an alert
- The hex output reflects available bytes only

### UI

- The `<div id="readSection">` is shown/hidden based on `fileMounted` (JS-side state)
- Read output is displayed in `<div id="readOutput">` as: `"Offset: X | Size: Y Byte(s)\n<HEX STRING>"`
- Console also prints the same hex output in `--- Read at Offset ... ---` format
- When "Debug" checkbox is checked, the first 64 bytes of any mounted file are also printed as hex to the console

## Hex Formatting

```javascript
function bytesToHex(byteArray) {
    return Array.from(byteArray)
        .map(b => b.toString(16).padStart(2, '0').toUpperCase())
        .join(' ');
}
// Example: "DE AD BE EF CA FE"
```
