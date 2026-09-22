# fldigi Modernization Notes for Developers

## Purpose

This document describes the modernization work applied to the fldigi
source and build system. It is intended for developers who build,
maintain, package, or modify fldigi.

The work was deliberately conservative. Its purpose was not to redesign
fldigi or require the newest C++ language standard. Instead, it removes
obsolete build and compiler compatibility layers, makes the supported
build environments explicit, improves Windows 64-bit packaging, and
makes the Autotools and CMake builds easier to maintain.

The current source requires a C++11-capable compiler. Genuine
operating-system portability remains important.

## Supported platform model

Platform-specific code remains where it represents a real
operating-system difference. In particular, fldigi continues to retain
conditional support for:

-   Linux
-   Apple/macOS
-   OpenBSD
-   FreeBSD
-   NetBSD
-   WIN32

The modernization work does not treat these platform conditionals as
obsolete merely because they are conditional.

For Apple systems, the supported model is the current Apple
compiler/toolchain, targeting both Intel and Apple silicon.
Compatibility code whose only purpose was to support old OS X compiler
generations is no longer a design constraint.

For Windows, the maintained native build is 64-bit Windows using the
MinGW-w64/MXE toolchain. Obsolete Cygwin-specific paths have been
removed.

## C++ language requirement

fldigi now explicitly requires C++11.

The Autotools build makes the existing C++11 compiler check mandatory
rather than optional. The CMake build specifies C++11 as its required
C++ standard.

CMake retains compiler extensions rather than forcing strict ISO mode.
This preserves the GNU-style compilation behavior traditionally used by
the project.

This is intentionally a conservative baseline. The modernization did not
raise the project to C++17 or C++20 merely to replace working C++11
code.

## Removal of obsolete C++ compatibility code

With C++11 now required, several old compatibility layers were no longer
useful.

### TR1 bind

The old selection between `std::bind` and `std::tr1::bind` was removed.
fldigi now uses `<functional>` and `std::bind` directly.

The existing `qrbind::bind` interface was retained, minimizing changes
to callers.

The corresponding Autotools and CMake feature-detection machinery was
removed.

### TR1 and GNU hash containers

The source previously selected among standard C++11 hashing, TR1
hashing, and older GNU hash extensions depending on compiler
capabilities.

fldigi now uses `std::hash` and `std::unordered_map` directly.

The obsolete TR1 and GNU `hash_map` fallbacks, compiler-version checks,
configuration macros, and build-system feature tests were removed.

### Old GCC version accommodations

Compiler-version checks dating from GCC 3.x and GCC 4.x were removed
where the required functionality is guaranteed by the current compiler
baseline.

Examples include:

-   the old GCC-version test around `<cstdint>` in `FreqControl.cxx`;
-   GCC 4.1-era memory-barrier fallback logic;
-   GCC 3-era attribute checks;
-   GCC 4.3-era `hot` and `cold` attribute checks.

Useful GNU-compatible compiler handling remains. The goal was to remove
obsolete version archaeology, not to eliminate valid compiler-specific
facilities.

### `std::unary_function`

The old compiler-dependent `std::unary_function` inheritance in the
spotter hash/comparison helpers was removed. The callable objects are
now ordinary C++11 function objects.

## FLTK requirement and cleanup

fldigi now requires FLTK 1.4 or newer.

Both build systems enforce this requirement. Development and regression
testing during this work used FLTK 1.4.5.

The source no longer carries fldigi-owned compatibility paths for FLTK
versions before 1.4. This includes old constructor/API variations,
screen API compatibility, input API guards, and local FLTK
version-selection macros.

The project is not hard-pinned to FLTK 1.4.5; 1.4 is the supported API
baseline.

### FLUID generated files

The FLUID source files remain authoritative. The normal fldigi
maintenance command is:

``` sh
make flgen
```

During modernization, the FLUID-generated sources were regenerated with
FLUID 1.4.5. Generator-version-only changes were retained together so
the generated files match the current FLTK toolchain.

## flarq separation

flarq is no longer built or distributed as part of the fldigi
source/build product.

The modernization removed fldigi build variables, configuration options,
version metadata, source integration, and obsolete documentation
assumptions associated with the bundled flarq program.

This does **not** mean that fldigi's ARQ interface was removed. fldigi
can still interoperate with separately installed applications such as
flarq, flmsg, and flamp where those applications use fldigi's supported
interfaces.

Documentation that describes valid external interoperability was
retained. Historical references in changelog material were also retained
as historical records.

## Cygwin retirement

Obsolete Cygwin-specific code paths were removed.

This included Cygwin configuration branches, serial-port handling, and
the Cygwin-only `com_to_tty` conversion code.

The live native-Windows `tty_to_com` handling remains because it is used
by the MinGW Windows serial implementation.

## Windows 64-bit build and installer

The maintained Windows packaging path is now explicitly 64-bit.

The NSIS installer was updated accordingly:

-   installation requires 64-bit Windows;
-   the 64-bit Program Files directory is used;
-   installer and uninstaller registry access uses the 64-bit registry
    view;
-   the previous `InstallDirRegKey` behavior was replaced with explicit
    registry reading where necessary so the selected registry view is
    honored.

