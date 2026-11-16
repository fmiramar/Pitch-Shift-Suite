#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <stdexcept>

namespace pitch {
// A05: Thuillier Table 6 PS2 preset, Table 5 prototype/Figure 15 pole reflection.
// Fixed octave up. Complex filters are second order, not sixth order: qERB=6
// specifies one sixth of a critical bandwidth. See the A05 equation trace.
class ERBPhaseOctave {
public:
    ERBPhaseOctave(double rate,std::size_t maxBlock) : maxBlock_(maxBlock) {
        if (!std::isfinite(rate) || rate<22050 || rate>192000 || !maxBlock || maxBlock>4096)
            throw std::invalid_argument("ERBPhaseOctave configuration");
        for(int k=0;k<bands;++k) {
            double fc=228.7*(std::pow(10.,(5.+k*4./6.)/21.3)-1)/2;
            double bandwidth=(24.7+.108*2*fc)/12;
            double q=fc/(2*bandwidth); // Real prototype Q is half the complex Q.
            double t=std::tan(pi*fc/rate),den=q*t*t+t+q;
            double a1=2*q*(t*t-1)/den,a2=(q*t*t-t+q)/den;
            double discriminant=4*a2-a1*a1;
            if (!(discriminant>0 && a2>0 && a2<1))
                throw std::invalid_argument("ERBPhaseOctave unstable prototype");
            auto p=Complex(-a1/2,std::sqrt(discriminant)/2);
            auto z=std::polar(1.,-2*pi*fc/rate);
            filters_[k].pole=p;
            // Keep zeros at +1/-1; normalize magnitude at fc with positive real gain.
            filters_[k].gain=std::abs((1.-p*z)*(1.-p*z)/(1.-z*z));
        }
        reset();
    }
    void reset() noexcept {
        previous_=older_=0;
        for(auto& f:filters_)f.first=f.second=Complex{};
    }
    bool process(const float* in,float* out,std::size_t frames) noexcept {
        if(!in || !out || frames>maxBlock_) {
            if(out)std::fill_n(out,frames,0.f);return false;
        }
        bool finite=true;
        for(std::size_t j=0;j<frames;++j) {
            double input=std::isfinite(in[j]) ? in[j] : 0;
            double difference=input-older_;older_=previous_;previous_=input;
            double sum=0;
            for(auto& f:filters_) {
                // g(1-z^-2)/(1-p z^-1)^2, realized as two first-order sections.
                f.first=clean(f.pole*f.first+f.gain*difference);
                f.second=clean(f.pole*f.second+f.first);
                sum+=phaseDoubleReal(f.second);
            }
            // Real input has half its sinusoidal amplitude in the positive image.
            out[j]=float(2*sum);
            if(!std::isfinite(out[j])) {out[j]=0;finite=false;}
        }
        return finite;
    }
private:
    friend struct ERBPhaseOctaveProbe;
    using Complex=std::complex<double>;
    static constexpr double pi=3.1415926535897932384626433832795;
    static constexpr int bands=43;
    struct Filter {Complex pole{},first{},second{};double gain=0;};
    std::array<Filter,bands> filters_{};
    std::size_t maxBlock_;
    double previous_=0,older_=0;
    static Complex clean(Complex z) noexcept {
        // Far below float audio precision; prevent denormal tails after silence.
        return {std::abs(z.real())<1e-30 ? 0:z.real(),std::abs(z.imag())<1e-30 ? 0:z.imag()};
    }
    static double phaseDoubleReal(Complex z) noexcept {
        double magnitude=std::abs(z);
        return magnitude<1e-30 ? 0 : (z.real()/magnitude)*z.real()-(z.imag()/magnitude)*z.imag();
    }
};
} // namespace pitch
