> Historical development/source trace. Current release licensing is specified in the root LICENSES.md; earlier open-license statements below describe development-time status. Private checkpoint links are omitted from this export.

# A10 — OLAStretchShift

**Evidence:** published equations. **Phase:** 3. **State:** implemented as a diagnostic baseline; non-aligned pitch accuracy fails; checkpoint (development record retained separately).

**Source:** Driedger & Müller 2016, [TSM review](https://www.audiolabs-erlangen.de/content/resources/MIR/00_PCD_AudioLabs/2016_DriedgerMueller_TSMOverview_AppliedSciences_ePrint.pdf), §3.1 equations (3)–(5). Independent code planned; the paper is not a source-code license.

**Equation trace:** form windowed input frames at analysis positions and overlap-add at synthesis positions; normalize by the corresponding sum of shifted windows. Stretch by `α=H_s/H_a`. For a duration-preserving pitch ratio r, choose α=r and resample the stretched signal by reading at rate r (project composition of TSM and resampling).

**Bands/ratios:** no spectral analysis bands. General positive ratios in a tested finite range. Offer at least two explicitly defined windows, grain durations and overlap counts. Symmetric versus periodic window endpoints must be specified; verify finite sums numerically instead of assuming a named window guarantees COLA.

**CPU/latency expectation:** overlapping-grain work and resampling; memory proportional to grain/history duration. Delay depends on scheduler, grain length and resampler. No measurements or “zero latency” claim.

**Failure modes/test gate:** cyclic coloration, phase cancellation, grain repetition/skipping, transient duplication and aliasing. Test unity reconstruction under wraparound, fixed phases, ratios 0.5/2, multiple input levels, impulses across grain starts, abrupt/slow automation, silence and callback partition changes. Resampler antialiasing is especially necessary for speedup. Allocate all grains/history before processing. Compare SC PitchShift as an external control, not a claimed equivalent implementation.
