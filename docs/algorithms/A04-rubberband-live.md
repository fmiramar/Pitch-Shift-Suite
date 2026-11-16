> Historical development/source trace. Current release licensing is specified in the root LICENSES.md; earlier open-license statements below describe development-time status. Private checkpoint links are omitted from this export.

# A04 — RubberBandLiveShift

**Evidence:** usable source. **Phase:** 2. **State:** minimal standalone/SC implementation verified on macOS arm64; checkpoint (development record retained separately).

**Source/license:** Rubber Band 4.0.0, `e4296ac80b1170018a110bc326fd0d45a0eb27d6`; [LiveShifter header](https://github.com/breakfastquay/rubberband/blob/e4296ac80b1170018a110bc326fd0d45a0eb27d6/rubberband/RubberBandLiveShifter.h). GPL-2.0-or-later/commercial alternative.

**Method/API trace:** dedicated `RubberBandLiveShifter` class with `shift()`, `setPitchScale()`, `getBlockSize()` and `getStartDelay()`. This is not an R2/R3 construction option. Fixed-size input/output blocks require a prepared host-block adapter; do not force SC's block size to match. Analysis bands and algorithm options remain upstream-specific.

**Ratios:** general positive pitch scale; finite project envelope and automation behavior must be established. Time stretching is not the exposed purpose of this API.

**CPU/latency expectation:** bounded fixed-block work is promising for scheduling but does not prove a deadline. Query initial-scale delay, add adapter delay, and verify impulses across host/internal block offsets. Avoid a hardcoded library block size or advertised latency. All numerical measurements pending.

**Required tests:** distinguish this class from both Stretcher engines; render the same ratio corpus independently. Test mismatched host block sizes, block-boundary pitch changes, channel linking, reset, silence, high-frequency aliasing, startup/tails and declared latency. Verify constructor/initialization outside callbacks, same-thread parameter changes, and no callback allocation/locking/logging. Follow license boundary (development record retained separately); source-level RT statements do not replace instrumentation.

**Delivered scope:** independent LiveShifter API, default short window, mono ratios 0.5–2 and queried-block adapter. Measured 48 kHz unity impulse delay 2624 samples; nominal query-plus-adapter estimate 2560 kept separate. Extended comparisons above remain deferred.
