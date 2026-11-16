> Historical development/source trace. Current release licensing is specified in the root LICENSES.md; earlier open-license statements below describe development-time status. Private checkpoint links are omitted from this export.

# A02 — RubberBandR2Shift

**Evidence:** usable source. **Phase:** 2. **State:** minimal standalone/SC implementation verified on macOS arm64; checkpoint (development record retained separately).

**Source/license:** Rubber Band 4.0.0, `e4296ac80b1170018a110bc326fd0d45a0eb27d6`. [Stretcher header](https://github.com/breakfastquay/rubberband/blob/e4296ac80b1170018a110bc326fd0d45a0eb27d6/rubberband/RubberBandStretcher.h). GPL-2.0-or-later/commercial alternative; separate optional module per LICENSES.md (development record retained separately).

**Method/API trace:** `RubberBandStretcher` with `OptionProcessRealTime | OptionEngineFaster`; explicitly choose single-thread processing for the initial callback adapter. Use time ratio 1 and pitch scale `r`. Feed bounded blocks to `process`, then drain `available`/`retrieve` through prepared FIFOs. No claim that one `process` call returns one host block. Band/window details belong to the R2 engine and selected options, not a new paper implementation.

**Ratios:** general positive pitch scale; expose only a tested finite range. R2 transient/phase/window/formant options must be documented individually. Unsupported combinations should fail preparation.

**CPU/latency expectation:** FFT/resampling and FIFO buffering; no measured values. Set initial parameters before `getPreferredStartPad()` and `getStartDelay()`. Pad/trim the offline reference, then measure causal live-adapter delay separately. Check startup, tail draining, and pitch-dependent behavior.

**Required tests:** R2 engine-version assertion, pitch/gain/delay tests, silence and finite output, reset, automation, block sizes 16/32/64/128/256 plus nonuniform chunks, and bounded FIFO occupancy. `setMaxProcessSize` belongs in preparation. Stress legal parameter extremes under an allocation monitor: upstream RT guarantees have exceptions. Disable callback logging, avoid offline `study()` in the realtime path, and retain license notices.

**Delivered scope:** fixed mono configuration, real-time time ratio 1, pitch 0.5–2, preferred startup pad/trim and 250 ms output reserve. Extended comparisons above remain deferred.
