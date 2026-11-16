# Optional Rubber Band module

Rubber Band 4.0.0, commit `e4296ac80b1170018a110bc326fd0d45a0eb27d6`,
is fetched only with `PITCH_BUILD_RUBBERBAND=ON`. Default builds and Signalsmith
do not link it. Source and COPYING remain available in the build dependency tree.

The selected single-file build uses its built-in BQ resampler and Apple vDSP FFT
on macOS (built-in FFT elsewhere); threading is disabled. No Speex, KissFFT or
external FFT/resampler package is selected. Source is unchanged.

Rubber Band is GPL-2.0-or-later; a commercial licence is a separately negotiated
alternative, not held or assumed here. Distributing binaries must satisfy the
chosen licence and SC adapter obligations. A separate optional module is a build
boundary, not a licensing exemption. Project contributions are GPL-3.0-or-later. See the root LICENSE,
LICENSES.md, NOTICES.md and the upstream COPYING.

The first R2/R3 adapters use mono real-time mode, time ratio 1, pitch consistency
mode and ratios 0.5–2. Output is buffered with a conservative 250 ms reserve;
this is added adapter delay, not a low-latency claim. Preparation and destruction
use SC's async worker; extreme RT-pool exhaustion has the same retained-instance
cleanup limitation as A01. Independent mono expansion does not link stereo phase.

R2 selects `OptionTransientsSmooth`; the flag has no effect on R3. R3's lazy FFT
state is exercised with non-silent input during preparation and then reset.
Upstream preferred startup padding and output trimming precede the causal output
reserve. At 48 kHz/block64, unity impulse peak delay measured 12000 samples for
R2 and 12032 for R3; do not substitute the library start-delay query for these
adapter measurements. Internal pitch changes may still change effective delay.
Invalid controls mute output; a FIFO fault mutes the instance until recreation.

`RubberBandLiveShift` independently uses `RubberBandLiveShifter::shift()`, with
queried-size input/output buffers. The host adapter adds one internal block;
there is no R2/R3 output reserve. The current library returns 512 frames per call.
Ratio is captured at the first sample of each internal block. Non-silent worker
warm-up followed by reset and silent pre-roll handles lazy FFT/resampler setup
and drains retained warm-up overlap. Worker startup time is not signal latency.
At 48 kHz/unity, the API-based total estimate is 2560 samples, while impulse peaks
measured 2624 across tested offsets/host blocks. Neither is a universal delay for
other ratios. Ratios 0.5–2, mono/default short window, no formant controls yet.
