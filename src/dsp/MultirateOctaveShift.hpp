#pragma once
#include "terrarium/BandShifter.hpp"
#include "terrarium/Multirate.hpp"
#include <algorithm>
#include <stdexcept>

namespace pitch {
// A13: adapted Terrarium phase-scaling variant. Fixed 48 kHz, selected voice:
// 0 = +12, 1 = -12, 2 = -24 semitones. No firmware shelves/dry mix.
class MultirateOctaveShift {
public:
    MultirateOctaveShift(double rate,std::size_t maxBlock,int voice=0) : maxBlock_(maxBlock),voice_(voice) {
        if(rate!=48000 || !maxBlock || maxBlock>4096 || voice<0 || voice>2)
            throw std::invalid_argument("MultirateOctaveShift requires 48000 Hz and voice 0, 1 or 2");
        for(int k=0;k<80;++k) {
            float f0=center(k-1),f1=center(k),f2=center(k+1),a=f2-f1,b=f1-f0;
            bands_[k]=terrarium::BandShifter(f1,8000,2.f*(a*b)/(a+b));
        }
        reset();
    }
    static float center(int k) noexcept {return float(480*std::pow(2.0f,.027f*k)-420);}
    void reset() noexcept {
        for(auto& band:bands_)band.reset();
        decimator_={};interpolator_={};input_.fill(0);output_.fill(0);position_=0;
    }
    bool process(const float* in,float* out,std::size_t frames) noexcept {
        if(!in || !out || frames>maxBlock_) {if(out)std::fill_n(out,frames,0.f);return false;}
        bool finite=true;
        for(std::size_t n=0;n<frames;++n) {
            float x=std::isfinite(in[n]) ? std::clamp(in[n],-1e6f,1e6f):0.f;
            input_[position_]=std::abs(x)<1e-15f ? 0:x;
            out[n]=output_[position_];
            if(!std::isfinite(out[n])) {out[n]=0;finite=false;}
            if(++position_==6) {
                float sample=decimator_(input_),sum=0;
                for(auto& band:bands_) {
                    band.update(sample);
                    sum+=voice_==0 ? band.up1() : voice_==1 ? band.down1() : band.down2();
                }
                output_=interpolator_(sum);position_=0;
            }
        }
        return finite;
    }
private:
    friend struct MultirateOctaveShiftProbe;
    std::size_t maxBlock_;int voice_,position_=0;
    std::array<terrarium::BandShifter,80> bands_{};
    terrarium::Decimator decimator_;
    terrarium::Interpolator interpolator_;
    std::array<float,6> input_{},output_{}; // Six-sample causal host adapter delay.
};
}
