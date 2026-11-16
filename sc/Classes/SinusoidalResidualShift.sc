SinusoidalResidualShift : UGen {
    *ar { |in, ratio=1, residual=1|
        ^this.multiNew('audio', in, ratio, residual)
    }
    checkInputs {
        if (inputs[0].rate != 'audio') { ^"input must be audio rate" };
        if (inputs[1].rate == 'audio') { ^"ratio must be scalar or control rate" };
        if (inputs[2].rate != 'scalar') { ^"residual level must be scalar" };
        ^nil
    }
}
