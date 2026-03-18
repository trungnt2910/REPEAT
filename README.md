# REPEAT

[![Discord Invite][2]][1]

**R**epackaging **E**LF to **PE** with **A**ccuracy and **T**ransparency.

<!--
    Oh won't you talk to me,
    If your WORDs are bittersweet,
    Play my broken STRINGs on repeat, on REPEAT...
-->

## Overview

REPEAT converts an ELF shared object into an assembly file.

The output can be compiled by Clang to produce a COFF object, which can then be linked with other
Windows code into a PE executable.

## Usage

```
OVERVIEW: REPEAT ELF repackager

USAGE: repeat [options] <input_elf>

OPTIONS:
  -h, --help    Display available options
  -o <file>     Write output to <file>
```

## Features

REPEAT can translate:
- ELF `PT_LOAD` segments into sections.
    + This allows the embedded ELF to have the exact same memory layout in the Windows process.
    + i.e. _Accuracy_.
- DWARF debug information into CodeView directives.
    + This helps Windows tools to analyze and debug through the embedded ELF binary.
    + i.e. _Transparency_.
- ELF relocations into assembly offsets.
- ELF import/exports into assembly symbols.

## Limitations

REPEAT cannot:
- Magically convert ELF/Linux binaries into Windows executables.
- Handle Windows/UNIX ABI differences.
    + Consumers of converted objects must ensure ABI compatibility, e.g. marking the import as
    `[[gnu::sysv_abi]]`.
- Work with non-LLVM toolchains.
    + The output can be compiled by Clang to produce a MSVC-compatible object.

## Use Cases

REPEAT mainly targets Windows kernel-mode drivers, where dynamically loading executable ELF code may
be challenging due to
[Memory Integrity](https://learn.microsoft.com/en-us/windows-hardware/test/hlk/testref/driver-compatibility-with-device-guard#how-to-build-compatible-drivers) (HVCI).

This tool should also work for user-mode Windows code for a unified ELF/PE debugging experience on
WinDbg.

## Community

This repo is a part of [Project Reality][1].

Need help using this project? Join me on [Discord][1], and let's find a solution together.

[1]: https://reality.trungnt2910.com/discord/repeat
[2]: https://img.shields.io/discord/1185622479436251227?logo=discord&logoColor=white&label=Discord&labelColor=%235865F2
