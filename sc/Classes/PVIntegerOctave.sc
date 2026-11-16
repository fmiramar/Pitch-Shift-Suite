PVIntegerOctave : UGen {
    *ar { |in, windowSize=1169|
        ^this.multiNew('audio', in, windowSize)
    }
    checkInputs {
        if (inputs[0].rate != 'audio') { ^"input must be audio rate" };
        if (inputs[1].rate != 'scalar') { ^"windowSize must be scalar" };
        ^nil
    }
}
