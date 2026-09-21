# Synopsis
ME7Sum is a tool written in C for checksum testing/correction of Bosch ME7.x firmware binary image dumps.

Supported ECU families:
- **Bosch ME7.1 / ME7.5** — VAG (Volkswagen, Audi, Seat, Skoda) — original support
- **Bosch ME7.2** — Porsche 986 Boxster / 996 Carrera (added in this release)

It is published under the BSD open source license, the most unrestrictive freeware license possible. No warranty implied or given.

The latest binary releases are always available [here](https://github.com/nyetwurk/ME7Sum/releases/latest).

# Running
To check image.bin:
```
ME7Check image.bin
me7sum image.bin
```

To output corrected checksums:
```
me7sum image.bin out.bin
```

**If you do not supply "out.bin", ME7Sum will only check "image.bin" for errors - it will not make any corrections**

Note that if me7sum cannot completely detect checksum/CRC locations correctly, it will not output a file!

**Always use me7sum on a original version of your bin first to make sure it is compatible!**

**Make sure to check all corrected bins with ME7Check.exe before flashing them!**

# Porsche ME7.2 Support (986 Boxster / 996 Carrera)

Porsche ME7.2 ECU ROMs are **auto-detected** by me7sum — no additional flags are required for stock factory binaries. The tool identifies these ROMs via the Bosch part-number block at offset `0x1FCBE`.

Checksums verified and corrected for Porsche ME7.2:
1. **ROMSYS Startup** — `word[0x8000] + word[0xFFFE]` stored `@0x8038`
2. **ROMSYS ParamPage** — `word[0x10000] + word[0x1FFFE]` stored `@0x18000`
3. **ROMSYS ProgramPages** — 8 KB page first+last words, range `0x0000-0xFFFF` + `0x20000-0x37FFF`, stored `@0x803C`
4. **Calibration CRC32** — standard CRC32 over `0x10000-0x1FC03`, stored `@0x1FC0E`
5. **Multipoint Block 1** — 2 descriptors `@0x17D26`
6. **Multipoint Block 2** — 32 descriptors `@0x1EAA6`

These offsets were verified empirically across **157 factory Porsche ME7.2 ECU binaries** (124 × 986 Boxster + 33 × 996 Carrera). No false positives were found on 85+ VAG ME7.x ROMs.

> **Note on tuner-modified Porsche files:** Some aftermarket tunes intentionally leave one or more checksums incorrect. Always verify a stock factory binary before working on modified files.

# Known Issues
**DO NOT USE ON TUNER MODIFIED BINARIES!**

Many tuners modify the CRC/Checksum algorithms to discourage modification of their tunes. ME7Sum most likely will not detect such modifications.

ME7Check may detect such modifications, but there is no way for it to be 100% sure.

Never use ME7Sum on a file that you your self did not write.

**Some files may require ME7Sum to be run on them iteratively, [see Issue 7](https://github.com/nyetwurk/ME7Sum/issues/7).**

If ME7Check fails on a ME7Sum fixed file after a single pass, please post or email me the file. You may be able to get all the checksums properly fixed by re-running ME7Sum on its own outputted file.

ME7Check should not fail on RSA corrected bins. If it does, please email the binary to me or post on Nefmoto.

DO NOT FLASH ANY BINS without a backup ECU or a way to restore a known good bin or you may be stranded!

# Building
See [BUILD.md](BUILD.md) for full build instructions and dependencies (Linux, Windows, Cygwin, macOS).

Quick start: under Unix or Cygwin, `make` should work (Debian: `libgmp-dev`; Cygwin: `libgmp-devel`). Under Windows MSVC/nmake, run `build clean` then `build`. Under macOS, `brew install gmp` then `make`.

# Contributing
Feel free to contribute to the project! See [RELEASE.md](RELEASE.md) for the release process and GitHub workflow behavior.

- **nyet's ME7Sum**: [Nefmoto](http://nefariousmotorsports.com/forum/index.php?topic=3347.0title=) | [GitHub](https://github.com/nyetwurk/ME7Sum/)
- **360trev's ME7Sum**: [Nefmoto](http://nefariousmotorsports.com/forum/index.php?topic=2993.0title=) | [GitHub](https://github.com/360trev/ME7Sum/)
