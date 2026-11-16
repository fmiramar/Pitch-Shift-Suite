> Historical development/source trace. Current release licensing is specified in the root LICENSES.md; earlier open-license statements below describe development-time status. Private checkpoint links are omitted from this export.

# A14 — SinusoidalResidualShift

**Evidence:** published equations + independent numerical/audio measurements. **Phase:** 6. **State:** experimental standalone/SC implementation; transient preservation and dense-polyphony quality remain open.

Serra & Smith (1990), *Spectral Modeling Synthesis*, Computer Music Journal14(4),12–24, [DOI](https://doi.org/10.2307/3680788), [author record](https://mtg.upf.edu/node/251), [full paper](https://www.audiolabs-erlangen.de/content/05_fau/professor/00_mueller/02_teaching/2024s-sarntal/02_group_SYNTH/1990_SerraS_SpectralModelingSynthesis_CMJ.pdf). Retrieved2026-11-06; SHA256 `5d4d508c4f9d5e05aa18755db9f29b32c3ea3035e83ff620dfd033cbef59ad50`. The old teaching URL used `2024s_sarntal`; the actual link uses `2024s-sarntal`. Institutional handle/API still timed out. Pertinent analysis/synthesis sections read; printed pages20–22 inspected visually. The paper uses unnumbered equations. No SMS tools/code imported; PDF remains an ignored research input.

## Source to implementation

| Source | Implemented interpretation |
|---|---|
| p14 model | Sinusoidal sum plus a separately synthesized noise component |
| pp15–17 STFT/peaks | Windowed FFT, local maxima, valley prominence, log-parabolic peak interpolation |
| pp17–19 continuation | Frequency guides; nearest-frequency conflict resolution; amplitude ramps at births/deaths |
| pp14,19–20 synthesis | Linear amplitudes/frequencies; integrate frequency into continuous oscillator phase |
| p21 residual | Absolute difference of original/deterministic magnitude spectra, with identical analysis settings; local-maximum envelope and linear interpolation |
| p22 stochastic synthesis | Fresh random phases, Hermitian spectrum, IFFT, Hann overlap-add |
| p23 transformation | Scale deterministic frequencies; residual spectrum stays at its source position |

**Phase discrepancy:** p20 prints `theta(m)=theta_previous+omega(m)*m`, despite specifying the frequency integral on pp14/20. For `omega(m)=w0+(w1-w0)m/H`, that literal expression doubles the frequency slope. We implement the integral `theta(m)=theta_previous+w0*m+(w1-w0)*m*m/(2H)`, with endpoint phase carried and wrapped. Both references verify its sample increments against midpoint frequency. This is an explicit mathematical correction, not a claim of literal reproduction.

## Fixed project preset and deviations

Mono,44.1/48 kHz only; ratio0.5–2 at scalar/control rate,5 ms smoothing, mean ratio per hop. `SinusoidalResidualShift.ar(input, ratio=1, residual=1)`, with scalar residual level0–1. Invalid ratio silences a block and pauses state; unsupported rate/initial settings leave the SC unit silent. Nonfinite input becomes zero, finite input is limited to±1e6 and magnitudes below1e−20 become zero. Exact in-place processing is supported.

Analysis:2048-sample periodic four-term Blackman–Harris window, coefficients0.35875/0.48829/0.14128/0.01168,8192-point FFT,256-sample hop. Padding refines peak interpolation; it does not improve window resolution. Peaks require40 Hz≤f≤0.48fs, magnitude≥max(1e−10,0.001×frame maximum), and12.04 dB prominence over both adjacent valleys. Retain at most64 strongest peaks. Peak frequency/amplitude use three log-magnitude samples, calibrated by twice the inverse window sum.

At most64 guides, with a fixed4096-pair workspace and sorted closest-pair assignment (distance,track index,peak index tie order). Maximum frame displacement=max(40 Hz,3% of old frequency), guide coefficient1, no sleeping. New tracks require separationfs/2048 and an unused slot; dying tracks retain their slot for their fade. Birth/death amplitudes ramp across one hop. Both source and shifted oscillators start at zero phase on birth; measured input phases are deliberately not retained. Frequency and amplitude interpolate per hop. Shifted amplitudes taper over0.45fs–0.49fs to prevent steady oscillator folding; this does not certify transient/modulation alias rejection.

The unshifted oscillator bank is reanalyzed after its full window exists. Residual is `abs(abs(X)-abs(D))`, including overestimated deterministic magnitudes as prescribed by the source. Envelope maxima use±16 analysis bins on a32-bin grid; linear interpolation supplies1025 positive-frequency values for a2048-point noise IFFT. A fixed xorshift32 seed `0x12345678` advances for every bin/frame, including muted residuals. DC/Nyquist are zeroed; conjugate symmetry supplies negative bins. A1024-sample Hann window overlaps at256 samples. No new random device/seed allocation occurs in processing.

Our additional energy correction compensates the upper-envelope inflation and expected independent-frame overlap power: `gain²=(P_residual/P_envelope)*(2048/8192)*2048/(sum(analysisWindow²)*1.5)`, using conjugate-weighted spectral powers before DC/Nyquist removal. It becomes zero for envelope power≤1e−30. This normalization, fixed seed, thresholds, bounded assignment and Nyquist fade are project choices, not parameters recovered from the paper.

The synthesis timeline is delayed2560 samples:58.05 ms at44.1 kHz,53.33 ms at48 kHz. Initial negative-time synthesis is discarded. This is scheduling alignment, **not waveform identity or a universal impulse/group delay**. No transient bypass or input-phase reconstruction exists: unity ratio is spectral resynthesis, not sample-exact reconstruction. Residual level0 isolates the oscillators,1 adds the stochastic component.

## Measured checkpoint and limits

Checkpoint (development record retained separately) and results/hashes (development record retained separately). Selected tone pitch, reference agreement, finite output, ordinary allocation, SC parity and lifecycle checks pass. An impulse is spread into a noise burst; dense mixtures lose/merge tracks and pure noise can create false sinusoidal tracks. These are documented failures of the stronger transient-preservation/source-independent decomposition objective. They are not accepted listening-quality results. Residual frequencies are not pitch-scaled, and untracked tonal energy can become unshifted noise.

Standalone DSP is original; the existing pinned MIT Signalsmith Linear FFT and notice are reused. SC preparation/deletion uses the established worker lifecycle. Emergency RT-pool disposal-enqueue failure still retains the engine until process exit; no new resolution is claimed. Full allocation/locking/deadline audit, stereo, other rates/platforms, formants, improved association/transients, listening comparison and release licensing remain deferred. A14 adds no generalized infrastructure.
