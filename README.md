# Pitch Shift Suite

15 independent pitch-shifting UGens for SuperCollider, covering spectral, time-domain, filterbank and multirate techniques. Includes an SCDoc guide and CC0 piano/guitar listening examples.

This is research software. Several engines are experimental or diagnostic; supported ratios, latency and audio quality differ. See each UGen’s help before use.

## Engines

| Technique | UGens |
|---|---|
| Signalsmith spectral processing | `SpectralHybridShift` |
| Rubber Band (optional) | `RubberBandR2Shift`, `RubberBandR3Shift`, `RubberBandLiveShift` |
| Time-domain overlap/add | `OLAStretchShift`, `WSOLAStretchShift` |
| Phase vocoders | `PhaseLockedShift`, `PVIntegerOctave`, `OCEANOctave`, `LowLatencySpectralShift` |
| Subband and multirate octaves | `ERBPhaseOctave`, `ERBSSBOctave`, `SubbandPhaseHalf`, `MultirateOctaveShift` |
| Sinusoidal/residual processing | `SinusoidalResidualShift` |

Octave-only engines do not support arbitrary pitch ratios. Some methods retain mistuning, transient smearing or incomplete polyphonic tracking. Stereo expansion uses independent mono processors. Full realtime and cross-platform validation remains open.

## Build and install

Requires CMake 3.21+, a C++17 compiler and a [SuperCollider source checkout](https://github.com/supercollider/supercollider) matching your server. Tested locally on macOS Apple Silicon with SuperCollider 3.14.1.

```sh
cmake --preset plugins -DSC_SOURCE_DIR=/path/to/supercollider
cmake --build --preset plugins --parallel 2
ctest --preset plugins
cmake --install build-plugins --prefix /path/to/stage
```

For all 15 engines, use `plugins-rubberband` in the first three commands and install from `build-plugins-rubberband`. This downloads the pinned GPL Rubber Band dependency.

Copy the staged component folders, including `PitchShiftSuite`, into SuperCollider’s user Extensions folder. Find that folder by evaluating `Platform.userExtensionDir`. Recompile the class library, then search Help for **Pitch Shift Suite**.

This publication contains source. A macOS Intel binary release is deferred.

## Examples and documentation

The [suite guide](sc/HelpSource/Guides/PitchShiftSuite.schelp) provides engine selection, recorded piano/guitar examples and dry/wet controls. The four bundled CC0 clips are monophonic sequences and polyphonic chords assembled from recorded notes. Guitar uses direct pickup; piano may retain natural room sound. [Audio provenance and licenses](sc/HelpSource/Audio/PitchShiftSuite/README.md) are included.

See [algorithm notes](docs/algorithms), [validation records](VALIDATION.md) and the [file inventory](RELEASE_MANIFEST.json) for implementation details and tested limits. Existing scientific papers and proprietary reference captures are not bundled.

## License

Project contributions: **GPL-3.0-or-later**. Third-party code retains its MIT/GPL terms, and the demonstration audio remains CC0-1.0. See [LICENSE](LICENSE), [license scope](LICENSES.md), [notices](NOTICES.md) and [distribution requirements](DISTRIBUTION.md).
