> Historical development/source trace. Current release licensing is specified in the root LICENSES.md; earlier open-license statements below describe development-time status. Private checkpoint links are omitted from this export.

# A05 — ERBPhaseOctave

**Evidence:** published equations + numerical/audio measurements. **Phase:** 5. **State:** standalone/SC functional checkpoint; exact thesis reproduction and perceived quality remain open.

**Source:** [Thuillier 2016](https://aaltodoc.aalto.fi/bitstreams/ff9e52cf-fd79-45eb-b695-93038244ec0e/download), §5.1 Table 3; §5.2 Figure 15/Table 5 and equations (37)–(40); §6.4 Table 6. Verified local PDF inspected after browser 403. Original implementation; no author DSP code or Terrarium filters imported.

## Equation-to-code trace

The fixed Table 6 PS2 preset has 43 bands, `qERB=6`, `qC=4`, `zleft=5`, `BWERB=28`, `qPS=2`. For `k=0…42`:

```text
fc[k] = 228.7 (10^((5 + k·4/6)/21.3) − 1) / 2             (39)–(40)
df[k] = (24.7 + 0.108·2fc[k]) / 12                         (37)–(38)
q = fc / (2df)                      real prototype Q = half the complex Q (§5.2.1)
t = tan(πfc/fs), D = q t² + t + q
a1 = 2q(t²−1)/D, a2 = (q t²−t+q)/D                       Table 5
p = (−a1 + i sqrt(4a2−a1²))/2
H(z) = g (1−z⁻²) / (1−p z⁻¹)²                           Figure 15 reconstruction
```

Figure 15 retains zeros at ±1 and moves the lower conjugate pole onto the upper pole, giving a repeated positive-frequency pole. The complex filter is **second order**; `qERB=6` is a bandwidth parameter, not filter order. Its poles are strictly inside the unit circle. The thesis calls this minimum phase; its retained unit-circle zeros mean it is not strictly minimum phase under the stable-inverse definition.

Table 5's original numerator is `b0(1−z⁻²)`, `b0=t/D`. Pole relocation changes its gain. We choose **positive real** `g = |(1−p exp(−iωc))² / (1−exp(−2iωc))|`, giving unit magnitude at specified `fc`. No complex phase correction is added. This normalization is an explicit implementation choice consistent with the figure's near-0 dB peaks, not a recovered author coefficient file. Native code uses two first-order sections driven by `g(x[n]−x[n−2])`; the independent SciPy reference uses the expanded second-order denominator `[1,−2p,p²]`.

For each complex output `z`, Table 3 PS2 is `z²/|z|`. The real output uses `(Re(z)/|z|)Re(z)−(Im(z)/|z|)Im(z)`, with zero for `|z|<1e−30`. Synthesis is **twice** the sum of real PS2 outputs, compensating the half-amplitude positive-frequency component of a real sinusoid. This does not flatten overlapping-band gain. State components below `1e−30` are cleared to avoid denormal tails. No phase unwrapping, oversampling or additional output filter is used.

## API, measurements and limits

`ERBPhaseOctave.ar(in)` is fixed **2× / +12 semitones**. No octave down, arbitrary ratio or 4× claim. Constructor accepts 22.05–192 kHz and blocks up to 4096; audio tests cover 44.1/48/96 kHz, with coefficient stability also checked at the rate endpoints. The SC wrapper prepares/disposes the engine on a worker using the existing lifecycle; it outputs silence until attached.

Analysis centers span 81.976–3936.367 Hz; target centers span 163.952–7872.735 Hz. The bank has no single exact transport delay. At 44.1 kHz, center group delays span 4.60–90.03 ms; these are linear analysis-filter delays, not a claim about every nonlinear output transient. A 0.5 impulse peaks after 172 samples (3.90 ms); 99% of its output energy arrives by sample 3109 (70.50 ms), with a downward chirp and a longer weak tail.

Equal-response crossovers measure **−13.97 to −14.68 dB** at 44.1 kHz, versus the thesis's approximate −12 dB. The stated parameters are preserved; no undocumented adjustment was made to force that number. Negative-frequency leakage at each filter's own center is −66.65 to −81.33 dB; this is not a whole-band analytic-signal guarantee. Leakage from other bands and nonlinear mixing explain additional components in real-input results even though ideal complex-sine PS2 is exact.

Eighteen single-tone tests give the correct octave, with at least 99.29% coherent target energy. Gain is uneven: input amplitude 0.1 yields target amplitudes about 0.1061 at 82 Hz, 0.0731 at 110 Hz and 0.01137 at 440 Hz (44.1 kHz). In the 220/277/330 Hz chord, the doubled components carry about 83.2% of total energy; a 440/445 Hz pair produces 885 Hz and other intermodulation components. No clean-polyphony or perceptual roughness claim follows. The selected 18 kHz/48 kHz probe is attenuated by 60.74 dB in total output energy, not a general anti-aliasing guarantee.

Source: [DSP](../../src/dsp/ERBPhaseOctave.hpp), [SC adapter](../../src/sc/ERBPhaseOctave.cpp), independent reference (development record retained separately). Checkpoint and reproduction commands (development record retained separately), saved measurements (development record retained separately). General benchmarking, listening, alternative normalization/post-filtering, stereo and full RT/release review are deferred. A06/A12 remain separate future tickets.
