# Source SBOM

Source repository inventory only; completeness is UNKNOWN. Syft inventories tracked source/package manifests. Gitlinks and declared vendored/model evidence are inspected at immutable pins. CMake declarations are conditional requirements, not resolved installations. This is NOT proof of operating-system packages, static-link closure, build-time resolution, or the contents of release binaries.

Generation: **success**

## Source identity

wave-owl/fldigi `e7c3ab3709ad62a7d91829364eb48a05fa325c0d`

- sbom:role: source-root
- sbom:git-commit: e7c3ab3709ad62a7d91829364eb48a05fa325c0d
- sbom:git-tree: a65b166a6fd9d69164b32a335945faf2feea019d
- sbom:source-repository: https://github.com/wave-owl/fldigi
- sbom:release-tags: ["v4.2.13"]
- sbom:license-evidence: [{"path": "COPYING", "sha256": "8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903"}]
- sbom:license-resolution: unresolved; manual review required

## Generation errors

None.

## Components and unresolved licenses

| Component | Version | Role | License |
| --- | --- | --- | --- |
| wave-owl/fldigi | e7c3ab3709ad62a7d91829364eb48a05fa325c0d | source-root | unresolved; manual review required |
| src/mbedtls | unresolved | declared-vendored-source | unresolved; manual review required |
| src/include/mbedtls | unresolved | declared-vendored-source | unresolved; manual review required |
| src/libtiniconv | unresolved | declared-vendored-source | unresolved; manual review required |
| src/xmlrpcpp | unresolved | declared-vendored-source | unresolved; manual review required |
| src/irrxml | unresolved | declared-vendored-source | unresolved; manual review required |
| src/include/dr_mp3.h | unresolved | declared-vendored-source | unresolved; manual review required |
| src/cmedia/hid.cxx | unresolved | declared-vendored-source | unresolved; manual review required |
| src/cmedia/hid_lin.cxx | unresolved | declared-vendored-source | unresolved; manual review required |
| src/cmedia/hid_mac.cxx | unresolved | declared-vendored-source | unresolved; manual review required |
| src/cmedia/hid_win.cxx | unresolved | declared-vendored-source | unresolved; manual review required |
| src/misc/locator.c | unresolved | declared-vendored-source | unresolved; manual review required |
| src/compat/regex.c | unresolved | declared-vendored-source | unresolved; manual review required |
| src/include/gfft.h | unresolved | declared-vendored-source | unresolved; manual review required |
| src/include/dsp.h | unresolved | declared-vendored-source | unresolved; manual review required |
| src/mt63/dsp.cxx | unresolved | declared-vendored-source | unresolved; manual review required |
| FLTK | unresolved | conditional-build-runtime-requirement | unresolved |
| libpng | unresolved | conditional-build-runtime-requirement | unresolved |
| libsamplerate | unresolved | conditional-build-runtime-requirement | unresolved |
| libsndfile | unresolved | conditional-build-runtime-requirement | unresolved |
| portaudio | unresolved | conditional-build-runtime-requirement | unresolved |
| hamlib | unresolved | conditional-build-runtime-requirement | unresolved |
| libflxmlrpc | unresolved | conditional-build-runtime-requirement | unresolved |
| libmbedtls | unresolved | conditional-build-runtime-requirement | unresolved |
| Boost | unresolved | conditional-build-runtime-requirement | unresolved |

License evidence paths/hashes, source manifests, model hashes and conditional
CMake declarations are recorded in SBOM.cdx.json. Declared upstream pins do not
prove vendored equivalence. No dependency installation or build was performed.
