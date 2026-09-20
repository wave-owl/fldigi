# fldigi: Notes for Users

## What has changed

fldigi has undergone a substantial internal cleanup to make the program
easier to build and maintain on current computers.

Most of these changes are behind the scenes. The intent was to modernize
the build system and remove support code for obsolete compilers,
libraries, and Windows environments without changing the familiar way
operators use fldigi on the air.

For most users, fldigi should continue to look and operate as it did
before these changes.

## Supported systems

The fldigi source continues to contain support for:

-   Linux
-   current Apple/macOS systems
-   OpenBSD
-   FreeBSD
-   NetBSD
-   64-bit Windows

The Windows version is now explicitly a 64-bit application. Very old
32-bit Windows installations are no longer part of the current build
target.

On Apple systems, the project is intended for current Apple development
tools and current Intel or Apple-silicon systems rather than old OS X
compiler environments.

## Windows installation

The current Windows installer is designed for 64-bit Windows and
installs fldigi in the normal 64-bit Program Files location.

Users should not need to make registry changes or choose special
compatibility settings. The installer handles the appropriate 64-bit
Windows locations automatically.

The old Cygwin environment is no longer supported or required for
fldigi.

## Linux and other Unix-like systems

Linux remains a primary fldigi platform.

The source can be built using the established Autotools build system,
and the project now also has a CMake build path. These are developer and
packager choices; an operator installing a packaged fldigi release
normally does not need to care which build system was used.

Support-related source code for OpenBSD, FreeBSD, and NetBSD has been
retained.

## FLTK

fldigi now requires FLTK 1.4 or newer when building from source.

This primarily affects people who compile fldigi themselves or maintain
distribution packages. A user installing a complete fldigi package
normally receives the required libraries through the package or
operating-system installation process.

The modernization work was tested with FLTK 1.4.5.

## flarq

flarq is no longer built or distributed as part of fldigi.

This does not remove fldigi's ARQ interface or prevent fldigi from
working with separately installed applications that use that interface.

Operators who use flarq should treat it as a separate application rather
than expecting a fldigi source or installer package to contain it.

The same general principle applies to other NBEMS applications: fldigi
can cooperate with companion programs without those programs being
physically bundled into fldigi.

## What has not changed

The modernization was intentionally conservative.

It was not a redesign of fldigi's modem operation, user interface,
logging, rig control, or normal operating workflow. Large portions of
the work involved removing old compiler and library compatibility code
that current systems no longer need.

Platform-specific code that is still required was retained.

Existing configuration and operating concepts therefore remain familiar
to experienced fldigi users.

## Why these changes were made

fldigi has been developed over many years and has supported several
generations of operating systems, compilers, and libraries.

Over time, compatibility code accumulates. Eventually some of it
supports environments that are no longer practical while making the
current program harder to understand, test, and maintain.

This modernization removes a number of those historical layers while
keeping a deliberately modest technical baseline. fldigi now requires a
C++11-capable compiler rather than jumping to a much newer C++ standard
simply for the sake of modernization.

The result should make future maintenance and building more predictable
without unnecessarily excluding current supported systems.

## Building from source

Users who build fldigi themselves should expect the following current
requirements:

-   a C++11-capable C++ compiler;
-   FLTK 1.4 or newer;
-   the normal fldigi development dependencies for the features being
    built;
-   a current native toolchain for the operating system.

Autotools remains supported.

CMake support is also available for native Linux builds and for the
maintained 64-bit Windows cross-build using MXE.

The exact dependency versions available on a particular Linux or BSD
distribution may vary, so builders should use the development packages
supplied for their system where practical.

## For Windows builders

The maintained cross-build target is 64-bit MinGW-w64 through MXE.

The Windows installer has also been updated for a fully 64-bit
installation model. Developers building old 32-bit or Cygwin
configurations should not expect those historical paths to remain
supported.

This change concerns the build environment; normal Windows operators do
not need MXE or MinGW installed in order to run a packaged fldigi
release.

## For long-time fldigi users

The most important point is simple: this work is primarily maintenance
of the machinery underneath fldigi.

The project has shed a number of accommodations for old compiler
generations, pre-1.4 FLTK interfaces, Cygwin, and bundled flarq
integration. Those changes reduce maintenance burden and make the
supported environments clearer.

They are not intended to change established fldigi operating practices.

If you have used fldigi for digital-mode operation, logging, rig
control, or interaction with other NBEMS applications, the basic
relationship between you and the program remains the same.

## Looking ahead

Additional source modernization may be considered in future development,
but the present work stops at a conservative and well-tested point.

Future changes should continue to favor reliability and
supported-platform portability over adopting newer language features
merely because they are available.
