#pragma once
#include "signalsmith-linear/fft.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

namespace pitch {
// A07: Thuillier (2016), eqs. (27)–(31), Fig. 21. Fixed +12 semitones.
// Actual odd N-point DFT: 4096 is only Bluestein's convolution length.
class PVIntegerOctave {
public:
    PVIntegerOctave(double rate, std::size_t maxBlock, int windowSize=1169)
        : maxBlock_(maxBlock), n_(windowSize) {
        if (!std::isfinite(rate) || rate<8000 || rate>192000 || !maxBlock || maxBlock>4096 ||
            (n_!=1169 && n_!=1161)) throw std::invalid_argument("PVIntegerOctave configuration");
        center_=(n_-1)/2; extent_=(center_+radius)/2; delay_=center_+extent_;
        fft_.resize(convolutionSize);
        chirp_.resize(n_); kernel_.resize(convolutionSize); work_.resize(convolutionSize);
        frequency_.resize(convolutionSize); time_.resize(n_); spectrum_.resize(n_);
        window_.resize(n_); synthesis_.resize(n_);
        for (int j=0;j<n_;++j) {
            // Reduced square argument keeps phase accurate in the prepared chirp.
            double angle=-tau*(std::int64_t(j)*j%(2*n_))/(2*n_);
            chirp_[j]=std::polar(1.,angle);
            window_[j]=.5-.5*std::cos(tau*j/(n_-1)); // symmetric Hann, zero endpoints
            work_[j]=std::conj(chirp_[j]);
            if (j) work_[convolutionSize-j]=work_[j];
        }
        fft_.fft(work_.data(),kernel_.data());
        double total=0;
        for (int j=-radius;j<=radius;++j) {
            double w=.42+.5*std::cos(tau*j/(2*radius))+.08*std::cos(tau*j/radius);
            fir_[j+radius]=w*(j ? std::sin(tau*.24*j)/(tau*j/2) : .48);
            total+=fir_[j+radius];
        }
        for (auto& x:fir_) x/=total;
        // Calibrate the periodic full-overlap gain with the actual phase-squared
        // DC frame (its envelope differs slightly from an unchanged Hann).
        for (int j=0;j<n_;++j) time_[centeredIndex(j-center_)]={window_[j],0};
        transformFrame();
        for (int j=-extent_;j<=extent_;++j) gain_[modHop(j)]+=decimated(j);
        for (double x:gain_) if (!std::isfinite(x) || x<=0) throw std::runtime_error("PV2 overlap gain");
        reset();
    }
    std::size_t nominalDelaySamples() const noexcept { return delay_; }
    static constexpr int hopSamples() noexcept { return hop; }
    void reset() noexcept {
        input_.fill(0); output_.fill(0); clock_=0; pending_=0;
    }
    bool process(const float* in, float* out, std::size_t frames) noexcept {
        if (!in || !out || frames>maxBlock_) {
            if (out) std::fill_n(out,frames,0.f);
            return false;
        }
        bool finite=true;
        for (std::size_t i=0;i<frames;++i) {
            input_[std::size_t(clock_)&(ringSize-1)]=std::isfinite(in[i]) ? in[i] : 0;
            if (++pending_==hop) { renderFrame();pending_=0; }
            auto slot=std::size_t(clock_)&(ringSize-1);
            // Frame centers are (hop-1-center)+m*hop. Gain is indexed by local j.
            out[i]=float(output_[slot]/gain_[modHop(clock_-delay_+center_+1)]);
            output_[slot]=0;
            if (!std::isfinite(out[i])) { out[i]=0;finite=false; }
            ++clock_;
        }
        return finite;
    }
private:
    friend struct PVIntegerOctaveProbe;
    using Complex=std::complex<double>;
    static constexpr double tau=6.283185307179586476925286766559;
    static constexpr int hop=16, radius=32, convolutionSize=4096, ringSize=2048;
    std::size_t maxBlock_;
    int n_,center_=0,extent_=0,delay_=0,pending_=0;
    std::int64_t clock_=0;
    signalsmith::linear::SimpleFFT<double> fft_;
    std::vector<Complex> chirp_,kernel_,work_,frequency_,time_,spectrum_;
    std::vector<double> window_,synthesis_;
    std::array<double,2*radius+1> fir_{};
    std::array<double,hop> gain_{};
    std::array<double,ringSize> input_{},output_{};
    int centeredIndex(int n) const noexcept { return n<0 ? n+n_ : n; }
    static int modHop(std::int64_t n) noexcept { return int(std::uint64_t(n)&(hop-1)); }
    void dft(const Complex* input, Complex* output, bool inverse=false) noexcept {
        // exp(-i2πnk/N)=chirp[n]*chirp[k]*conj(chirp[n-k]).
        std::fill(work_.begin(),work_.end(),Complex{});
        for (int j=0;j<n_;++j) work_[j]=(inverse ? std::conj(input[j]) : input[j])*chirp_[j];
        fft_.fft(work_.data(),frequency_.data());
        for (int j=0;j<convolutionSize;++j) frequency_[j]*=kernel_[j];
        fft_.ifft(frequency_.data(),work_.data());
        for (int j=0;j<n_;++j) {
            auto x=work_[j]*chirp_[j]/double(convolutionSize);
            output[j]=inverse ? std::conj(x)/double(n_) : x;
        }
    }
    static Complex doublePhase(Complex x) noexcept {
        double magnitude=std::abs(x);
        return magnitude>0 ? x*(x/magnitude) : Complex{}; // Eq. (31), silence-safe.
    }
    void transformFrame() noexcept {
        dft(time_.data(),spectrum_.data());
        spectrum_[0]={std::abs(spectrum_[0].real()),0}; // Negative DC also doubles phase.
        for (int k=1;k<=center_;++k) {
            spectrum_[k]=doublePhase(spectrum_[k]);
            spectrum_[n_-k]=std::conj(spectrum_[k]); // Odd N has no singleton Nyquist bin.
        }
        dft(spectrum_.data(),time_.data(),true);
        for (int j=0;j<n_;++j) synthesis_[j]=time_[centeredIndex(j-center_)].real()*window_[j];
    }
    double decimated(int j) const noexcept {
        // Linear-phase anti-alias FIR AFTER synthesis windowing, then 2:1 sampling.
        // Outside the frame is zero, not circular audio; FIR tails are retained.
        double value=0;
        for (int k=-radius;k<=radius;++k) {
            int index=2*j-k+center_;
            if (index>=0 && index<n_) value+=fir_[k+radius]*synthesis_[index];
        }
        return value;
    }
    void renderFrame() noexcept {
        auto center=clock_-center_;
        for (int j=0;j<n_;++j)
            time_[centeredIndex(j-center_)]={input_[std::size_t(clock_-(n_-1)+j)&(ringSize-1)]*window_[j],0};
        transformFrame();
        // Virtual synthesis hop 2R followed by 2:1 decimation -> final hop R.
        for (int j=-extent_;j<=extent_;++j)
            output_[std::size_t(center+delay_+j)&(ringSize-1)]+=decimated(j);
    }
};
} // namespace pitch
