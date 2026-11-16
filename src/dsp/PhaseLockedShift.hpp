#pragma once
#include "signalsmith-linear/fft.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

namespace pitch {
// A09: centered STFT, Laroche–Dolson (1999) §§II-B/III-C, then frame-local
// stretch/resample. See docs/algorithms/A09-phase-locked.md for conventions.
class PhaseLockedShift {
public:
    PhaseLockedShift(double sampleRate, std::size_t maxBlock, int fftSize=2048, int mode=1)
        : rate_(sampleRate), maxBlock_(maxBlock), n_(fftSize), mode_(mode) {
        if (!std::isfinite(rate_) || rate_<8000 || rate_>192000 || !maxBlock_ || maxBlock_>4096 ||
            (n_!=512 && n_!=1024 && n_!=2048 && n_!=4096) || mode_<0 || mode_>2)
            throw std::invalid_argument("PhaseLockedShift configuration");
        hop_=n_/8; bins_=n_/2+1; delay_=3*n_/2;
        fft_.resize(n_);
        input_.resize(n_); sum_.resize(8*n_); weight_.resize(8*n_);
        time_.resize(n_); spectrum_.resize(n_); window_.resize(n_);
        phase_.resize(bins_); magnitude_.resize(bins_); previousPhase_.resize(bins_);
        previousMagnitude_.resize(bins_); synthesis_.resize(bins_); previousSynthesis_.resize(bins_);
        owner_.resize(bins_,-1); previousOwner_.resize(bins_,-1); peaks_.resize(bins_);
        for (int j=0;j<n_;++j) window_[j]=.5-.5*std::cos(tau*j/n_);
        smoothing_=1-std::exp(-1/(.005*rate_));
        makeResampler(); // All memory/plans/tables are prepared off the audio thread.
    }
    std::size_t unityDelaySamples() const noexcept { return delay_; }
    int hopSamples() const noexcept { return hop_; }
    bool process(const float* input, float* output, std::size_t frames, float ratio) noexcept {
        if (!input || !output || frames>maxBlock_ || !std::isfinite(ratio) || ratio<.5f || ratio>2.f) {
            if (output) std::fill_n(output,frames,0.f);
            return false; // Invalid controls silence this block and pause state.
        }
        if (!started_) { smoothed_=ratio; started_=true; }
        bool finite=true;
        for (std::size_t i=0;i<frames;++i) {
            input_[std::size_t(clock_)&(n_-1)]=std::isfinite(input[i]) ? input[i] : 0.f;
            smoothed_+=smoothing_*(ratio-smoothed_);
            if (std::abs(smoothed_-ratio)<1e-10) smoothed_=ratio;
            ratioSum_+=smoothed_;
            if (++untilFrame_==hop_) {
                renderFrame(ratioSum_/hop_); ratioSum_=0; untilFrame_=0;
            }
            auto slot=std::size_t(clock_)&(sum_.size()-1);
            // Full coverage has at least 1.5 summed Hann^2 (ratio<=2, hop=N/8).
            // Startup is zero primed: never amplify an incomplete window edge.
            double value=sum_[slot]/std::max(1.5,weight_[slot]);
            output[i]=float(value);
            if (!std::isfinite(output[i])) { output[i]=0; finite=false; }
            sum_[slot]=weight_[slot]=0; ++clock_;
        }
        return finite;
    }
private:
    friend struct PhaseLockedShiftProbe; // Numerical phase tests; no host-facing diagnostics API.
    static constexpr double tau=6.283185307179586476925286766559;
    static constexpr int radius=16, phases=64, bands=9;
    static double principal(double x) noexcept { return x-tau*std::floor(x/tau+.5); }
    double rate_, smoothing_=0, smoothed_=1, ratioSum_=0, previousFloor_=0;
    std::size_t maxBlock_;
    int n_, mode_, hop_=0, bins_=0, delay_=0, untilFrame_=0;
    std::int64_t clock_=0;
    bool started_=false, haveFrame_=false;
    signalsmith::linear::SimpleFFT<double> fft_;
    std::vector<double> input_,sum_,weight_,window_,phase_,magnitude_,previousPhase_,previousMagnitude_,synthesis_,previousSynthesis_;
    std::vector<int> owner_,previousOwner_,peaks_;
    std::vector<std::complex<double>> time_,spectrum_;
    std::array<double,bands*(phases+1)*(2*radius+1)> sinc_{};

