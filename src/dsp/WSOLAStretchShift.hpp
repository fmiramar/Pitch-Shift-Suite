#pragma once
#include "dsp/OLAStretchShift.hpp"
namespace pitch {
// Same bounded grain/resampling implementation, with the explicit WSOLA search.
// Five milliseconds radius (at most 512 samples); added to causal lookahead.
class WSOLAStretchShift : public OLAStretchShift {
public:
    WSOLAStretchShift(double rate,std::size_t maxBlock,double seconds=.04,int overlap=4,int window=0)
        : OLAStretchShift(rate,maxBlock,seconds,overlap,window,
                          std::isfinite(rate) && rate>=44100 && rate<=96000 ? int(std::min(512.,std::round(rate*.005))) : 0) {}
};
}
