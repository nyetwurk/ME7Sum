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

- **Visual Studio Build Tools 2022** (or any VS 2017+ install) with the **Desktop development with C++** workload — free download from [Visual Studio](https://visualstudio.microsoft.com/downloads/)
- **nmake** — Included with the C++ build tools
- **Git** — For `GIT_VERSION` (git describe)

`build.cmd` locates MSVC via `vswhere` (any edition: Community, Professional, Enterprise, or Build Tools). Override auto-detection by setting `VSINSTALL` to the Visual Studio installation directory before running `build.cmd`.

The Windows build uses **bundled MPIR** (`mpir/mpir-2017.lib`) for big-integer math. No system GMP or MPIR installation is required. The library was built with the VS 2017 toolset but links with newer MSVC versions; rebuild MPIR only if linking fails after a major VS upgrade.

### Build

From a normal Command Prompt:

```cmd
build
```

Or explicitly:

```cmd
build clean
build
```

`build.cmd` sets up the VS environment and runs `nmake`. GitHub Actions uses the same script (`build.cmd` on `windows-latest`). Produces: `me7sum.exe`

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

## Tests and corpus

`make test` runs `me7sum` over the test images and fails on any `ABORT`, `WARNING` or `ERROR`. Reports go to the gitignored `testdata/reports/`. The images are:

- every image in the private [ecu-corpus](https://github.com/nyetlabs/ecu-corpus) submodule (`corpus/`) whose `corpus.tsv` row has `checksums` = `ok`
- the modified (non-OEM) images in `testdata/modified/`; `testdata/broken/` holds images ME7Sum can't handle and is not tested

With read access to `ecu-corpus`, fetch the submodule at its pinned commit with `make corpus`; `make corpus-bump` moves it to the corpus head (commit the change yourself). The submodule URL is HTTPS; to use SSH, run `git config --global url.git@github.com:.insteadOf https://github.com/`. Without access, `corpus/` stays empty and `make test` skips the corpus images. `XDFKIT_CORPUS` points the tests at another corpus checkout, and `XDFKIT_REQUIRE_CORPUS=1` (set in CI) makes a missing corpus a failure. See xdfkit's `docs/corpus.md` for the corpus specification.

`make bins` is temporary and will be removed: it fills a gitignored `bins/` with symlinks under the names the old committed `bins/` directory used (`scripts/bins-compat.tsv` maps them to corpus images), for workflows that still read `bins/`. New work should use `corpus/images/` instead.

---

## Platform-Specific Notes

| Platform | GMP/MPIR source       | Executable suffix     |
| -------- | --------------------- | --------------------- |
| Linux    | System `libgmp-dev`   | (none)                |
| macOS    | Homebrew `gmp`        | (none)                |
| Cygwin   | `libgmp-devel`        | `-cyg.exe`            |
| MinGW    | System `libgmp`       | `.exe`                |
| Windows  | Bundled `mpir`        | `.exe`                |

---

## Optional: Cross-compile for Windows (MinGW)

`vars.mk` supports MinGW when `gcc -dumpmachine` contains `mingw`. Uncomment and set:

```make
SYS := i686-w64-mingw32
```

Requires MinGW toolchain and GMP built for MinGW.
