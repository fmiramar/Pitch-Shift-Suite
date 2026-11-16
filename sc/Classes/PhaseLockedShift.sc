PhaseLockedShift : UGen {
    *ar { |in, ratio=1, fftSize=2048, mode=1|
        ^this.multiNew('audio', in, ratio, fftSize, mode)
    }
    checkInputs {
        if (inputs[0].rate != 'audio') { ^"input must be audio rate" };
        if (inputs[1].rate == 'audio') { ^"ratio must be scalar or control rate" };
        (2..3).do { |i|
            if (inputs[i].rate != 'scalar') { ^"FFT size and mode must be scalar" };
        };
        ^nil
    }
}
