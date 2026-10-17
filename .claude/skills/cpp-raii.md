---
name: cpp-raii
description: Apply RAII and smart pointers to replace manual resource management in C++
category: languages
---

# Cpp Raii

Use this when modernizing C++ that leaks, double-frees, or manually pairs `new`/`delete` and `open`/`close`, by tying resource lifetimes to scope.

## Why RAII

Resource Acquisition Is Initialization ensures that every acquired resource is automatically released when it goes out of scope — whether by normal return, exception, or `goto`. Because the destructor of a stack-allocated object is **guaranteed to run**, no manual cleanup is needed, eliminating entire classes of bugs: resource leaks, double-frees, and use-after-frees.

## Steps

1. **Find raw owning resources:** `new`/`delete`, `malloc`/`free`, `fopen`, file descriptors, locks, sockets, handles — each is a candidate for an RAII wrapper.

2. **Replace owning raw pointers with smart pointers:** `std::unique_ptr` (sole ownership) or `std::shared_ptr` (shared); use `std::make_unique` / `std::make_shared` instead of bare `new`.

3. **Wrap non-memory resources** (files, mutexes, handles) in a class whose destructor releases them, or use existing guards like `std::lock_guard` / `std::scoped_lock`.

4. **Apply the Rule of Zero:** prefer types that need no custom destructor / copy / move because members manage themselves; if you write one special member, consider all five. Destructors MUST be `noexcept` — throwing during stack unwinding calls `std::terminate`. Move constructors should also be `noexcept` — non-`noexcept` moves force containers to copy instead of move, degrading performance.

5. **Pass non-owning references as raw pointers or references** (or `std::span` / `std::string_view`); reserve smart pointers for transferring or sharing ownership. Use `std::unique_ptr` return values for factory functions that transfer ownership (`std::unique_ptr<Foo> FooFactory()`).

6. **Build with sanitizers** (`-fsanitize=address,leak,undefined`) and run the suite to confirm leaks and use-after-free are gone.

## Custom Deleters (C API Interop)

For C APIs without RAII wrappers (glib, CUDA, POSIX), use `unique_ptr` with a custom deleter — often a one-liner:

```cpp
std::unique_ptr<FILE, decltype(&fclose)> file(fopen("data.txt", "r"), fclose);
std::unique_ptr<char, decltype(&std::free)> buf(std::malloc(N), std::free);
```

Common deleter functions: `fclose`, `std::free`, `close` (POSIX fd), `g_free` (glib), `cudaFree` (CUDA), `DeleteGlobalRef` (JNI).

## Rules

- Never `delete` a pointer owned by a smart pointer, and never store the same raw pointer in two owning smart pointers — that's a double free.
- Default to `unique_ptr`; reach for `shared_ptr` only when ownership is genuinely shared, and break reference cycles with `weak_ptr`.
- Don't pass `shared_ptr` by value through call chains that don't take ownership — it churns the atomic refcount; pass `const&` or the raw pointer.
- Acquire resources in the constructor and release in the destructor; destructors MUST be `noexcept`. If acquisition can fail (e.g., `fopen`), use a factory function or a two-phase init that returns an RAII wrapper — don't put partial state in the object.
- Prefer `make_unique` / `make_shared` over `new` to stay exception-safe and avoid leaks from evaluation-order pitfalls.
- For C APIs without RAII wrappers, use `unique_ptr` with a custom deleter (see Custom Deleters section above).
