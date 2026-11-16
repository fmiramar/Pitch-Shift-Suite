> Historical development/source trace. Current release licensing is specified in the root LICENSES.md; earlier open-license statements below describe development-time status. Private checkpoint links are omitted from this export.

# A03 — RubberBandR3Shift

**Evidence:** usable source. **Phase:** 2. **State:** minimal standalone/SC implementation verified on macOS arm64; checkpoint (development record retained separately).

**Source/license:** Rubber Band 4.0.0, `e4296ac80b1170018a110bc326fd0d45a0eb27d6`, [Stretcher API](https://github.com/breakfastquay/rubberband/blob/e4296ac80b1170018a110bc326fd0d45a0eb27d6/rubberband/RubberBandStretcher.h). GPL-2.0-or-later/commercial, isolated with A02/A04.

**Method/API trace:** select `OptionEngineFiner | OptionProcessRealTime` at construction. Use the same API-level streaming contract as A02, but a separate engine registration and results. Do not assume that R2 phase/transient options affect R3: inspect each option's engine applicability. Query `getEngineVersion()` to reject accidental fallback. Internal analysis resolution is engine-specific; no independent equation implementation is proposed.

**Ratios:** general positive pitch scale with duration ratio 1; project-supported interval awaits measurement. Link channel behavior explicitly rather than assuming SC multichannel expansion provides it.

**CPU/latency expectation:** R3 may have a different resolution/cost tradeoff from R2; quality and CPU rankings are test questions. Query padding/start-delay APIs with the chosen initial scale and account for FIFO/block-adapter delay. No project measurements exist.

**Required tests:** independent R3 offline and SC renders, fractional ratios and ±1/±2 octaves, finite silence/DC response, impulse timing, dense chords, noise/transients, reset and automation. Exercise maximum configured blocks and supported ratio bounds under allocation monitoring. Reuse bounded streaming infrastructure only where contracts agree; never reuse R2 latency measurements or quality conclusions as R3 evidence. Initialization, logging policy and license separation follow [A02](A02-rubberband-r2.md).

**Delivered scope:** fixed mono configuration, real-time time ratio 1, pitch 0.5–2, preferred startup pad/trim and 250 ms output reserve. Extended comparisons above remain deferred.
