MultirateOctaveShift : UGen {
    *ar { |in, voice=0|
        ^this.multiNew('audio', in, voice)
    }
    checkInputs {
        if (inputs[0].rate != 'audio') { ^"input must be audio rate" };
        if (inputs[1].rate != 'scalar') { ^"voice must be scalar" };
        ^nil
    }
}
