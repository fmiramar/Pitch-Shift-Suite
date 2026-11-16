#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <stdexcept>

namespace pitch {
// A06: Thuillier Table 6 SSM2. Same analysis bank as A05, different modulator.
// Per band: real(z[n] exp(i 2pi fc n/fs)); a tone f becomes f+fc, not always 2f.
class ERBSSBOctave {
public:
    ERBSSBOctave(double rate,std::size_t maxBlock) : maxBlock_(maxBlock) {
        if (!std::isfinite(rate) || rate<22050 || rate>192000 || !maxBlock || maxBlock>4096)
            throw std::invalid_argument("ERBSSBOctave configuration");
        for(int k=0;k<bands;++k) {
            // qC=8, qERB=12, qPS=1: identical fc and bandwidth to A05.
            double fc=228.7*(std::pow(10.,(5.+k*8./12.)/21.3)-1)/2;
            double bandwidth=(24.7+.108*2*fc)/12;
            double q=fc/(2*bandwidth);
            double t=std::tan(pi*fc/rate),den=q*t*t+t+q;
            double a1=2*q*(t*t-1)/den,a2=(q*t*t-t+q)/den;
            double discriminant=4*a2-a1*a1;
            if (!(discriminant>0 && a2>0 && a2<1))
                throw std::invalid_argument("ERBSSBOctave unstable prototype");
            auto p=Complex(-a1/2,std::sqrt(discriminant)/2);
            auto z=std::polar(1.,-2*pi*fc/rate);
            filters_[k].pole=p;
            filters_[k].gain=std::abs((1.-p*z)*(1.-p*z)/(1.-z*z));
            filters_[k].step=std::conj(z);
        }
        reset();
    }
    void reset() noexcept {
        previous_=older_=0;oscillatorTick_=0;
        for(auto& f:filters_) {f.first=f.second=Complex{};f.phase={1,0};}
    }
    bool process(const float* in,float* out,std::size_t frames) noexcept {
        if(!in || !out || frames>maxBlock_) {
            if(out)std::fill_n(out,frames,0.f);return false;
        }
        bool finite=true;
        for(std::size_t j=0;j<frames;++j) {
            double input=std::isfinite(in[j]) ? in[j] : 0;
            double difference=input-older_;older_=previous_;previous_=input;
            oscillatorTick_=(oscillatorTick_+1)&1023;
            double sum=0;
            for(auto& f:filters_) {
                f.first=clean(f.pole*f.first+f.gain*difference);
                f.second=clean(f.pole*f.second+f.first);
                sum+=(f.second*f.phase).real(); // Phase is 1 on sample zero.
                advance(f.phase,f.step,oscillatorTick_==0);
            }
            out[j]=float(2*sum); // Same real-input positive-image compensation as A05.
            if(!std::isfinite(out[j])) {out[j]=0;finite=false;}
        }
        return finite;
    }
private:
    friend struct ERBSSBOctaveProbe;
    using Complex=std::complex<double>;
    static constexpr double pi=3.1415926535897932384626433832795;
    static constexpr int bands=43;
    struct Filter {Complex pole{},first{},second{},step{},phase{1,0};double gain=0;};
    std::array<Filter,bands> filters_{};
    std::size_t maxBlock_;
    double previous_=0,older_=0;
    unsigned oscillatorTick_=0;
    static Complex clean(Complex z) noexcept {
        return {std::abs(z.real())<1e-30 ? 0:z.real(),std::abs(z.imag())<1e-30 ? 0:z.imag()};
    }
    static void advance(Complex& phase,Complex step,bool normalize) noexcept {
        phase*=step;
        if(normalize)phase/=std::abs(phase); // Bound radial drift; never reset phase at a block boundary.
    }
};
} // namespace pitch
