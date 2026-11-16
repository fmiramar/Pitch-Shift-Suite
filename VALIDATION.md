# Pitch Shift Suite validation

## Original source-release checks — 2026-11-08

Verified on macOS arm64 with Apple Clang 21, CMake 4.4.4 and SuperCollider 3.14.1. Tests ran against this separate source package. The build and staged binaries are held outside it in the development workspace; no compiled artifact is in the source archive.

- Fresh all-engines Release configuration/build succeeded, with no compiler warning/error lines observed. Optional Rubber Band source was supplied from the existing pinned local checkout; the network FetchContent route was not rerun. Its six recorded source/license hashes match the pin. Matching local SC headers were supplied explicitly.
- All 14 native smoke tests passed in 42.42 seconds. These exercise the existing scoped pitch, stability and ordinary allocation checks; retained algorithm-quality failures remain documented.
- All 15 installed-layout SCDoc files parsed. All 45 actual help example bodies compiled and rendered through SC at 48 kHz/block 64, with finite non-silent output and automatic node cleanup.
- Full local installation produced 13 modules and 15 helps. Every component includes LICENSE, LICENSES.md and the complete MIT notices in NOTICES.md, in addition to its original component-specific notices.
- Vendored Signalsmith hashes match the pinned manifest. Copied DSP headers, SC C++ adapters, class files and native tests match the development source bytes. Stage-only edits concern packaging, help license text, the help-test count and extraction of existing OSC/WAV helper functions.
- Original source inventory excluded PDFs, recordings, derived audio, plugin binaries, JARs, images, private capture tools, hidden Git history and symlinks. No absolute developer home paths were found in the source files.
- At that checkpoint, all 323 pre-existing versionable development files were byte-identical. Papers, private captures, Phase 8 code, existing builds and Git history remain in the original workspace for later review.

No new sanitizer run, Windows/Linux runtime validation, listening panel, full real-time audit, patent search or legal clearance is claimed. This is a local source-release preparation checkpoint, not completion of the project's Phase 9 audit.

The release scope defers the Phase 8 fitted candidate, A15 source research, full corpus/benchmark tooling and historical workflow logs. This keeps the source package independent of restricted recordings and private research documents. Existing scientific limitations are retained in each help file and algorithm sheet.

## Guide and CC0 audio update — 2026-11-16

- Project renamed Pitch Shift Suite; selected release folder renamed PitchShiftSuite-source-0.1.0. UGen class names and DSP code remain unchanged.
- PitchShiftSuite guide parsed in SuperCollider 3.14.1; all nine actual code blocks compiled. The actual playback graph rendered all four CC0 clips through SpectralHybridShift, PhaseLockedShift and ERBPhaseOctave, plus four dry cases: 16/16 finite, audible, unclipped outputs with automatic stopping. Test adapter used a numeric default for the guide's mix control; the live guide uses the numeric wet argument at SynthDef construction. No live audio-device or buffer-lifecycle runtime check is claimed.
- Initial test script failed from nested setup wrapping and a nonnumeric SynthDef control default in the test adapter. Both were corrected; final test passed. These were test-adapter faults; no DSP change.
- Shared documentation component configured and installed locally through CMake. Installed guide and four WAV bytes match the source; original CC0 notices and source records are retained. Guitar is direct pickup; piano may retain room sound. Monophonic/polyphonic clips are assembled recorded-note patterns, not continuous performances.
- Historical 14 native tests/45 class examples were not rerun: DSP is unchanged and those results belong to the earlier checkpoint. Class-help/audio/build/doc updates are packaging changes. No new platform/RT/patent clearance claim.
- Original PDFs, private recordings, Phase 8 code and development history remain local. The earlier all-files-unchanged statement predates the explicitly requested README, CMake and help changes in this update.
