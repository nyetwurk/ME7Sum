# Synopsis
This project is under BSD open source license. Its on the most unrestrictive freeware license possible. No warranty implied or given.

It is a tool written in C for management of Bosch ME7.1 firmware dumps.

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

**If you do not supply "out.bin", ME7Sum will only check "image.bin" for errors - it wll not make any corrections**

Note that if me7sum cannot completely detect checksum/CRC locations correctly, it will not output a file!

**Always use me7sum on a original version of your bin first to make sure it is compatible!**

**Make sure to check all corrected bins with ME7Check.exe before flashing them!**

# Known Issues
**DO NOT USE ON TUNER MODIFIED BINARIES!**

Many tuners modify the CRC/Checksum algorithms to discourage modification of their tunes. ME7Sum most likely will not detect such modifications.

ME7Check may detect such modifications, but there is no way for it to be 100% sure.

Never use ME7Sum on a file that you your self did not write.

**Some files may require ME7Sum to be run on them iteratively, [see Issue 7](https://github.com/nyetwurk/ME7Sum/issues/7).**

If ME7Check fails on a ME7Sum fixed file after a single pass, please post or email me the file. You may be able to get all the checksums properly fixed by re-running ME7Sum on its own outputted file.

ME7Check should not fail on RSA corrected bins. If it does, please email the binary to me or post on Nefmoto.

DO NOT FLASH ANY BINS without a backup ECU or a way to restore a known good bin or you may be stranded!

It should generally autodetect checksum/CRC blocks, but is known not to work on non VAG Motronic bins, eg:

ferrari360.bin

# Building
Under unix or cygwin, "make" should work. On debian you will need `libgmp-dev`

Under Windows MSVSS/nmake, type "build clean" then "build"

Under MacOS, type "brew install gmp" then "make"

# Contributing
Feel free to contribute to the project!

nyet's ME7Sum:
http://nefariousmotorsports.com/forum/index.php?topic=3347.0title=
https://github.com/nyetwurk/ME7Sum/

360trev's ME7Sum:
http://nefariousmotorsports.com/forum/index.php?topic=2993.0title=
https://github.com/360trev/ME7Sum/
