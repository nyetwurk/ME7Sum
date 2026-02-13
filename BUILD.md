# Build Instructions

This document summarizes build dependencies and procedures for ME7Sum on Linux, native Windows (MSVC), and Cygwin.

## Build System Overview

The project uses multiple build systems:

- **GNUmakefile** + **vars.mk** + **makefile.common** — Unix-like builds (Linux, macOS, Cygwin, MinGW)
- **makefile** — Native Windows with MSVC/nmake
- **inifile/makefile** — Builds `libini.a` (included by main build)

---

## Linux

### Dependencies

- **gcc** — C compiler
- **make** — Build tool
- **git** — For `GIT_VERSION` (git describe)
- **libgmp-dev** — GMP library (arbitrary-precision arithmetic for RSA)

On Debian/Ubuntu:

```bash
sudo apt install build-essential git libgmp-dev
```

### Build

```bash
make
make test   # optional
```

Produces: `me7sum`, `inifile/libini.a`

---

## Native Windows (MSVC)

### Dependencies

- **Visual Studio 2017** (or compatible) — `build.cmd` expects VS 2017 at the default path
- **nmake** — Included with Visual Studio
- **Git** — For `GIT_VERSION`

The Windows build uses **bundled MPIR** (`mpir/mpir-2017.lib`) for big-integer math. No system GMP or MPIR installation is required.

### Build

From a normal Command Prompt (or after running `vcvarsall.bat`):

```cmd
build
```

Or explicitly:

```cmd
build clean
build
```

`build.cmd` sets up the VS environment and runs `nmake`. Produces: `me7sum.exe`

---

## Cygwin

### Dependencies

- **gcc** (Cygwin)
- **make**
- **git**
- **libgmp-devel** — GMP development package (headers and library)

Install via Cygwin setup:

```bash
# Install gcc-core, make, git, libgmp-devel
```

### Build

```bash
make
make test   # optional
```

Produces: `me7sum-cyg.exe` (Cygwin-specific suffix per `vars.mk`)

---

## macOS

### Dependencies

- **Xcode Command Line Tools** (or full Xcode) — provides `gcc`/`clang`
- **Homebrew** — for GMP
- **gmp** — via Homebrew

```bash
brew install gmp
```

### Build

```bash
make
make test   # optional
```

---

## Platform-Specific Notes

| Platform   | GMP/MPIR source      | Executable suffix |
|-----------|----------------------|-------------------|
| Linux     | System `libgmp-dev`  | (none)            |
| macOS     | Homebrew `gmp`       | (none)            |
| Cygwin    | `libgmp-devel`       | `-cyg.exe`        |
| MinGW     | System `libgmp`      | `.exe`            |
| Windows   | Bundled `mpir`       | `.exe`            |

---

## Optional: Cross-compile for Windows (MinGW)

`vars.mk` supports MinGW when `gcc -dumpmachine` contains `mingw`. Uncomment and set:

```make
SYS := i686-w64-mingw32
```

Requires MinGW toolchain and GMP built for MinGW.
