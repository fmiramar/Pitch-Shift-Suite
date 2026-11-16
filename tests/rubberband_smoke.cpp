#include "RubberBandEngine.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>

static std::atomic<std::size_t> allocations{0};
void* operator new(std::size_t n) {
    ++allocations;if (void* p=std::malloc(n ? n : 1)) return p;throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while(false)
int main() try {
    constexpr double tau=6.283185307179586;
    for (int version : {2,3}) for (int rate : {44100,48000,96000}) for (float ratio : {.5f,1.f,2.f}) {
        pitch::RubberBandEngine engine(rate,256,version,ratio);
        CHECK(engine.version()==version);
        std::vector<float> input(rate*3),output(input.size());
        for (std::size_t i=0;i<input.size();++i) input[i]=.1f*std::sin(tau*440*i/rate);
        auto before=allocations.load();std::size_t low=rate,high=0;
        for (std::size_t pos=0;pos<input.size();) {
            auto count=std::min<std::size_t>(1+pos%256,input.size()-pos);
            CHECK(engine.process(input.data()+pos,output.data()+pos,count,ratio));pos+=count;
            low=std::min(low,engine.queuedSamples());high=std::max(high,engine.queuedSamples());
        }
        CHECK(allocations.load()==before);
        int crossings=0;
        for (int i=rate*2;i<rate*3;++i) {
            CHECK(std::isfinite(output[i]) && std::abs(output[i])<1);
            crossings+=output[i-1]<=0 && output[i]>0;
        }
        double bestPower=0;int bestFrequency=0;double meanPower=0;
        for (int i=rate*2;i<rate*3;++i) meanPower+=double(output[i])*output[i]/rate;
        for (int hz=int(440*ratio)-4;hz<=int(440*ratio)+4;++hz) {
            double real=0,imag=0;
            for (int n=0;n<rate;++n) {
                real+=output[rate*2+n]*std::cos(tau*hz*n/rate);
                imag+=output[rate*2+n]*std::sin(tau*hz*n/rate);
            }
            double power=(real*real+imag*imag)/(double(rate)*rate);
            if (power>bestPower) { bestPower=power;bestFrequency=hz; }
        }
        CHECK(std::abs(bestFrequency-440*ratio)<=1 && bestPower>meanPower*.1 && bestPower>1e-6);
        std::cout << "R" << version << " rate " << rate << " ratio " << ratio << " pitch " << bestFrequency << " crossings " << crossings
                  << " queued " << low << ':' << high << " reserve " << engine.reserveSamples()
                  << " preferredPad " << engine.preferredPad() << " startDelay " << engine.startDelay() << '\n';
    }
    for (int version : {2,3}) {
        pitch::RubberBandEngine engine(48000,64,version);
        std::vector<float> input(96000),output(input.size()); input[24000]=.5f;
        auto before=allocations.load();
        for (std::size_t pos=0;pos<input.size();pos+=64) CHECK(engine.process(input.data()+pos,output.data()+pos,64,1));
        CHECK(allocations.load()==before);
        auto peak=std::max_element(output.begin(),output.end(),[](float a,float b){return std::abs(a)<std::abs(b);});
        CHECK(std::abs(*peak)>.01);
        std::cout << "R" << version << " measured unity impulse delay " << (peak-output.begin())-24000 << '\n';
        for (int cycle=0;cycle<8;++cycle) {
            pitch::RubberBandEngine silence(48000,64,version);
            float block[64]{};before=allocations.load();
            for (int n=0;n<1500;++n) CHECK(silence.process(block,block,64,n<500 ? .5f : n<1000 ? 2.f : 1.f));
            CHECK(allocations.load()==before);
            for (float x:block) CHECK(std::isfinite(x) && std::abs(x)<1e-6);
            for (float ratio : {0.f,3.f,std::numeric_limits<float>::quiet_NaN()}) CHECK(!silence.process(block,block,64,ratio));
        }
        // Dense original chord and ratio changes exercise the bounded adapter.
        for (std::size_t i=0;i<input.size();++i) input[i]=float(.04*(std::sin(tau*220*i/48000)+std::sin(tau*277.1826*i/48000)+std::sin(tau*329.6276*i/48000)));
        before=allocations.load();
        for (std::size_t pos=0;pos<input.size();pos+=64)
            CHECK(engine.process(input.data()+pos,output.data()+pos,64,pos<48000 ? 1.5f : .5f));
        CHECK(allocations.load()==before);
        for (float x:output) CHECK(std::isfinite(x) && std::abs(x)<1);
    }
    std::cout << "R2/R3 tone, silence, chord, impulse, variable blocks, aliasing and lifecycle passed; zero observed C++ new in process\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n';return 1; }
