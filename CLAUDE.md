# CLAUDE.md

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 0. Never Assume Project Intent from Name

**Do not implement or propose based solely on the project name.**

- The project name (`virt-rpi-wasm`) does not define requirements.
- Always wait for explicit direction from the user before suggesting or implementing anything.
- Suggestions are allowed — implementation without explicit approval is not.

## 0.1 Plan Output: Always Write to `.claude/plans/`

When creating a plan, the plan file **MUST** be written to this project's `.claude/plans/` directory:

```
.claude/plans/<plan-name>.md
```

- Always check whether the directory exists (`mkdir -p .claude/plans`).
- Never write plans outside of this project directory.
- If a plan for the same task already exists, overwrite it rather than creating a duplicate.

## 0.2 Git Commits: Only Staged Files, Conventional Commits Format

When asked to commit:

- **Do not run `git add` without explicit instruction from the user.** Only commit files the user has explicitly staged.
- **Do not run `git push` without explicit instruction from the user.**
- **Always verify `git status` before committing** — list what will be committed and confirm with the user if unsure.
- **Commit message format follows `.claude/skills/conventional-commits.md`** (Conventional Commits 1.0.0): `<type>[scope]: description` with `fix`, `feat`, `!`, etc.
- Present the chosen type and scope to the user before committing so they can confirm it is correct.

## 0.3 Build Stack: Vanilla C/C++ via LLVM/Clang, No Emscripten

**WebAssembly for this project is produced from Vanilla C/C++ compiled with LLVM/Clang.**

- **Do not use Emscripten.** Only raw `clang` (or `clang++`) targeting `wasm32` is acceptable.
- Do not pull in Emscripten SDKs, ports, or compatibility shims.
- If you encounter references to Emscripten in tooling or dependencies, surface them for the user's decision — do not adopt them silently.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.