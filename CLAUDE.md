# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

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

## Project Overview

Rockbox is an open-source replacement firmware for digital audio players (DAPs). Written in C (gnu99) with architecture-specific assembly, it supports 80+ hardware targets across ARM, MIPS, Coldfire (m68k), and hosted platforms (SDL/Android/Linux). Licensed under GPLv2.

## Custom Build Targets

This tree is a custom build for **iPod Classic 6G/7G** and **iPod Video 5G/5.5G**. Changes may diverge from upstream Rockbox to suit these targets. The two iPods share the same 320x240 LCD and most app-layer code, but have different SoCs, USB controllers, and board-level drivers:

- **iPod Classic (6G/7G):** S5L8702 SoC, DesignWare USB OTG, CS42L55 codec. Config: `ipod6g`. Full feature set including MFi digital audio, SSD power management.
- **iPod Video (5G/5.5G):** PP5022 SoC, ARC USB OTG, WM8758 codec. Config: `ipodvideo`. UI features (Cover Flow, dynamic colors, themes). MFi digital audio is ported but untested.

## Build Commands

Rockbox requires out-of-tree builds. Cross-compiler toolchains are built via `tools/rockboxdev.sh`.

**Environment note:** `brew` and `sudo` are not available in this session. If a build dependency is missing, ask the user to install it manually.

```bash
# Hardware builds (clean) — output in build-hw-<target>/
./build-hw.sh                # iPod Classic 6G (default)
./build-hw.sh 5g             # iPod Video 5G
./build-hw.sh ipod6g         # explicit target name
./build-hw.sh ipodvideo      # explicit target name

# Incremental rebuild
cd build-hw-ipod6g && make -j$(sysctl -n hw.ncpu) && make zip && ../bundle-theme.sh
cd build-hw-ipodvideo && make -j$(sysctl -n hw.ncpu) && make zip && ../bundle-theme.sh

# Non-interactive configure (reference)
../tools/configure --target=ipod6g --type=n     # 6G hardware
../tools/configure --target=ipodvideo --type=n  # 5G hardware

# Other make targets
make rocks                  # plugins only
make codecs                 # codecs only
make bin                    # binary only
make zip                    # create deployment zip (themeless -- see below)
make reconf                 # reconfigure after tools/configure changes
make clean / make veryclean
```

**Theme bundling — `make zip` is not enough.** `tools/buildzip.pl` is kept
byte-identical to upstream, so it knows nothing about this fork's theme. A zip
straight from `make zip` has **no Themify_2 and no first-boot `config.cfg`**.
Always follow it with `../bundle-theme.sh`, which injects the theme and prunes
the empty `classic_statusbar` dir upstream still creates. `./build-hw.sh` does
this for you; a bare `make zip` does not.

**Configure build types:** (N)ormal, (S)imulator, (B)ootloader, (A)dvanced, (C)heckWPS, (D)atabase tool, (W)arble codec tool

**Sanitizers:** `--with-address-sanitizer` and `--with-ubsan` flags to configure.

**Default CFLAGS:** `-W -Wall -Wextra -Wundef -Os -nostdlib -ffreestanding -Wstrict-prototypes -pipe -std=gnu99`

## Architecture

### Layer Structure

```
bootloader/    — Minimal boot code, loads main firmware
firmware/      — HAL, kernel, drivers, filesystem, low-level services
lib/           — Shared libraries (rbcodec, skin_parser, fixedpoint, tlsf)
apps/          — Application layer: UI, playback engine, codecs loader, plugins, i18n
```

### Target Tree

Hardware abstraction is organized hierarchically under `firmware/target/`:
```
firmware/target/<cpu_arch>/<soc>/<manufacturer>/<model>/
```
Each target has a config header at `firmware/export/config/<modelname>.h` defining all hardware capabilities via `#define` macros. The central `firmware/export/config.h` includes an auto-generated `autoconf.h` (from configure) that selects the correct target header.

### SOURCES Files (Build System)

Source file selection uses `SOURCES` files (not per-target Makefiles). These are preprocessed with the C preprocessor, using `#ifdef` conditionals based on target config defines. This is how a single build system handles all 80+ targets. Key SOURCES files: `firmware/SOURCES`, `apps/SOURCES`, `apps/plugins/SOURCES`.

### Plugin System

Plugins are dynamically loaded `.rock` files. The API is a large struct of function pointers defined in `apps/plugin.h`, versioned so plugins must match the core. Entry point: `enum plugin_status plugin_start(const void *parameter)`.

### Codec System

Audio codecs live in `lib/rbcodec/` and are loaded as `.codec` files with their own API struct (`codecs.h`). The codec framework includes DSP processing (EQ, crossfeed, replaygain). Supports MP3, FLAC, Vorbis, Opus, AAC, ALAC, WavPack, APE, WMA, and many more.

### Memory Management

- **buflib** — Rockbox's custom compacting, handle-based allocator (`firmware/buflib*`). Two backends: `buflib_mempool` (bare-metal) and `buflib_malloc` (hosted platforms).
- **core_alloc** — Core allocation interface built on buflib.
- **TLSF** — Used for hosted/application builds (`lib/tlsf/`).

### Platform Types

This fork builds bare-metal native firmware only (`PLATFORM_NATIVE`). The
simulator and its `uisimulator/` tree have been removed, and `apps/` no longer
carries `SIMULATOR` or `PLATFORM_HOSTED` conditionals. `tools/configure` still
offers `--type=s`, but it will not build.

### Threading

Native assembler threads (ARM) with cooperative multitasking.

## Code Style

- **C only** (gnu99). Assembly only when necessary for performance.
- **4-space indentation**, no tabs. 80-column line limit. Unix LF line endings. UTF-8.
- **Naming:** all lowercase for variables, functions, structs, enums. UPPER_CASE for preprocessor symbols and enum constants. No mixed case. No typedefs for structs.
- **Comments:** `/* C-style only */`. Use `#if 0` to comment out blocks. No `//` comments.
- **Function braces** on a new line. Otherwise follow existing file style.
- When editing existing code, follow the style already present in that file.

## Key Tools

- `tools/configure` — build configuration (~4900-line shell script)
- `tools/rockboxdev.sh` — cross-compiler toolchain builder
- `tools/genlang` — language file processor
- `tools/bmp2rb` — bitmap converter for Rockbox
- `tools/convbdf` — BDF font converter
- `tools/scramble` / `tools/descramble` — firmware file format tools
- `tools/buildzip.pl` — creates deployment ZIP
- `tools/voice.pl` — voice file generator (TTS)

## Release Workflow

To build, tag, and publish a release on GitHub:

```bash
# 1. Build hardware release zips
./build-hw.sh          # 6G
./build-hw.sh 5g       # 5G

# 2. Commit, tag, and push
git add <files>
git commit -m "vX.Y: description"
git tag vX.Y
git push origin master
git push origin vX.Y

# 3. Create GitHub release with both zips
gh release create vX.Y \
    build-hw-ipod6g/rockbox.zip#rockbox-ipod6g.zip \
    build-hw-ipodvideo/rockbox.zip#rockbox-ipodvideo-5g.zip \
    --repo nuxcodes/rockbox -t "vX.Y" \
    -F release-notes.md
# Add -p for prerelease/alpha/beta tags
```

Releases are published at `https://github.com/nuxcodes/rockpod/releases`.
