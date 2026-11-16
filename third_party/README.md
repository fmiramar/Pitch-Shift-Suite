Signalsmith Stretch 1.3.2 (`88061fd4eda56a2353c5d65c74aa3f4ccf76f9da`)
and Linear 0.6.6 (`7bb1c8f36794d6980080ce817c91f8da8fe56a66`).
Selected headers are unmodified; each directory contains its upstream MIT license.
Only the portable C++ FFT backend is included. No upstream tools, test code,
submodules or optional platform backends are included.

Repositories: https://github.com/Signalsmith-Audio/signalsmith-stretch and
https://github.com/Signalsmith-Audio/linear . Exact file hashes are in
`signalsmith-sources.json`.

A13 adds the adapted MIT [Terrarium subset](terrarium-sources.json) under `terrarium/`. Copyright2024 Steven Schulteis; the notice is retained and installed with `MultirateOctaveShift`. FIR coefficients/offsets and phase-scaling rules are preserved, with explicit storage/numerical/API adaptations. q/gcem/Daisy are not runtime dependencies.

A16 adapts Nicolas Juillerat's MIT2018 OCEAN algorithm in the DSP header and numerical reference. Only the original notice is copied under `juillerat-ocean/`; [source hashes and adaptations](juillerat-ocean-sources.json) document the inspected Java files. The existing portable Signalsmith FFT replaces BooFFT; no Java runtime, framework or bundled JAR dependency is imported. Both notices are installed with `LowLatencySpectralShift`.