The CMake packaging path can produce the same localized NSIS installer
variants used by the established build.

## CMake build support

CMake support was added for both native Linux and MXE cross-compilation.

The CMake build obtains the fldigi version from the top-level `VERSION`
file, keeping version information centralized rather than maintaining
independent version strings.

A significant build-independence fix ensures that the CMake-generated
`config.h` takes precedence over a `src/config.h` left behind by an
Autotools configuration.

This matters when developers use the same source tree for multiple
out-of-tree builds. For example, an Autotools-configured Linux source
tree must not cause an MXE CMake build to inherit Linux feature
definitions such as PulseAudio support.

The generated CMake configuration include directory is therefore
intentionally ordered before the source configuration directory.

## Autotools and CMake parity

Autotools remains a supported build system. CMake was added as another
maintained path rather than as an immediate replacement.

Changes that affect feature requirements or compiler behavior should be
reflected in both systems when applicable. Examples include:

-   the C++11 requirement;
-   the FLTK 1.4 requirement;
-   removal of obsolete compiler/library feature tests;
-   centralized version handling;
-   platform feature configuration.

A successful build in only one build system should not automatically be
treated as sufficient validation for build-system changes.

## MXE cross-compilation

The CMake MXE build targets:

``` text
x86_64-w64-mingw32.static
```

The toolchain uses the MXE C and C++ compilers, `windres`, the MXE
target root for libraries/includes/packages, and the target-specific
`pkg-config`.

The important maintenance principle is that CMake should discover target
libraries from the MXE environment and must not accidentally consume
host Linux configuration or libraries.

## Hamlib

The Hamlib integration was updated for the current `get_conf` API used
by the supported Hamlib environment.

Hamlib remains an external dependency and should be treated as such;
modernization of fldigi should not depend on retaining obsolete Hamlib
API compatibility indefinitely when the supported dependency baseline
has moved forward.

## Source areas intentionally left alone

Not every old-looking construct is a modernization target.

Bundled or imported third-party/compatibility sources, including areas
such as the bundled regular-expression implementation and other external
libraries, were intentionally excluded from broad mechanical cleanup.

Likewise, genuine compiler/platform selections remain where they provide
required functionality. Examples include stack-trace implementation
differences and Windows compiler distinctions.

The objective is maintainability, not an empty compatibility grep.

## Build and regression practice

For source changes, build the relevant native and cross-compiled targets
before considering a modernization patch complete.

The modernization work was repeatedly validated with Linux and MXE
builds reaching successful completion.

For build-system changes, a clean configure/build is preferred so stale
generated files do not hide configuration problems.

Out-of-tree CMake builds are particularly useful for detecting
accidental dependencies on files generated into the source tree by
Autotools.

## Patch discipline

The modernization was developed as small, focused StGit patches rather
than a single large rewrite.

That approach is recommended for future work. Compatibility removal can
expose unrelated assumptions, and narrow patches make those effects much
easier to identify, test, review, and revert.

Examples of useful patch boundaries from this work include separate
changes for:

-   requiring C++11;
-   retiring TR1 bind;
-   retiring TR1/GNU hash support;
-   retiring old GCC accommodations;
-   retiring `std::unary_function`;
-   requiring FLTK 1.4;
-   removing individual FLTK compatibility areas;
-   retiring Cygwin;
-   adding CMake Linux and MXE builds;
-   modernizing individual NSIS x64 behaviors.

## Guidance for future modernization

Future modernization should follow the same conservative rule:

> Remove compatibility code when the supported platform/compiler/library
> baseline makes it unnecessary, but preserve code that represents a
> real supported platform distinction.

Before deleting a conditional, determine what it protects and whether
that condition can still occur on a supported system.

Prefer a source inventory followed by inspection over broad mechanical
replacement. Build after each coherent group of changes.

Potential future work may include additional C++ cleanup, dead-code
review, and further build-system simplification, but these should be
treated as separate campaigns rather than folded into the completed
compatibility work.

The AsciiDoc Python 3.12 warnings are not fldigi warnings; AsciiDoc
upstream fixed the table-separator escape handling by changing the
constructed literal to a raw string. This fix is not included in
some recent distributions, including Linux Mint 22.3.

## Current baseline

At the end of this modernization phase:

-   fldigi requires C++11;
-   fldigi requires FLTK 1.4 or newer;
-   Linux remains a primary native build environment;
-   current Apple toolchains remain supported by policy;
-   OpenBSD, FreeBSD, and NetBSD platform conditionals remain valid;
-   native Windows support is 64-bit;
-   MXE provides the maintained Windows cross-build path;
-   Cygwin compatibility has been retired;
-   TR1 and old GNU hash fallbacks have been retired;
-   fldigi-owned pre-FLTK-1.4 compatibility has been retired;
-   flarq has been separated from the fldigi build and distribution;
-   Autotools and CMake builds are both maintained;
-   CMake Linux and MXE builds are supported;
-   CMake/Autotools configuration files can coexist without the CMake
    build accidentally consuming the wrong generated `config.h`.

The result is still recognizably the fldigi codebase and build process,
but with substantially less historical compatibility machinery to
maintain.
