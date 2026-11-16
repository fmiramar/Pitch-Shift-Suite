#pragma once
#include "signalsmith-linear/fft.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <complex>
#include <vector>
#include <stdexcept>

namespace pitch {
// A14: independent bounded SMS adaptation. Serra & Smith (1990), pp.14–23.
// Fixed mono preset, magnitude residual stays at source frequencies. See A14 sheet.
class SinusoidalResidualShift {
public:
    static constexpr int windowSize=2048, fftSize=8192, hop=256, maxTracks=64;
    static constexpr int noiseFFT=2048, noiseWindow=1024, delay=2560;
    static constexpr double tau=6.283185307179586476925286766559;
    SinusoidalResidualShift(double rate, std::size_t maxBlock, double residual=1.)
        : rate_(rate), residual_(residual), maxBlock_(maxBlock) {
        if ((rate!=44100 && rate!=48000) || maxBlock==0 || maxBlock>4096 ||
            !std::isfinite(residual) || residual<0 || residual>1)
            throw std::invalid_argument("SinusoidalResidualShift configuration");
        fft_.resize(fftSize); noiseFFT_.resize(noiseFFT);
        time_.resize(fftSize); spectrum_.resize(fftSize); model_.resize(fftSize);
        noiseTime_.resize(noiseFFT); noiseSpectrum_.resize(noiseFFT);
        for (int j=0;j<windowSize;++j) {
            double x=tau*j/windowSize;
            window_[j]=.35875-.48829*std::cos(x)+.14128*std::cos(2*x)-.01168*std::cos(3*x);
            windowSum_+=window_[j]; windowPower_+=window_[j]*window_[j];
        }
        for (int j=0;j<noiseWindow;++j) noiseWin_[j]=.5-.5*std::cos(tau*j/noiseWindow);
        smooth_=1-std::exp(-1/(.005*rate));
    }
    // Integral of a linearly interpolated radian frequency. Page20's printed
    // omega(m)*m conflicts with the page14 integral for nonconstant omega.
    static double phaseIntegral(double phase,double w0,double w1,double t) noexcept {
        return phase+w0*t+(w1-w0)*t*t/(2*hop);
    }
    int activeTracks() const noexcept {
        int count=0;for(const auto& t:tracks_)count+=t.active;return count;
    }
    void reset() noexcept {
        input_.fill(0);det_.fill(0);output_.fill(0);tracks_.fill({});
        clock_=0;random_=0x12345678U;started_=false;ratio_=previousRatio_=1;ratioSum_=0;
    }
    bool process(const float* input,float* output,std::size_t frames,float ratio) noexcept {
        if (!input || !output || frames>maxBlock_ || !std::isfinite(ratio) || ratio<.5f || ratio>2.f) {
            if(output)std::fill_n(output,frames,0.f);return false;
        }
        if (!started_) {ratio_=previousRatio_=ratio;started_=true;}
        for (std::size_t j=0;j<frames;++j,++clock_) {
            double x=std::isfinite(input[j]) ? std::clamp(double(input[j]),-1e6,1e6):0;
            input_[index(clock_)]=std::abs(x)<1e-20 ? 0:x;
            ratio_+=smooth_*(ratio-ratio_);ratioSum_+=ratio_;
            // A full frame can only write to samples after the current one.
            if ((clock_+1)%hop==0) {frame(clock_+1,ratioSum_/hop);ratioSum_=0;}
            output[j]=clock_<delay ? 0.f:float(output_[index(clock_)]);
            output_[index(clock_)]=0;
        }
        return true;
    }
private:
    static constexpr int ring=8192, bins=fftSize/2+1;
    struct Peak {double frequency=0,amplitude=0;};
    struct Track {double frequency=0,amplitude=0,phase=0,shiftPhase=0;bool active=false;};
    struct Pair {double distance;int track,peak;};
    double rate_,residual_,windowSum_=0,windowPower_=0,smooth_=0;
    std::size_t maxBlock_;
    std::int64_t clock_=0;
    bool started_=false;
    double ratio_=1,previousRatio_=1,ratioSum_=0;
    std::uint32_t random_=0x12345678U;
    signalsmith::linear::SimpleFFT<double> fft_,noiseFFT_;
    std::vector<std::complex<double>> time_,spectrum_,model_,noiseTime_,noiseSpectrum_;
    std::array<double,ring> input_{},det_{},output_{};
    std::array<double,windowSize> window_{};
    std::array<double,noiseWindow> noiseWin_{};
    std::array<double,bins> magnitude_{},residualMagnitude_{};
    std::array<double,129> envelope_{};
    std::array<double,noiseFFT/2+1> noiseMagnitude_{};
    std::array<Peak,maxTracks> peaks_{};
    std::array<Track,maxTracks> tracks_{};
    std::array<Pair,maxTracks*maxTracks> pairs_{};
    static std::size_t index(std::int64_t n) noexcept {return std::uint64_t(n)&(ring-1);}
    void analyze(const std::array<double,ring>& source,std::int64_t center,
                 std::vector<std::complex<double>>& target) noexcept {
        std::fill(time_.begin(),time_.end(),std::complex<double>{});
        for(int j=0;j<windowSize;++j)time_[j]=source[index(center-windowSize/2+j)]*window_[j];
        fft_.fft(time_.data(),target.data());
    }
    int peaks() noexcept {
        double top=0;for(int k=0;k<bins;++k) {magnitude_[k]=std::abs(spectrum_[k]);top=std::max(top,magnitude_[k]);}
        const double floor=std::max(1e-10,top*.001);int count=0;
        for(int k=1;k<bins-1;++k) {
            double b=magnitude_[k];if(b<floor || b<=magnitude_[k-1] || b<=magnitude_[k+1])continue;
            int left=k-1,right=k+1;
            while(left>0 && magnitude_[left-1]<magnitude_[left])--left;
            while(right<bins-1 && magnitude_[right+1]<magnitude_[right])++right;
            if(b<4*std::max(magnitude_[left],magnitude_[right]))continue;
            double a=std::log(std::max(magnitude_[k-1],1e-30)),c=std::log(std::max(magnitude_[k+1],1e-30));
            b=std::log(b);double offset=.5*(a-c)/(a-2*b+c);
            Peak peak{(k+offset)*rate_/fftSize,2*std::exp(b-.25*(a-c)*offset)/windowSum_};
            if(peak.frequency<40 || peak.frequency>.48*rate_)continue;
            int at=0;while(at<count && peaks_[at].amplitude>=peak.amplitude)++at;
            if(at==maxTracks)continue;
            for(int j=std::min(count,maxTracks-1);j>at;--j)peaks_[j]=peaks_[j-1];
            peaks_[at]=peak;count=std::min(count+1,maxTracks);
        }
        return count;
    }
    void frame(std::int64_t end,double ratio) noexcept {
        auto center=end-windowSize/2;
        analyze(input_,center,spectrum_);int count=peaks(),pairCount=0;
        std::array<int,maxTracks> assigned;assigned.fill(-1);
        std::array<bool,maxTracks> claimed{};
        for(int i=0;i<maxTracks;++i)if(tracks_[i].active)for(int k=0;k<count;++k) {
            double distance=std::abs(tracks_[i].frequency-peaks_[k].frequency);
            if(distance<=std::max(40.,.03*tracks_[i].frequency))pairs_[pairCount++]={distance,i,k};
        }
        // Bounded closest-pair conflict resolution, alpha=1, no sleeping guides.
        std::sort(pairs_.begin(),pairs_.begin()+pairCount,[](const Pair& a,const Pair& b) {
            if(a.distance!=b.distance)return a.distance<b.distance;
            return a.track!=b.track ? a.track<b.track:a.peak<b.peak;
        });
        for(int j=0;j<pairCount;++j) {
            auto p=pairs_[j];if(assigned[p.track]<0 && !claimed[p.peak]) {assigned[p.track]=p.peak;claimed[p.peak]=true;}
        }
        for(int k=0;k<count;++k)if(!claimed[k]) {
            bool close=false;int free=-1;
            for(int i=0;i<maxTracks;++i) {
                if(tracks_[i].active)close|=std::abs(tracks_[i].frequency-peaks_[k].frequency)<rate_/windowSize;
                else if(free<0)free=i;
            }
            if(close)continue;if(free<0)break;
            tracks_[free]={peaks_[k].frequency,0,0,0,true};assigned[free]=k;claimed[k]=true;
        }
        for(int j=0;j<hop;++j)det_[index(center-hop+j)]=0;
        for(int i=0;i<maxTracks;++i)if(tracks_[i].active) {
            auto& old=tracks_[i];Peak next=assigned[i]>=0 ? peaks_[assigned[i]]:Peak{old.frequency,0};
            double w0=tau*old.frequency/rate_,w1=tau*next.frequency/rate_;
            double u0=previousRatio_*w0,u1=ratio*w1;
            double a0=old.amplitude*std::clamp((.49*rate_-previousRatio_*old.frequency)/(.04*rate_),0.,1.);
            double a1=next.amplitude*std::clamp((.49*rate_-ratio*next.frequency)/(.04*rate_),0.,1.);
            for(int j=0;j<hop;++j) {
                double t=double(j)/hop;
                auto pos=center-hop+j;
                det_[index(pos)]+=(old.amplitude+(next.amplitude-old.amplitude)*t)*std::cos(phaseIntegral(old.phase,w0,w1,j));
                output_[index(pos+delay)]+=(a0+(a1-a0)*t)*std::cos(phaseIntegral(old.shiftPhase,u0,u1,j));
            }
            old={next.frequency,next.amplitude,std::remainder(phaseIntegral(old.phase,w0,w1,hop),tau),
                 std::remainder(phaseIntegral(old.shiftPhase,u0,u1,hop),tau),next.amplitude>0};
        }
        previousRatio_=ratio;
        // Reanalyze an older window now completely covered by the deterministic synthesis.
        auto residualCenter=end-windowSize;
        analyze(input_,residualCenter,spectrum_);analyze(det_,residualCenter,model_);
        double rawPower=0;
        for(int k=0;k<bins;++k) {
            double r=std::abs(std::abs(spectrum_[k])-std::abs(model_[k]));residualMagnitude_[k]=r;
            rawPower+=(k==0 || k==bins-1 ? 1:2)*r*r;
        }
        for(int q=0;q<129;++q) {
            double value=0;for(int k=std::max(0,32*q-16);k<=std::min(bins-1,32*q+16);++k)value=std::max(value,residualMagnitude_[k]);
            envelope_[q]=value;
        }
        double envPower=0;
        for(int k=0;k<=noiseFFT/2;++k) {
            double q=double(k)/8;int lo=std::min(int(q),127);double fraction=q-lo;
            double e=envelope_[lo]+(envelope_[lo+1]-envelope_[lo])*fraction;noiseMagnitude_[k]=e;
            envPower+=(k==0 || k==noiseFFT/2 ? 1:2)*e*e;
        }
        // Correct upper-envelope energy inflation and expected independent-frame
        // Hann overlap power (sum w_s^2=1.5); this normalization is our adaptation.
        double gain=envPower>1e-30 ? std::sqrt(rawPower/envPower*noiseFFT/fftSize)*std::sqrt(noiseFFT/(windowPower_*1.5)):0;
        for(int k=0;k<=noiseFFT/2;++k) {
            random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;
            double phase=tau*(double(random_)/4294967296.);
            auto z=std::polar(noiseMagnitude_[k]*gain,phase);noiseSpectrum_[k]=z;
            if(k>0 && k<noiseFFT/2)noiseSpectrum_[noiseFFT-k]=std::conj(z);
        }
        noiseSpectrum_[0]=noiseSpectrum_[noiseFFT/2]=0;
        noiseFFT_.ifft(noiseSpectrum_.data(),noiseTime_.data());
        for(int j=0;j<noiseWindow;++j)output_[index(residualCenter-noiseWindow/2+j+delay)]+=residual_*noiseTime_[j].real()*noiseWin_[j]/noiseFFT;
    }
};
} // namespace pitch
