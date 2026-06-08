# MINI

> A modern, lightweight terminal text editor for Android/Termux — inspired by Nano.

<p align="center">
  <strong>36KB binary · Pure C11 · No ncurses · UTF-8 · Termux native</strong>
</p>

---

## Features

| Feature | Detail |
|---|---|
| **Zero dependencies** | Pure C11 + POSIX `termios`. No ncurses, no external libs — only libc. |
| **Tiny footprint** | ~36KB compiled binary. Fits on any device. |
| **UTF-8 native** | Full 1–4 byte sequence support: accented chars, CJK, emoji. |
| **Dynamic line numbers** | 3–6 character width, adapts to file size. Gray/cyan coloring. |
| **Real-time error detection** | Unclosed brackets/quotes → RED. Risky patterns → YELLOW. Correct → WHITE. |
| **Smart status bar** | `[MINI | filename | Line X/Total | OK/ERROR]` with color indicators. |
| **Error zone** | Auto-appears on error, auto-clears 1 second after correction. |
| **Text search** | Forward + wrap-around (`Ctrl+R`). |
| **Atomic save** | `mkstemp` + `rename` pattern — no corruption on crash. |
| **Termux ready** | Builds and runs natively on Android/Termux. |

---

## Keyboard Shortcuts

| Key | Action |
|---|---|
| `Ctrl+S` | Save file |
| `Ctrl+R` | Search text |
| `Ctrl+X` | Quit editor |
| `Ctrl+G` | Help overlay |
| `Ctrl+K` | Cut current line |
| `Ctrl+A` | Go to line start |
| `Ctrl+E` | Go to line end |
| `Tab` | Insert 4 spaces |
| `Arrow keys` | Navigate |
| `Home / End` | Line start / end |
| `Page Up / Down` | Scroll page |
| `Backspace / Delete` | Delete characters |
| `Enter` | Insert newline |

---

## Build

### Prerequisites

- GCC or Clang (C11 support)
- Make
- POSIX-compliant system (Linux, macOS, Termux)

### Compile

```bash
git clone https://github.com/HackerCompagnion7/mini.git
cd mini
make
```

### Install

```bash
sudo make install    # installs to /usr/local/bin/mini
```

### Run

```bash
mini [filename]
```

---

## Run Tests

```bash
# Compile and run unit tests
gcc -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -Iinclude \
    tests/test_core.c src/buffer.c src/editor.c src/term.c src/file.c \
    src/search.c src/syntax.c -o test_core && ./test_core

gcc -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -Iinclude \
    tests/test_regression.c src/buffer.c src/editor.c src/term.c src/file.c \
    src/search.c src/syntax.c -o test_regression && ./test_regression

gcc -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -Iinclude \
    tests/test_syntax.c src/buffer.c src/editor.c src/term.c src/file.c \
    src/search.c src/syntax.c -o test_syntax && ./test_syntax

# AddressSanitizer build
gcc -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -fsanitize=address \
    -Iinclude tests/test_core.c src/buffer.c src/editor.c src/term.c \
    src/file.c src/search.c src/syntax.c -o test_asan && ./test_asan
```

---

## Architecture

```
mini/
├── src/
│   ├── main.c          # Entry point, rendering, key bindings
│   ├── editor.c        # Editor state, cursor, scrolling, insert/delete
│   ├── buffer.c        # Line-based buffer, UTF-8 helpers
│   ├── term.c          # Terminal control (raw mode, ANSI escapes)
│   ├── file.c          # File I/O (load, atomic save)
│   ├── search.c        # Text search with wrap-around
│   └── syntax.c        # Bracket/quote mismatch detection & coloring
├── include/
│   ├── editor.h
│   ├── buffer.h
│   ├── term.h
│   ├── file.h
│   ├── search.h
│   └── syntax.h
├── tests/
│   ├── test_core.c         # 20 core unit tests
│   ├── test_regression.c   # 8 regression tests
│   └── test_syntax.c       # Syntax module tests
├── packages/
│   └── mini/
│       └── build.sh        # Termux package build script
├── Makefile
├── LICENSE
├── CONTRIBUTING.md
└── README.md
```

### Design Principles

1. **No ncurses** — Direct ANSI escape sequences + POSIX `termios` for raw mode. Smaller binary, no dependency.
2. **Line-based buffer** — Each line is an independent dynamic array (`Line` struct: `data`, `len`, `cap`). Doubling strategy for amortized O(1) insertion.
3. **UTF-8 byte-level** — All operations work at byte level with helper functions for char boundaries (`utf8_char_len`, `utf8_prev_char_pos`, `utf8_next_char_pos`).
4. **Atomic save** — Write to temp file via `mkstemp`, then `rename`. Never corrupts the original on crash.
5. **Batched rendering** — Escape sequence buffer (8KB) minimizes `write()` syscalls for smooth performance.

---

## Termux Installation

Once the package is merged into `termux-packages`:

```bash
pkg install mini
```

Until then, build from source in Termux:

```bash
pkg install clang make
git clone https://github.com/HackerCompagnion7/mini.git
cd mini
make
cp mini $PREFIX/bin/
```

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines. We follow the **VAVR methodology** (Validate → Analyze → Verify → Repair) for all changes.

---

## License

MIT License — see [LICENSE](LICENSE).

Copyright (c) 2026 HackerCompagnion7
