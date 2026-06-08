#!/usr/bin/env bash
#
# MINI — Lightweight terminal text editor installer
# https://github.com/HackerCompagnion7/mini
#
# Usage:
#   curl -s https://raw.githubusercontent.com/HackerCompagnion7/mini/main/install.sh | bash
#

set -e

# ---- Colors ----
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
RESET='\033[0m'

# ---- Helpers ----
info()  { printf "${CYAN}[MINI]${RESET} %s\n" "$1"; }
ok()    { printf "${GREEN}[MINI]${RESET} %s\n" "$1"; }
warn()  { printf "${YELLOW}[MINI]${RESET} %s\n" "$1"; }
error() { printf "${RED}[MINI]${RESET} %s\n" "$1" >&2; exit 1; }

# ---- Detect platform ----
detect_platform() {
    if [ -n "$TERMUX_VERSION" ] || [ -d "/data/data/com.termux" ]; then
        echo "termux"
    elif [ "$(uname)" = "Darwin" ]; then
        echo "macos"
    elif [ "$(uname)" = "Linux" ]; then
        echo "linux"
    else
        echo "unknown"
    fi
}

PLATFORM=$(detect_platform)
info "Platform detected: $PLATFORM"

# ---- Set install prefix ----
if [ "$PLATFORM" = "termux" ]; then
    PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
    BINDIR="$PREFIX/bin"
elif [ "$PLATFORM" = "macos" ]; then
    PREFIX="/usr/local"
    BINDIR="$PREFIX/bin"
else
    PREFIX="${PREFIX:-/usr/local}"
    BINDIR="$PREFIX/bin"
fi

# ---- Check for curl or wget ----
if command -v curl >/dev/null 2>&1; then
    FETCH="curl -sL"
elif command -v wget >/dev/null 2>&1; then
    FETCH="wget -qO-"
else
    error "Neither curl nor wget found. Please install one."
fi

# ---- Check for git ----
if ! command -v git >/dev/null 2>&1; then
    warn "git not found. Attempting to install..."

    case "$PLATFORM" in
        termux)
            pkg install -y git
            ;;
        linux)
            if command -v apt-get >/dev/null 2>&1; then
                sudo apt-get update -qq && sudo apt-get install -y -qq git
            elif command -v dnf >/dev/null 2>&1; then
                sudo dnf install -y git
            elif command -v pacman >/dev/null 2>&1; then
                sudo pacman -S --noconfirm git
            else
                error "Cannot install git automatically. Please install it manually."
            fi
            ;;
        macos)
            if command -v brew >/dev/null 2>&1; then
                brew install git
            else
                error "git not found and Homebrew not available. Install Xcode Command Line Tools: xcode-select --install"
            fi
            ;;
        *)
            error "Cannot install git on this platform. Please install it manually."
            ;;
    esac
fi

# ---- Check for C compiler ----
if ! command -v gcc >/dev/null 2>&1 && ! command -v cc >/dev/null 2>&1 && ! command -v clang >/dev/null 2>&1; then
    warn "C compiler not found. Attempting to install..."

    case "$PLATFORM" in
        termux)
            pkg install -y clang make
            ;;
        linux)
            if command -v apt-get >/dev/null 2>&1; then
                sudo apt-get update -qq && sudo apt-get install -y -qq gcc make
            elif command -v dnf >/dev/null 2>&1; then
                sudo dnf install -y gcc make
            elif command -v pacman >/dev/null 2>&1; then
                sudo pacman -S --noconfirm gcc make
            else
                error "Cannot install a C compiler automatically. Please install gcc and make."
            fi
            ;;
        macos)
            if command -v brew >/dev/null 2>&1; then
                brew install gcc make
            else
                error "C compiler not found. Install Xcode Command Line Tools: xcode-select --install"
            fi
            ;;
        *)
            error "Cannot install a C compiler on this platform."
            ;;
    esac
fi

# ---- Check for make ----
if ! command -v make >/dev/null 2>&1; then
    warn "make not found. Attempting to install..."

    case "$PLATFORM" in
        termux)
            pkg install -y make
            ;;
        linux)
            if command -v apt-get >/dev/null 2>&1; then
                sudo apt-get install -y -qq make
            elif command -v dnf >/dev/null 2>&1; then
                sudo dnf install -y make
            elif command -v pacman >/dev/null 2>&1; then
                sudo pacman -S --noconfirm make
            fi
            ;;
        macos)
            if command -v brew >/dev/null 2>&1; then
                brew install make
            fi
            ;;
    esac
fi

# ---- Create temp build directory ----
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

info "Downloading MINI source..."
cd "$TMPDIR"

git clone --depth 1 https://github.com/HackerCompagnion7/mini.git 2>/dev/null
cd mini

# ---- Build ----
info "Building MINI..."
make clean 2>/dev/null || true
make

# ---- Verify binary ----
if [ ! -f "./mini" ]; then
    error "Build failed — binary not found."
fi

BINARY_SIZE=$(wc -c < "./mini")
info "Binary size: ${BINARY_SIZE} bytes"

# ---- Install ----
info "Installing to $BINDIR/mini ..."

if [ "$PLATFORM" = "termux" ]; then
    # Termux: no sudo needed
    cp -f ./mini "$BINDIR/mini"
    chmod 755 "$BINDIR/mini"
else
    # Linux/macOS: may need sudo
    if [ -w "$BINDIR" ]; then
        cp -f ./mini "$BINDIR/mini"
        chmod 755 "$BINDIR/mini"
    else
        sudo cp -f ./mini "$BINDIR/mini"
        sudo chmod 755 "$BINDIR/mini"
    fi
fi

# ---- Verify install ----
if command -v mini >/dev/null 2>&1; then
    ok "MINI installed successfully!"
    echo ""
    printf "  ${BOLD}mini${RESET}          — launch editor (empty file)\n"
    printf "  ${BOLD}mini file.txt${RESET} — open a file\n"
    printf "  ${BOLD}mini --help${RESET}   — show help\n"
    echo ""
    printf "  ${CYAN}https://github.com/HackerCompagnion7/mini${RESET}\n"
else
    warn "Binary installed to $BINDIR/mini but not in PATH."
    warn "Add $BINDIR to your PATH or run: $BINDIR/mini"
fi
