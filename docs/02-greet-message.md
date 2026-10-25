# Greet Message

## Overview

The original (pre-file-mount) feature: the WASM module exports a hardcoded greeting message that JavaScript reads from linear memory and displays via `console.log`.

## WASM Exports

| Function | Return Type | Description |
|----------|-------------|-------------|
| `get_message()` | `const char*` | Pointer to greeting message in WASM memory |
| `get_message_length()` | `int` | Length of greeting (13, excluding null terminator) |

## Implementation Details

**Source**: `src/file_manager.cc` (lines 19-27)

```cpp
static char message_buffer[] = "Hello, world!";

extern "C" {

const char* get_message() {
    return message_buffer;
}

int get_message_length() {
    return 13;
}

}
```

Key design:
- `get_message_length()` returns a compile-time constant (13) — no `std::strlen` dependency
- Stored in `static char[]` inside WASM linear memory (offset 0)
- JavaScript reads the message by: `memory.slice(bufferPtr, bufferPtr + length + 1)`

## JavaScript Interaction

```javascript
const bufferPtr = instance.exports.get_message();
const length = instance.exports.get_message_length();
const memoryView = new Uint8Array(memory.buffer);
const bytes_ = memoryView.slice(Number(bufferPtr), Number(bufferPtr) + length + 1);
const message = new TextDecoder().decode(bytes_);
console.log(message);  // "Hello, world!"
```

The `+1` in the slice accounts for the null terminator.

## Notes

- This feature is preserved from the original project; the file-mount extensions are layered on top.
- No tests are written for `get_message()` / `get_message_length()` in the unit test suite — the message is a fixed string, and testing it would violate the anti-coverage rule (one-line getter, no decision point).
