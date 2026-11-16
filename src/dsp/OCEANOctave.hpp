#pragma once
#include "signalsmith-linear/fft.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

namespace pitch {
// A08: thesis Table 7 (N=897, R=224), bin translation plus frame rotation.
// Fixed nominal octave up; off-bin mistuning/transient duplication are retained.
class OCEANOctave {
public:
    OCEANOctave(double rate,std::size_t maxBlock) : maxBlock_(maxBlock) {
        if (!std::isfinite(rate) || rate<8000 || rate>192000 || !maxBlock || maxBlock>4096)
            throw std::invalid_argument("OCEANOctave configuration");
        fft_.resize(convolutionSize);
        chirp_.resize(n);kernel_.resize(convolutionSize);work_.resize(convolutionSize);
        frequency_.resize(convolutionSize);time_.resize(n);spectrum_.resize(n);shifted_.resize(n);
        for (int j=0;j<n;++j) {
            chirp_[j]=std::polar(1.,-tau*(std::int64_t(j)*j%(2*n))/(2*n));
            work_[j]=std::conj(chirp_[j]);if(j)work_[convolutionSize-j]=work_[j];
            roots_[j]=std::polar(1.,tau*j/n);
            window_[j]=.5-.5*std::cos(tau*j/(n-1));
        }
        fft_.fft(work_.data(),kernel_.data());
        reset();
    }
    static constexpr int windowSamples() noexcept { return n; }
    static constexpr int hopSamples() noexcept { return hop; }
    // Reference WOLA alignment, NOT a constant onset/peak delay for this algorithm.
    static constexpr int alignmentDelaySamples() noexcept { return n-1; }
    void reset() noexcept { input_.fill(0);output_.fill(0);clock_=0;pending_=0;rotation_=1; }
    bool process(const float* in,float* out,std::size_t frames) noexcept {
        if (!in || !out || frames>maxBlock_) {
            if(out)std::fill_n(out,frames,0.f);return false;
        }
        bool finite=true;
        for(std::size_t i=0;i<frames;++i) {
            auto slot=std::size_t(clock_)&(ringSize-1);
            input_[slot]=std::isfinite(in[i]) ? in[i] : 0;
            if(!pending_) { renderFrame();pending_=hop; }
            --pending_;
            out[i]=float(output_[slot]/1.5); // Exact full symmetric-Hann WOLA weight.
            output_[slot]=0;
            if(!std::isfinite(out[i])) { out[i]=0;finite=false; }
            ++clock_;
        }
        return finite;
    }
private:
    friend struct OCEANOctaveProbe;
    using Complex=std::complex<double>;
    static constexpr double tau=6.283185307179586476925286766559;
    static constexpr int n=897,hop=224,convolutionSize=2048,ringSize=2048;
    std::size_t maxBlock_;
    std::int64_t clock_=0;
    int pending_=0,rotation_=1;
    signalsmith::linear::SimpleFFT<double> fft_;
    std::vector<Complex> chirp_,kernel_,work_,frequency_,time_,spectrum_,shifted_;
    std::array<Complex,n> roots_{};
    std::array<double,n> window_{};
    std::array<double,ringSize> input_{},output_{};
    void dft(const Complex* input,Complex* output,bool inverse=false) noexcept {
        // Bluestein evaluates the actual 897-point DFT; no audio zero-padding preset.
        std::fill(work_.begin(),work_.end(),Complex{});
        for(int j=0;j<n;++j)work_[j]=(inverse ? std::conj(input[j]) : input[j])*chirp_[j];
        fft_.fft(work_.data(),frequency_.data());
        for(int j=0;j<convolutionSize;++j)frequency_[j]*=kernel_[j];
        fft_.ifft(frequency_.data(),work_.data());
        for(int j=0;j<n;++j) {
            auto x=work_[j]*chirp_[j]/double(convolutionSize);
            output[j]=inverse ? std::conj(x)/double(n) : x;
        }
    }
    void translate() noexcept {
        std::fill(shifted_.begin(),shifted_.end(),Complex{});
        shifted_[0]={spectrum_[0].real(),0};
        for(int j=1;j<=224;++j) {
            // Forward DFT uses exp(-iΩk): phase +Ωj*a is needed at destination 2j.
            // Conjugating the spectral convention gives thesis Eq. (17)'s minus
            // sign. See author fixSpectrum note and the explicit comparison test.
            auto value=spectrum_[j]*roots_[(j*rotation_)%n];
            shifted_[2*j]=value;
            shifted_[n-2*j]=std::conj(value);
        }
        // Bins above original quarter-rate are discarded, never wrapped/aliased.
        // Odd N has no singleton Nyquist bin. Positive odd destination bins stay zero.
    }
    void renderFrame() noexcept {
        // Frame starts a=clock-(N-1)=mR, initially m=-4. Its samples run forward.
        for(int j=0;j<n;++j)
            time_[j]={input_[std::size_t(clock_-(n-1)+j)&(ringSize-1)]*window_[j],0};
        dft(time_.data(),spectrum_.data());translate();dft(shifted_.data(),time_.data(),true);
        for(int j=0;j<n;++j)
            output_[std::size_t(clock_+j)&(ringSize-1)]+=time_[j].real()*window_[j];
        rotation_=(rotation_+hop)%n; // Exact a mod N; 897 frames, no floating drift.
    }
};
} // namespace pitch
