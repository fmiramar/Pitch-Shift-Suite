// Rubber Band integration: GPL-2.0-or-later distribution boundary; see README.md.
#pragma once
#include "rubberband/RubberBandStretcher.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>
namespace pitch {
class RubberBandEngine {
    using RB=RubberBand::RubberBandStretcher;
    struct QuietLogger : RB::Logger {
        void log(const char*) override {}
        void log(const char*,double) override {}
        void log(const char*,double,double) override {}
    };
public:
    RubberBandEngine(double rate,std::size_t block,int version,float ratio=1) :
        fifo_(validate(rate,block,version,ratio)), input_(block), scratch_(block), maxBlock_(block), ratio_(ratio) {
        int options=RB::OptionProcessRealTime | RB::OptionThreadingNever | RB::OptionPitchHighConsistency | RB::OptionTransientsSmooth |
                    (version==2 ? RB::OptionEngineFaster : RB::OptionEngineFiner);
        engine_=std::make_unique<RB>(std::size_t(rate),1,std::make_shared<QuietLogger>(),options,1.,double(ratio));
        if (engine_->getEngineVersion()!=version) throw std::runtime_error("Wrong Rubber Band engine version");
        engine_->setDebugLevel(0);
        engine_->setMaxProcessSize(block);
        if (version==3) {
            // R3/vDSP lazily creates double-precision FFT state on non-silence.
            // Exercise it on the preparation worker, then clear signal history.
            for (float warmRatio : {.5f,1.f,2.f}) {
                engine_->setPitchScale(warmRatio);
                for (std::size_t pos=0;pos<std::size_t(rate/4);pos+=block) {
                    for (std::size_t i=0;i<block;++i) input_[i]=float(.01*std::sin(6.283185307179586*440*(pos+i)/rate));
                    const float* in[]={input_.data()};engine_->process(in,block,false);
                    while (engine_->available()>0) {
                        float* out[]={scratch_.data()};
                        engine_->retrieve(out,std::min<std::size_t>(block,std::size_t(engine_->available())));
                    }
                }
            }
            engine_->reset();engine_->setPitchScale(ratio);
            std::fill(input_.begin(),input_.end(),0.f);
        }
        // Deliberately conservative causal output reserve; upstream startup
        // padding/trim is distinct from this host-output reserve.
        reserve_=std::size_t(rate*.25);
        count_=reserve_; write_=reserve_;
        startDelay_=engine_->getStartDelay(); preferredPad_=engine_->getPreferredStartPad();
        skip_=startDelay_;
        for (std::size_t left=preferredPad_;left;) {
            auto n=std::min(left,block);const float* zeros[]={input_.data()};
            engine_->process(zeros,n,false);drain();left-=n;
        }
        if (fault_) throw std::runtime_error("Rubber Band preparation overflow");
    }
    static bool validRatio(float ratio) { return std::isfinite(ratio) && ratio>=.5f && ratio<=2.f; }
    bool process(const float* input,float* output,std::size_t frames,float ratio) noexcept {
        if (!output) return false;
        if (!input || frames>maxBlock_ || !validRatio(ratio) || fault_) {
            std::fill_n(output,frames,0.f);return false;
        }
        if (ratio!=ratio_) { engine_->setPitchScale(ratio);ratio_=ratio; }
        for (std::size_t i=0;i<frames;++i) input_[i]=std::isfinite(input[i]) ? input[i] : 0;
        const float* in[]={input_.data()}; engine_->process(in,frames,false);
        drain();
        if (fault_ || count_<frames) {
            fault_=true;std::fill_n(output,frames,0.f);return false;
        }
        for (std::size_t i=0;i<frames;++i) {
            output[i]=fifo_[read_];read_=(read_+1)%fifo_.size();
            if (!std::isfinite(output[i])) { output[i]=0;fault_=true; }
        }
        count_-=frames;
        return !fault_;
    }
    int version() const { return engine_->getEngineVersion(); }
    std::size_t reserveSamples() const { return reserve_; }
    std::size_t startDelay() const { return startDelay_; }
    std::size_t preferredPad() const { return preferredPad_; }
    std::size_t queuedSamples() const { return count_; }
private:
    void drain() noexcept {
        while (engine_->available()>0) {
            auto n=std::min<std::size_t>(scratch_.size(),std::size_t(engine_->available()));
            float* out[]={scratch_.data()};n=engine_->retrieve(out,n);
            auto drop=std::min(skip_,n);skip_-=drop;
            if (!n || n-drop>fifo_.size()-count_) { fault_=true;break; }
            for (std::size_t i=drop;i<n;++i) {
                fifo_[write_]=scratch_[i];write_=(write_+1)%fifo_.size();
            }
            count_+=n-drop;
        }
    }
    std::unique_ptr<RB> engine_;
    static std::size_t validate(double rate,std::size_t block,int version,float ratio) {
        if (!std::isfinite(rate) || rate<44100 || rate>96000 || !block || block>4096 ||
            (version!=2 && version!=3) || !validRatio(ratio)) throw std::invalid_argument("Invalid Rubber Band preparation");
        return std::size_t(rate);
    }
    std::vector<float> fifo_,input_,scratch_;
    std::size_t maxBlock_,reserve_=0,startDelay_=0,preferredPad_=0;
    std::size_t read_=0,write_=0,count_=0,skip_=0;
    float ratio_;
    bool fault_=false;
};
}
