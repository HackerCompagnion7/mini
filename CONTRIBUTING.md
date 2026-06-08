# Contributing to MINI

Thank you for your interest in contributing to MINI! This document outlines the process and standards for contributing to the project.

---

## VAVR Methodology

All contributions to MINI must follow the **VAVR** methodology:

1. **Validate** — Confirm the bug or feature request is real and reproducible.
2. **Analyze** — Identify root cause or design the solution. Document your reasoning.
3. **Verify** — Implement the fix/feature and prove it works with tests.
4. **Repair** — If verification reveals issues, loop back to Analyze.

**Never consider work done until it has been demonstrated.** A PR without evidence (test output, ASan report, manual test steps) will not be merged.

---

## Code Standards

### Language & Style

- **C11** with POSIX extensions (`_POSIX_C_SOURCE 200809L`)
- No ncurses, no external dependencies beyond libc
- 4-space indentation, no tabs in source
- `snake_case` for functions and variables
- `UPPER_SNAKE_CASE` for macros and constants
- Every function gets a brief comment header
- All public APIs declared in `include/*.h`, implemented in `src/*.c`

### Binary Size Constraint

The compiled binary must stay **under 50KB**. Before submitting, build with:

```bash
make clean && make
ls -l mini
```

If your change pushes the binary over 50KB, optimize or reconsider the approach.

### Memory Safety

- Run AddressSanitizer on all new code:

```bash
gcc -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -fsanitize=address \
    -Iinclude src/*.c tests/test_core.c -o test_asan && ./test_asan
```

- **Zero leaks, zero use-after-free, zero buffer overflows** — no exceptions.

### Portability

- Must compile and run on **Linux**, **macOS**, and **Termux (Android)**
- No GNU extensions (e.g., no `memmem` — we use a portable replacement)
- No hardcoded paths or platform-specific assumptions

---

## Pull Request Process

### Before Submitting

1. **Fork** the repository and create a feature branch
2. **Write tests** for your change (in `tests/`)
3. **Run all existing tests** — they must pass
4. **Run ASan** — must be clean
5. **Check binary size** — must be under 50KB
6. **Update documentation** if your change affects user-facing behavior

### PR Template

```
## Description
[What does this PR do?]

## VAVR Evidence
- Validate: [How did you confirm the issue?]
- Analyze: [Root cause / design decision]
- Verify: [Test output, ASan report]
- Repair: [Any iteration needed?]

## Test Results
[Paste test output here]

## Binary Size
[Paste `ls -l mini` output here]
```

### Review Criteria

- [ ] All tests pass (core + regression + syntax)
- [ ] ASan clean (0 errors, 0 leaks)
- [ ] Binary under 50KB
- [ ] No new external dependencies
- [ ] Code follows style guide
- [ ] VAVR evidence provided

---

## Reporting Bugs

When filing an issue, please include:

1. **Environment** — OS, Terminal, Termux version (if applicable)
2. **Steps to reproduce** — Minimal sequence to trigger the bug
3. **Expected behavior** — What should happen
4. **Actual behavior** — What happens instead
5. **Build info** — Compiler version, compile flags used

---

## Feature Requests

Feature requests are welcome, but keep in mind MINI's core philosophy:

- **Lightweight** — Must stay under 50KB binary
- **No dependencies** — Pure C + POSIX only
- **Termux first** — Android/Termux is the primary target
- **Nano-inspired** — Simple, approachable, not a full IDE

Features that violate these constraints (syntax highlighting for 50+ languages, LSP support, plugin systems) will likely be declined.

---

## Adding a New Module

If your contribution adds a new source module:

1. Create `src/module.c` and `include/module.h`
2. Add `src/module.c` to `SRCS` in the `Makefile`
3. Add test file `tests/test_module.c`
4. Update `README.md` architecture section
5. Keep the module under 300 lines if possible

---

## Termux Package

The `packages/mini/build.sh` file is the Termux package definition. If you change the build system or add compile flags, update this file accordingly. The package targets:

- **Package name**: `mini`
- **Install**: `pkg install mini`
- **Upstream**: `https://github.com/HackerCompagnion7/mini`

---

Thank you for helping make MINI better!
