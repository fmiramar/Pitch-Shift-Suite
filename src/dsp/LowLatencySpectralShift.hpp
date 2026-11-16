#pragma once
// A16: adaptation of Nicolas Juillerat's PitchShiftOcean.java (MIT, 2018).
// See third_party/juillerat-ocean/LICENSE.txt and docs/algorithms/A16-low-latency-spectral.md.
#include "signalsmith-linear/fft.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <stdexcept>
#include <vector>

namespace pitch {
class LowLatencySpectralShift {
public:
    static constexpr int n=512, padding=4, synthesis=n*padding, overlap=8, hop=n/overlap;
    static constexpr int cycle=padding*overlap, bins=n/2+1;
    static constexpr double tau=6.283185307179586476925286766559;
    LowLatencySpectralShift(double rate,std::size_t maxBlock,float ratio=1) : maxBlock_(maxBlock),ratio_(ratio) {
        if ((rate!=44100 && rate!=48000) || !maxBlock || maxBlock>4096 ||
            !std::isfinite(ratio) || ratio<.5f || ratio>2.f)
            throw std::invalid_argument("LowLatencySpectralShift configuration");
        analysisFFT_.resize(n);synthesisFFT_.resize(synthesis);
        time_.resize(n);spectrum_.resize(n);shifted_.resize(synthesis);inverse_.resize(synthesis);
        for(int j=0;j<n;++j)window_[j]=.5-.5*std::cos(tau*j/n); // Periodic Hann, author Windows.java.
        for(int p=0;p<cycle;++p)roots_[p]=std::polar(1.,tau*p/cycle);
        for(int a=1;a<bins;++a) {
            destination_[a]=int(std::floor(padding*ratio_*a+.5));
            rotation_[a]=mod(destination_[a]-padding*a);
        }
        // Source getDemodulationWindow; fixed ratio permits one finite precomputed cycle.
        for(int p=0;p<cycle;++p)for(int i=0;i<hop;++i) {
            double weight=0;
            for(int k=0;k<overlap;++k) {
                const int offset=k*hop+i;
                weight+=modifiedWindow(offset,p-k)*window_[offset];
            }
            minimumWeight_=std::min(minimumWeight_,weight);
            demodulation_[p*hop+i]=1/std::max(.1,weight); // Author's reciprocal cap, no callback logging.
        }
        reset();
    }
    // Unity reconstruction delay, excluding asynchronous preparation and host/device buffering.
    static constexpr int alignmentDelaySamples() noexcept {return n;}
    double minimumDemodulationWeight() const noexcept {return minimumWeight_;}
    void reset() noexcept {
        input_.fill(0);pending_.fill(0);ola_.fill(0);output_.fill(0);
        position_=0;phase_=mod(-(overlap-1));
    }
    bool process(const float* in,float* out,std::size_t frames) noexcept {
        if(!in || !out || frames>maxBlock_) {if(out)std::fill_n(out,frames,0.f);return false;}
        bool finite=true;
        for(std::size_t i=0;i<frames;++i) {
            pending_[position_]=std::isfinite(in[i]) ? double(in[i]):0.; // Read before writing: in-place safe.
            out[i]=float(output_[position_]);
            if(!std::isfinite(out[i])) {out[i]=0;finite=false;}
            if(++position_==hop) {renderFrame();position_=0;}
        }
        return finite;
    }
private:
    using Complex=std::complex<double>;
    static int mod(int p) noexcept {return (p%cycle+cycle)%cycle;}
    // Discrete modified Hann from the reference's computeModifiedAnalysisWindow*. Integer
    // division/floor is intentional, including fractional padded ratios interpolated below.
    double integerWindow(int i,int p,int r) const noexcept {
        const int shift=n*mod(p)*mod(r-padding)/cycle;
        return window_[(i*r/padding+shift)%n];
    }
    double modifiedWindow(int i,int p) const noexcept {
        const double r=ratio_*padding;const int low=int(r);const double fraction=r-low;
        return integerWindow(i,p,low)*(1-fraction)+integerWindow(i,p,low+1)*fraction;
    }
    void renderFrame() noexcept {
        std::copy(input_.begin()+hop,input_.end(),input_.begin());
        std::copy(pending_.begin(),pending_.end(),input_.end()-hop);
        for(int j=0;j<n;++j)time_[j]={input_[j]*window_[j],0};
        analysisFFT_.fft(time_.data(),spectrum_.data());
        std::fill(shifted_.begin(),shifted_.end(),Complex{});
        shifted_[0]={spectrum_[0].real(),0}; // Author's unshifted DC.
        for(int a=1;a<bins;++a) {
            const int b=destination_[a];
            if(b>0 && b<=synthesis/2)
                // Corrected padded phase formula. Positive for our exp(-i*w*n) FFT;
                // author's BooFFT real transform uses its conjugate (fixSpectrum note).
                shifted_[b]+=spectrum_[a]*roots_[(phase_*rotation_[a])%cycle];
        }
        // Preserve source real-IFFT endpoint convention, including relocated input Nyquist.
        shifted_[synthesis/2]={shifted_[synthesis/2].real(),0};
        for(int b=1;b<synthesis/2;++b)shifted_[synthesis-b]=std::conj(shifted_[b]);
        synthesisFFT_.ifft(shifted_.data(),inverse_.data());
        // BooFFT: forward 2/N, inverse half unscaled. Combined gain is 1/N,
        // NOT 1/(mN). Only the first N synthesis samples are used (paper section 3.4).
        for(int i=0;i<hop;++i)
            output_[i]=(ola_[i]+inverse_[i].real()*window_[i]/n)*demodulation_[phase_*hop+i];
        std::copy(ola_.begin()+hop,ola_.end(),ola_.begin());
        std::fill(ola_.end()-hop,ola_.end(),0.);
        for(int i=0;i<n-hop;++i)ola_[i]+=inverse_[i+hop].real()*window_[i+hop]/n;
        phase_=(phase_+1)%cycle;
    }
    std::size_t maxBlock_;
    double ratio_,minimumWeight_=100;
    int position_=0,phase_=0;
    signalsmith::linear::SimpleFFT<double> analysisFFT_,synthesisFFT_;
    std::vector<Complex> time_,spectrum_,shifted_,inverse_;
    std::array<Complex,cycle> roots_{};
    std::array<int,bins> destination_{},rotation_{};
    std::array<double,n> window_{},input_{};
    std::array<double,hop> pending_{},output_{};
    std::array<double,n-hop> ola_{};
    std::array<double,cycle*hop> demodulation_{};
};
} // namespace pitch