    void makeResampler() {
        for (int b=0;b<bands;++b) for (int p=0;p<=phases;++p) {
            double cutoff=.475/(1+b/8.), fraction=double(p)/phases, total=0;
            auto* row=&sinc_[(b*(phases+1)+p)*(2*radius+1)];
            for (int j=-radius;j<=radius;++j) {
                double x=j-fraction;
                double window=std::abs(x)<=radius ? .42+.5*std::cos(tau*x/(2*radius))+.08*std::cos(tau*x/radius) : 0;
                row[j+radius]=window*(std::abs(x)<1e-12 ? 2*cutoff : std::sin(tau*cutoff*x)/(tau*x/2));
                total+=row[j+radius];
            }
            for (int j=0;j<=2*radius;++j) row[j]/=total;
        }
    }
    double readFrame(double position, int band) const noexcept {
        int base=int(std::floor(position));
        double phase=(position-base)*phases;
        int p=std::min(phases-1,int(phase)); double mix=phase-p;
        auto* a=&sinc_[(band*(phases+1)+p)*(2*radius+1)];
        auto* b=a+2*radius+1;
        double value=0;
        for (int j=-radius;j<=radius;++j)
            value+=time_[std::size_t(base+j)&(n_-1)].real()*(a[j+radius]+mix*(b[j+radius]-a[j+radius]));
        return value;
    }
    void updatePhases(double ratio) noexcept {
        double maximum=0;
        for (int k=0;k<bins_;++k) {
            magnitude_[k]=std::abs(spectrum_[k]); phase_[k]=std::arg(spectrum_[k]);
            maximum=std::max(maximum,magnitude_[k]);
        }
        const double floor=std::max(n_*1e-12,maximum*1e-8);
        int count=0;
        std::fill(owner_.begin(),owner_.end(),-1);
        // §III-C: maximum over four neighbours, midpoint regions (ties to lower peak).
        for (int k=1;k<bins_-1;++k) {
            bool peak=magnitude_[k]>floor;
            for (int d=-2;d<=2;++d) if (d && k+d>=0 && k+d<bins_)
                peak=peak && magnitude_[k]>magnitude_[k+d];
            if (peak) peaks_[count++]=k;
        }
        int region=0;
        for (int k=1;count && k<bins_-1;++k) {
            while (region+1<count && 2*k>peaks_[region]+peaks_[region+1]) ++region;
            owner_[k]=peaks_[region];
        }
        // Eq. (5)/(6); scaled mode uses the previous region's peak, Eq. (15).
        // Centered FFT removes the alternating pi offset when peak bins change.
        for (int k=0;k<bins_;++k) {
            int prior=(mode_==2 && owner_[k]==k) ? previousOwner_[k] : k;
            synthesis_[k]=phase_[k]; // Onset/low energy/unmatched: analysis phase.
            if (haveFrame_ && magnitude_[k]>floor && prior>=0 && previousMagnitude_[prior]>previousFloor_) {
                double expected=tau*k*hop_/n_;
                double advance=expected+principal(phase_[k]-previousPhase_[prior]-expected);
                synthesis_[k]=principal(previousSynthesis_[prior]+ratio*advance);
            }
        }
        if (mode_ && count) {
            const double beta=mode_==2 ? (2+ratio)/3 : 1; // §III-C2 empirical choice.
            for (int i=0;i<count;++i) {
                int p=peaks_[i]; double difference=0;
                // Eq. (12) for identity; Eq. (16) with vertical unwrapping for scaled.
                for (int k=p+1;k<bins_-1 && owner_[k]==p;++k) {
                    difference+=principal(phase_[k]-phase_[k-1]);
                    synthesis_[k]=principal(synthesis_[p]+beta*difference);
                }
                difference=0;
                for (int k=p-1;k>0 && owner_[k]==p;--k) {
                    difference+=principal(phase_[k]-phase_[k+1]);
                    synthesis_[k]=principal(synthesis_[p]+beta*difference);
                }
            }
        }
        for (int k=0;k<bins_;++k) {
            previousPhase_[k]=phase_[k]; previousMagnitude_[k]=magnitude_[k];
            previousSynthesis_[k]=synthesis_[k]; previousOwner_[k]=owner_[k];
            // Unity phases remain propagated (no bypass), so returning to unity
            // does not reset accumulated phase. Initial unity reconstructs exactly.
            if (k==0 || k==bins_-1) {
                // Real signal endpoints cannot carry arbitrary complex phases.
                // Preserve signed DC/Nyquist; the resampler rejects shifted Nyquist.
                spectrum_[k]={spectrum_[k].real(),0};
            } else spectrum_[k]=std::polar(magnitude_[k],synthesis_[k]);
        }
        for (int k=1;k<bins_-1;++k) spectrum_[n_-k]=std::conj(spectrum_[k]);
        previousFloor_=floor; haveFrame_=true;
    }
    void renderFrame(double ratio) noexcept {
        const auto center=clock_+1-n_/2;
        for (int j=0;j<n_;++j) {
            auto index=center+j-n_/2;
            time_[(j+n_/2)&(n_-1)]={input_[std::size_t(index)&(n_-1)]*window_[j],0};
        }
        fft_.fft(time_.data(),spectrum_.data());
        updatePhases(ratio);
        fft_.ifft(spectrum_.data(),time_.data());
        // IFFT is unnormalised. Frame-local resampling equals stretch→resample
        // at constant ratio, with virtual synthesis hop ratio*hop (no rounded hop).
        const int band=std::min(8,std::max(0,int(std::ceil((ratio-1)*8))));
        const int first=int(std::ceil(-n_/(2*ratio))), end=int(std::ceil(n_/(2*ratio)));
        for (int j=first;j<end;++j) {
            double position=ratio*j;
            double window=.5+.5*std::cos(tau*position/n_);
            double sample=ratio==1 ? time_[std::size_t(j)&(n_-1)].real() : readFrame(position,band);
            auto slot=std::size_t(center+delay_+j)&(sum_.size()-1);
            sum_[slot]+=sample*window/n_;
            weight_[slot]+=window*window;
        }
    }
};
} // namespace pitch
