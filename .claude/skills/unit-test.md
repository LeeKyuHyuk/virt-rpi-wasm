---
name: unit-test
description: Principles and patterns for writing unit tests — regression, refactoring, feedback, maintainability
category: testing
---

**Use this skill when writing, reviewing, or refactoring unit tests.** Apply these principles to keep tests reliable, fast, and maintainable.

## Core Elements of Unit Testing

Unit tests serve four interdependent purposes. Neglecting any one degrades the value of the suite.

| Element | What it means | What happens when neglected |
|---------|---------------|----------------------------|
| **Regression protection** | Tests encode known failure modes; when a bug is fixed, a test is added so it never returns. | Bugs reappear across code changes. |
| **Refactoring tolerance** | Tests validate external behavior, not internal structure. | Fear of changing code; dead tests that pass but verify nothing useful. |
| **Fast feedback** | Tests run in seconds (or minutes at most). Developers run them constantly. | Tests are skipped; bugs reach production. |
| **Maintainability** | Tests are as clean and well-organized as production code. | Test suites become unmaintainable; developers stop fixing broken tests. |

**Guidance for Claude Code:** When writing or reviewing a test, verify it serves at least two of the four elements. A test that protects against regression but breaks on harmless refactoring has traded one element for another — surface the trade-off.

## The AAA Pattern

Every unit test follows **Arrange → Act → Assert**. Structure each test as three distinct phases:

```
1. **Arrange** — Set up the preconditions: create objects, initialize state, inject dependencies. Keep it minimal: only what the test under test actually needs.

2. **Act** — Execute the single behavior under test. One method call, one operation. Not two, not a workflow.

3. **Assert** — Verify the outcome. Check observable state, return values, or side effects. One assertion per concept under test (though related assertions about the same outcome are fine).
```

### Example (Google Test style)

```cpp
TEST(StringUtil, TruncateToMax) {
    // Arrange
    std::string input = "hello world";
    size_t maxLen = 5;

    // Act
    std::string result = StringUtil::TruncateToMax(input, maxLen);

    // Assert
    EXPECT_EQ(std::string("hello"), result);
}
```

### When to break AAA

If setting up preconditions is so verbose that it obscures what the test is testing, extract the setup into a dedicated test fixture class (named `FooTest` inheriting from `::testing::Test`). The fixture's `SetUp()` method handles arrangement; individual tests then only `Act` and `Assert`. Do not use fixtures as a crutch for poorly organized tests — if a fixture serves more than one unrelated test concept, split it.

## Writing Principles

### Test behavior, not implementation

Tests should verify what a system does, not how it does it. If refactoring the implementation (changing internal data structures, moving methods between classes, renaming private helpers) causes the test to break, the test is coupled to the implementation — extract the invariant behavior it was actually trying to verify and test that instead.

### Full isolation

Each test must be independent. No shared mutable state. No test depends on the order of execution. No test depends on another test having run first. If you delete all tests except one, the remaining test must still pass.

Practical isolation techniques:
- Create fresh objects per test (not shared across tests)
- Use unique temp file paths per test (never leave artifacts behind)
- Inject mocks/stubs for dependencies (network, filesystem, time)
- Reset global/static state in `TearDown()` if unavoidable

### Test independence

This is a corollary of full isolation stated separately because it is so commonly violated. A test that passes only when another test runs first is a hidden dependency. Even if you cannot fully isolate the test (e.g., a file must exist before a test reads it), document the dependency explicitly and create a fixture that sets it up.

### Simple naming

Test names should describe the scenario being tested, not the mechanism. A good test name reads as a sentence: `TEST_F(HttpServer, Returns404ForUnknownPath)`. A bad name: `TEST_F(HttpServer, TestMethod3)`.

Naming convention for this project's tests:
- **Test names** (Google Test `TEST` / `TEST_F`): camelCase, describing the scenario — e.g., `server-basic`, `port-fallback`.
- **Test fixture classes** (Google Test `TEST_F`): `FooTest` — matches the class being tested.
- **Test targets** (CMake / CTest): kebab-case — e.g., `server-basic`, `port-fallback`.
- **Test function names** (standalone `TEST`): camelCase, same as test name — e.g., `serverBasic`.

## Anti-Patterns to Avoid

### Obsession with internal implementation

❌ Checking that a private method was called, verifying the exact container type used internally, or asserting on an implementation-specific ordering.

✅ Check the externally observable result. If the behavior is correct but the internal structure changed, the test should still pass.

### Logic in test code

❌ Putting conditionals, loops, or complex setup logic inside tests. If you need to compute what the expected value is, that computation belongs in the production code (or a helper function shared with production).

✅ Keep test logic flat: arrange inputs, act, compare with `EXPECT_EQ`. If setup is complex, extract a fixture.

Example — bad:
```cpp
TEST(Foo, Process) {
    // Anti-pattern: conditional logic in test
    std::vector<int> inputs;
    for (int i = 0; i < 10; ++i) {
        if (i % 2 == 0) inputs.push_back(i);
    }
    // ...
}
```

Example — good:
```cpp
TEST(Foo, Process) {
    // Arrangement: simple, declarative
    std::vector<int> inputs = {0, 2, 4, 6, 8};

    // Act
    auto result = Foo::Process(inputs);

    // Assert
    EXPECT_EQ(5, result.size());
}
```

### Excessive integration testing masked as unit testing

❌ Unit tests that start databases, open network connections, or exercise the full stack. They are slower, fragile, and make it hard to identify which component is broken.

✅ Unit tests test one unit (function, class, module) in isolation. Use a separate integration test suite for end-to-end scenarios.

**This project's test setup:** The existing `run-server-test` binary tests the HTTP server by starting it in a background thread, waiting for readiness, then using cpp-httplib `Client` to verify endpoints. This is an integration test (it starts a real server). Keep it, but do not call it a unit test. A true unit test of the server would mock the network layer.

## Reference: This Project's Test Setup

This project uses:
- **CMake** with `enable_testing()` and `add_test()` for test registration
- **CTest** as the test runner (`ctest --test-dir build`)
- **cpp-httplib Client** for HTTP-level verification in `server/tests/test_server.cc`
- **A two-library structure** (`server_port_lib` + `server_lib`) to avoid "multiple main" link errors when both `run-server` and `run-server-test` link the same server code

When adding new tests:
1. Add the test source to `server/tests/test_server.cc` (or a new `test_*.cc` in `server/tests/`) for server tests. WASM module tests go in `tests/` (e.g., `tests/test_main.cc`).
2. Add a `TEST` or `TEST_F` block following AAA
3. Register with `add_test()` in CMakeLists.txt with a kebab-case name
4. Verify with `ctest --test-dir build -V`

## Rules

- Every bug fix should include a test that would have caught the bug (or proven the fix works).
- Tests should run in under a few minutes total. If a test suite exceeds that, split into fast unit tests and slow integration tests.
- Test names are the most important documentation a test has. Write them as sentences describing scenarios.
- If a test needs to be skipped, use `GTEST_SKIP()` with a reason — never comment it out or use `#if 0`.
- When a production API changes in a way that breaks tests, evaluate: was the test testing behavior (keep and update) or implementation (rewrite)?
