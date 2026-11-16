SpectralHybridShift : UGen {
    *ar { |in, ratio=1|
        ^this.multiNew('audio', in, ratio)
    }
    checkInputs {
        if (inputs.at(0).rate != 'audio') { ^"input must be audio rate" };
        if (inputs.at(1).rate == 'audio') { ^"ratio must be scalar or control rate" };
        ^nil
    }
}
