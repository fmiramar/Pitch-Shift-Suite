> Historical development/source trace. Current release licensing is specified in the root LICENSES.md; earlier open-license statements below describe development-time status. Private checkpoint links are omitted from this export.

# A11 — WSOLAStretchShift

**Evidence:** published equations. **Phase:** 3. **State:** functional standalone/SC implementation; selected tone checks pass; checkpoint (development record retained separately).

**Sources:** [Driedger–Müller §4.1](https://www.audiolabs-erlangen.de/content/resources/MIR/00_PCD_AudioLabs/2016_DriedgerMueller_TSMOverview_AppliedSciences_ePrint.pdf), equations (6)–(11); [Roelands–Verhelst, Eurospeech 1993](https://www.isca-archive.org/eurospeech_1993/roelands93_eurospeech.pdf), §§2–4, equations (7)–(13). All four Eurospeech pages visually inspected. The separately cited ICASSP paper timed out and was not read.

**Equation trace:** around each predicted analysis position, choose bounded displacement `δ` maximizing correlation with the natural continuation of the preceding selected frame. Window/normalize and overlap-add at the synthesis hop. Compose stretch and resampling as in A10. A normalized metric, tie-breaking and silence fallback must be specified as implementation choices, not mislabeled verbatim paper equations.

**Bands/ratios:** time-domain frames; no spectral bands. General positive r in a measured finite range. Stereo-linked mode shares alignment decisions.

**CPU/latency expectation:** naive search is O(S·L) per grain for S candidates and L correlation samples, plus resampling (project estimate). Search radius and lookahead affect buffering. Bound maximum search work; measure worst-case callback cost. No current measurements.

**Required tests:** ratio-1 waveform reconstruction, tuned sines at 0.5 and 2, deterministic search ties, silence/DC, abrupt onset, repeated transients, buffer wrap and automation. Dense chords can contain incompatible periodicities; evaluate warble and transient repetition instead of assuming speech results transfer to music. Dependencies: Phase 1 harness, A10-style scheduler/normalization and validated resampler. Preserve independent algorithm identity and source attribution.
