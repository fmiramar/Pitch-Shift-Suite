LowLatencySpectralShift : UGen {
    *ar { |in, ratio=1|
        ^this.multiNew('audio', in, ratio)
    }
    checkInputs {
        if (inputs[0].rate != 'audio') { ^"input must be audio rate" };
        if (inputs[1].rate != 'scalar') { ^"ratio must be scalar" };
        ^nil
    }
}
