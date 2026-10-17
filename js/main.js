/// Loads virt-rpi.wasm, reads the exported message from linear memory,
/// and prints it via console.log.

const wasmUrl = new URL('/build/virt-rpi.wasm', location.origin);

async function main() {
    const response = await fetch(wasmUrl);
    const bytes = await response.arrayBuffer();
    const { instance } = await WebAssembly.instantiate(bytes);

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

main();
