OCEANOctave : UGen {
    *ar { |in| ^this.multiNew('audio', in) }
    checkInputs {
        if (inputs[0].rate != 'audio') { ^"input must be audio rate" };
        ^nil
    }
}
