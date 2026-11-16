#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <stdexcept>

namespace pitch {
// A12 experimental fixed octave down. The A05 bank is a controlled starting
// point, not a published octave-down preset. Nulls can hide an entire phase turn.
class SubbandPhaseHalf {
public:
    SubbandPhaseHalf(double rate,std::size_t maxBlock) : maxBlock_(maxBlock) {
        if(!std::isfinite(rate) || rate<22050 || rate>192000 || !maxBlock || maxBlock>4096)
            throw std::invalid_argument("SubbandPhaseHalf configuration");
        decay_=std::exp(-1/(.05*rate));fade_=1./std::max(1L,std::lround(.001*rate));
        for(int k=0;k<bands;++k) {
            double fc=228.7*(std::pow(10.,(5.+k*4./6.)/21.3)-1)/2;
            double bandwidth=(24.7+.108*2*fc)/12,q=fc/(2*bandwidth);
            double t=std::tan(pi*fc/rate),den=q*t*t+t+q;
            double a1=2*q*(t*t-1)/den,a2=(q*t*t-t+q)/den;
            double discriminant=4*a2-a1*a1;
            if(!(discriminant>0 && a2>0 && a2<1))throw std::invalid_argument("SubbandPhaseHalf prototype");
            auto p=Complex(-a1/2,std::sqrt(discriminant)/2),z=std::polar(1.,-tau*fc/rate);
            filters_[k].pole=p;filters_[k].gain=std::abs((1.-p*z)*(1.-p*z)/(1.-z*z));
            filters_[k].omega=tau*fc/rate;
        }
        reset();
    }
    void reset() noexcept {
        previous_=older_=0;
        for(auto& f:filters_) {f.first=f.second={};f.track=Tracker{};}
    }
    bool process(const float* in,float* out,std::size_t frames) noexcept {
        if(!in || !out || frames>maxBlock_) {
            if(out)std::fill_n(out,frames,0.f);return false;
        }
        bool finite=true;
        for(std::size_t j=0;j<frames;++j) {
            double input=std::isfinite(in[j]) ? in[j]:0;
            double difference=input-older_;older_=previous_;previous_=input;
            double sum=0;
            for(int k=0;k<bands;++k) {
                auto& f=filters_[k];f.first=clean(f.pole*f.first+f.gain*difference);
                f.second=clean(f.pole*f.second+f.first);
                // Initial sign only: closest root to the active lower neighbor.
                // Never flip a running band's sign to chase changing neighbors.
                const Tracker* anchor=k && filters_[k-1].track.reliable ? &filters_[k-1].track:nullptr;
                sum+=f.track.process(f.second,f.omega,decay_,fade_,anchor);
            }
            out[j]=float(2*sum);
            if(!std::isfinite(out[j])) {out[j]=0;finite=false;}
        }
        return finite;
    }
private:
    friend struct SubbandPhaseHalfProbe;
    using Complex=std::complex<double>;
    static constexpr double pi=3.1415926535897932384626433832795,tau=2*pi;
    static constexpr int bands=43;
    static double wrap(double x) noexcept {return x-tau*std::floor((x+pi)/tau);}
    struct Tracker {
        double half=0,previousPhase=0,peak=0,level=0;
        bool initialized=false,reliable=false;
        double process(Complex z,double omega,double decay,double fade,const Tracker* anchor=nullptr) noexcept {
            double magnitude=std::abs(z);peak=std::max(magnitude,peak*decay);
            if(peak<1e-30)peak=0; // Avoid a denormal envelope tail during prolonged silence.
            double floor=std::max(1e-12,peak*1e-4);
            bool good=magnitude>floor*(reliable ? 1:2);
            if(!good) {
                if(initialized)half=wrap(half+.5*omega); // Nominal-center prediction through unreliable input.
                reliable=false;level=0;return 0;
            }
            double phase=std::arg(z);
            if(!initialized) {
                half=.5*phase;
                if(anchor && std::cos(half-anchor->half)<0)half=wrap(half+pi);
                initialized=true;
            } else if(reliable) {
                half=wrap(half+.5*wrap(phase-previousPhase));
            } else {
                double prediction=wrap(half+.5*omega);
                half=wrap(prediction+.5*wrap(phase-2*prediction));
            }
            previousPhase=phase;reliable=true;level=std::min(1.,level+fade);
            return magnitude*std::cos(half)*level;
        }
    };
    struct Filter {Complex pole{},first{},second{};double gain=0,omega=0;Tracker track;};
    std::array<Filter,bands> filters_{};
    std::size_t maxBlock_;
    double previous_=0,older_=0,decay_=0,fade_=0;
    static Complex clean(Complex z) noexcept {
        return {std::abs(z.real())<1e-30 ? 0:z.real(),std::abs(z.imag())<1e-30 ? 0:z.imag()};
    }
};
} // namespace pitch
