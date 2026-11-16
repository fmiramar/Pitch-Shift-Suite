#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>
namespace pitch {
// Independent granular OLA + resampling. Prepare/destroy outside audio callback.
class OLAStretchShift {
    static constexpr int half=16, taps=33, phases=64, bands=9;
    static constexpr double pi=3.14159265358979323846;
    struct Grain { std::size_t age=0; double ratio=1; int shift=0; };
public:
    OLAStretchShift(double rate,std::size_t maxBlock,double seconds=.04,int overlap=4,int window=0,int searchRadius=0)
        : maxBlock_(maxBlock),overlap_(overlap),search_(searchRadius) {
        if (!std::isfinite(rate) || rate<44100 || rate>96000 || !maxBlock || maxBlock>4096 ||
            !std::isfinite(seconds) || seconds<.02-1e-8 || seconds>.08+1e-8 ||
            (overlap!=2 && overlap!=4) || (window!=0 && window!=1) || searchRadius<0 || searchRadius>512)
            throw std::invalid_argument("Invalid OLA preparation");
        length_=std::size_t(std::llround(rate*seconds/overlap))*std::size_t(overlap);
        hop_=length_/std::size_t(overlap);delay_=length_+half+std::size_t(search_);
        history_.resize(4*length_+2*half+2*std::size_t(search_)+4,0.f);window_.resize(length_);
        for (std::size_t i=0;i<length_;++i) {
            double phase=double(i)/length_;
            window_[i]=window==0 ? .5-.5*std::cos(2*pi*phase) : std::sin(pi*phase);
        }
        for (int k=0;k<overlap;++k) grains_[k].age=std::size_t(k)*hop_;
        smoothing_=1-std::exp(-1/(.005*rate));
        coefficients_.resize(bands*(phases+1)*taps);
        for (int band=0;band<bands;++band) for (int phase=0;phase<=phases;++phase) {
            double cutoff=.475/(1+double(band)/8),sum=0;
            double row[taps]{};
            for (int k=-half;k<=half;++k) {
                double x=k+double(phase)/phases;
                if (std::abs(x)>half) continue;
                double sinc=std::abs(x)<1e-12 ? 2*cutoff : std::sin(2*pi*cutoff*x)/(pi*x);
                double w=.42+.5*std::cos(pi*x/half)+.08*std::cos(2*pi*x/half);
                row[k+half]=sinc*w;sum+=row[k+half];
            }
            for (int k=0;k<taps;++k) coefficients_[(band*(phases+1)+phase)*taps+k]=float(row[k]/sum);
        }
    }
    static bool validRatio(float r) { return std::isfinite(r) && r>=.5f && r<=2.f; }
    std::size_t unityDelaySamples() const { return delay_; }
    std::size_t grainSamples() const { return length_; }
    int lastSearchShift() const { return lastShift_; }
    bool process(const float* input,float* output,std::size_t frames,float ratio) noexcept {
        if (!output) return false;
        if (!input || frames>maxBlock_ || !validRatio(ratio)) { std::fill_n(output,frames,0.f);return false; }
        if (first_) { ratio_=ratio;for (auto& g:grains_) g.ratio=ratio;first_=false; }
        bool finite=true;
        for (std::size_t i=0;i<frames;++i) {
            history_[write_]=std::isfinite(input[i]) ? input[i] : 0.f;
            ratio_+=smoothing_*(ratio-ratio_);
            if (std::abs(ratio_-ratio)<1e-9) ratio_=ratio;
            double sum=0,weight=0;
            // Make all decisions against ages at the same sample instant.
            for (int k=0;k<overlap_;++k) if (grains_[k].age==0) {
                grains_[k].ratio=ratio_;
                grains_[k].shift=search_ && std::abs(ratio_-1)>1e-9 ? chooseShift(k) : 0;
                lastShift_=grains_[k].shift;
            }
            for (int k=0;k<overlap_;++k) {
                auto& g=grains_[k];
                const double w=window_[g.age];
                // Launch at input position launchTime-delay; read at grain ratio.
                const double lag=double(delay_)+(1-g.ratio)*g.age-g.shift;
                sum+=w*read(lag,g.ratio);weight+=w;
                if (++g.age==length_) g.age=0;
            }
            output[i]=float(sum/weight);
            if (!std::isfinite(output[i])) { output[i]=0;finite=false; }
            if (++write_==history_.size()) write_=0;
        }
        return finite;
    }
private:
    int chooseShift(int current) const noexcept {
        // WSOLA: match the preceding grain's natural continuation. Integer
        // displacement, mean-removed normalized correlation over 64 input samples.
        int previous=-1;
        for (int k=0;k<overlap_;++k) if (k!=current && grains_[k].age>0 &&
            (previous<0 || grains_[k].age<grains_[previous].age)) previous=k;
        if (previous<0) return 0;
        const auto& g=grains_[previous];
        double lag=double(delay_)+(1-g.ratio)*g.age-g.shift;
        int reference=int(std::floor(lag));double fraction=lag-reference;
        double ref[64],mean=0,energy=0;
        for (int n=0;n<64;++n) {
            ref[n]=(1-fraction)*at(reference-n)+fraction*at(reference+1-n);mean+=ref[n]/64;
        }
        for (double& x:ref) { x-=mean;energy+=x*x; }
        if (energy<1e-12) return 0; // Deterministic silence/DC fallback.
        double best=-2;int choice=0;
        for (int step=0;step<=2*search_;++step) {
            int delta=step==0 ? 0 : (step%2 ? -(step+1)/2 : step/2);
            double sum=0,squares=0,cross=0;
            for (int n=0;n<64;++n) {
                double x=at(int(delay_)-delta-n);sum+=x;squares+=x*x;cross+=ref[n]*x;
            }
            double variance=squares-sum*sum/64;
            if (variance<1e-12) continue;
            double score=cross/std::sqrt(energy*variance);
            if (score>best+1e-10) { best=score;choice=delta; }
        }
        return choice; // Nearest zero wins equal scores; negative wins ± ties.
    }
    float at(int lag) const noexcept { return history_[(write_+history_.size()-std::size_t(lag))%history_.size()]; }
    double read(double lag,double ratio) const noexcept {
        if (std::abs(ratio-1)<1e-9) return at(int(std::llround(lag))); // Exact unity reconstruction.
        int base=int(std::floor(lag));double phase=(lag-base)*phases;
        int p=std::min(phases-1,int(phase));double blend=phase-p;
        int band=std::clamp(int(std::ceil(std::max(1.,ratio)*8))-8,0,bands-1);
        const float* a=coefficients_.data()+(band*(phases+1)+p)*taps;
        const float* b=a+taps;
        double sum=0;
        for (int k=-half;k<=half;++k) sum+=at(base-k)*(a[k+half]+blend*(b[k+half]-a[k+half]));
        return sum;
    }
    std::size_t maxBlock_,length_=0,hop_=0,delay_=0,write_=0;
    int overlap_,search_=0,lastShift_=0;
    double smoothing_=0,ratio_=1;
    bool first_=true;
    std::array<Grain,4> grains_{};
    std::vector<float> history_,coefficients_;
    std::vector<double> window_;
};
}
