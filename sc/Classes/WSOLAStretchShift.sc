WSOLAStretchShift : UGen {
    *ar { |in, ratio=1, grainSeconds=0.04, overlaps=4, window=0|
        ^this.multiNew('audio', in, ratio, grainSeconds, overlaps, window)
    }
    checkInputs {
        if (inputs[0].rate != 'audio') { ^"input must be audio rate" };
        if (inputs[1].rate == 'audio') { ^"ratio must be scalar or control rate" };
        (2..4).do { |i|
            if (inputs[i].rate != 'scalar') { ^"grain configuration must be scalar" };
        };
        ^nil
    }
}
