// Optional GPL-2.0-or-later module; see README.md.
#pragma once
#include "rubberband/RubberBandLiveShifter.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>
namespace pitch {
class RubberBandLiveEngine {
    using RB=RubberBand::RubberBandLiveShifter;
    struct QuietLogger : RB::Logger {
        void log(const char*) override {}
        void log(const char*,double) override {}
        void log(const char*,double,double) override {}
    };
public:
    // Construct and destroy off the audio thread.
    RubberBandLiveEngine(double rate,std::size_t maxBlock,float ratio=1) : maxBlock_(maxBlock) {
        if (!std::isfinite(rate) || rate<44100 || rate>96000 || !maxBlock || maxBlock>4096 ||
            !validRatio(ratio)) throw std::invalid_argument("Invalid LiveShifter preparation");
        engine_=std::make_unique<RB>(std::size_t(rate),1,std::make_shared<QuietLogger>(),RB::DefaultOptions);
        engine_->setDebugLevel(0);engine_->setPitchScale(ratio);
        block_=engine_->getBlockSize();delay_=engine_->getStartDelay();
        if (!block_ || block_>65536) throw std::runtime_error("Unsupported LiveShifter block size");
        input_.resize(block_,0);output_.resize(block_,0);
        // The selected vDSP backend creates FFT state lazily. Prepare using
        // non-silence on the worker; reset before accepting the real stream.
        for (float warmRatio : {.5f,1.f,2.f}) {
            engine_->setPitchScale(warmRatio);
            for (std::size_t pos=0;pos<std::size_t(rate/4);pos+=block_) {
                for (std::size_t i=0;i<block_;++i) input_[i]=float(.01*std::sin(6.283185307179586*440*(pos+i)/rate));
                const float* in[]={input_.data()};float* out[]={output_.data()};engine_->shift(in,out);
            }
        }
        engine_->setPitchScale(ratio);engine_->reset();
        std::fill(input_.begin(),input_.end(),0.f);
        // Reset leaves warm-up overlap in the selected upstream implementation.
        // Drain it on the worker and finish first-process resampler setup before
        // callback-side ratio changes can enter its allocating startup branch.
        for (std::size_t pos=0;pos<std::size_t(rate/4);pos+=block_) {
            const float* in[]={input_.data()};float* out[]={output_.data()};engine_->shift(in,out);
        }
        delay_=engine_->getStartDelay();
        std::fill(output_.begin(),output_.end(),0.f);
        ratio_=blockRatio_=ratio;
    }
    static bool validRatio(float ratio) { return std::isfinite(ratio) && ratio>=.5f && ratio<=2.f; }
    bool process(const float* input,float* output,std::size_t frames,float ratio) noexcept {
        if (!output) return false;
        if (!input || frames>maxBlock_ || !validRatio(ratio)) {
            std::fill_n(output,frames,0.f);return false;
        }
        bool finite=true;
        for (std::size_t i=0;i<frames;++i) {
            if (position_==0) blockRatio_=ratio;
            input_[position_]=std::isfinite(input[i]) ? input[i] : 0.f;
            output[i]=output_[position_]; // One complete internal block of adapter delay.
            if (!std::isfinite(output[i])) { output[i]=0;finite=false; }
            if (++position_==block_) {
                if (blockRatio_!=ratio_) { engine_->setPitchScale(blockRatio_);ratio_=blockRatio_; }
                const float* in[]={input_.data()};float* out[]={output_.data()};
                engine_->shift(in,out);position_=0;
            }
        }
        return finite;
    }
    std::size_t internalBlockSize() const { return block_; }
    std::size_t startDelay() const { return delay_; }
    std::size_t nominalLatencySamples() const { return block_+delay_; } // Initial ratio only.
private:
    std::unique_ptr<RB> engine_;
    std::vector<float> input_,output_;
    std::size_t maxBlock_,block_=0,delay_=0,position_=0;
    float ratio_=1,blockRatio_=1;
};
}
