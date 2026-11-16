> Historical development/source trace. Current release licensing is specified in the root LICENSES.md; earlier open-license statements below describe development-time status. Private checkpoint links are omitted from this export.

# A01 — SpectralHybridShift

**Evidence:** usable source. **Phase:** 2. **State:** minimal standalone/SC implementation verified on macOS arm64; checkpoint (development record retained separately).

**Source and license:** Signalsmith Stretch 1.3.2, commit `88061fd4eda56a2353c5d65c74aa3f4ccf76f9da`; MIT. [Pinned header](https://github.com/Signalsmith-Audio/signalsmith-stretch/blob/88061fd4eda56a2353c5d65c74aa3f4ccf76f9da/signalsmith-stretch.h), [API notes](https://github.com/Signalsmith-Audio/signalsmith-stretch/blob/88061fd4eda56a2353c5d65c74aa3f4ccf76f9da/README.md). Linear 0.6.6 is a separate MIT dependency; see manifest (development record retained separately).

**Method/API trace:** wrap `signalsmith::stretch::SignalsmithStretch<float>` rather than reproduce its spectral phase reconstruction. Use `configure`/presets, `setTransposeFactor(r)`, and `process(input, frames, output, frames)` for duration-preserving transposition. Analysis bands and interpolation are upstream internals, not interchangeable with a common FFT-size parameter. No numbered-paper equivalence claimed.

**Ratios:** positive general ratios; project test envelope not established. Initially test {0.25, 0.5, 1, 1.5, 2, 4}, accepting only measured-safe configurations. Keep formant and tonality options engine-specific.

**CPU/latency expectation:** spectral block work; measure peaks as well as average cost. Query both `inputLatency()` and `outputLatency()`; compare their sum against impulses for equal input/output rates. Split computation adds an interval to output latency. No measured values yet.

**Implementation decisions/tests:** prepare all buffers and use distinct input/output storage. Construct with an explicit seed; verify reset and random-state repeatability. Test silence, impulses at every hop position, steady tones/chords, automation timing, formant settings, arbitrary callback partitions, and callback allocations. Defaults may be too latent for the intended use; presets are comparison cases, not validated live defaults. Source review does not establish RT safety or stereo coherence of the future adapter.

**Current delivered scope:** default preset, mono, portable FFT, seeded instances, ratios 0.5–2, preparation/destruction on SC async worker. Standalone 44.1/48/96 kHz and SC 48 kHz checks pass; unity impulse delay is 5760 samples at 48 kHz. Advanced preset/formant/automation/quality comparisons listed above remain deferred.
