#include "dsp/WSOLAStretchShift.hpp"
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
    int cases=0;
    for (int rate : {44100,48000,96000}) for (double seconds : {.02,.04,.08})
    for (int overlap : {2,4}) for (int window : {0,1}) {
        pitch::WSOLAStretchShift engine(rate,257,seconds,overlap,window);
        std::vector<float> input(rate),output(rate),alias(rate);
        for (int n=0;n<rate;++n) input[n]=float(int((std::size_t(n)*73)%1021)-510)/2048;
        auto before=allocations.load();
        for (std::size_t pos=0;pos<input.size();) {
            auto count=std::min<std::size_t>(1+pos%257,input.size()-pos);
            CHECK(engine.process(input.data()+pos,output.data()+pos,count,1));pos+=count;
        }
        CHECK(allocations.load()==before);
        for (std::size_t n=0;n<input.size();++n)
            CHECK(output[n]==(n<engine.unityDelaySamples() ? 0 : input[n-engine.unityDelaySamples()]));
        pitch::WSOLAStretchShift other(rate,257,seconds,overlap,window);alias=input;
        before=allocations.load();
        for (std::size_t pos=0;pos<alias.size();pos+=257)
            CHECK(other.process(alias.data()+pos,alias.data()+pos,std::min<std::size_t>(257,alias.size()-pos),1));
        CHECK(allocations.load()==before && alias==output);++cases;
    }
    std::cout << cases << " unity configurations sample-exact; wraparound, aliasing and partition equality passed\n";
    for (int rate : {44100,48000,96000}) for (float ratio : {.5f,1.f,1.5f,2.f}) {
        pitch::WSOLAStretchShift engine(rate,64);
        std::vector<float> input(rate*2),output(input.size());
        for (std::size_t n=0;n<input.size();++n) input[n]=float(.1*std::sin(tau*400*n/rate));
        auto before=allocations.load();
        for (std::size_t pos=0;pos<input.size();pos+=64)
            CHECK(engine.process(input.data()+pos,output.data()+pos,std::min<std::size_t>(64,input.size()-pos),ratio));
        CHECK(allocations.load()==before);
        double real=0,imag=0,total=0;
        for (int n=0;n<rate;++n) {
            float x=output[rate+n];CHECK(std::isfinite(x) && std::abs(x)<.2f);
            real+=x*std::cos(tau*400*ratio*n/rate);imag+=x*std::sin(tau*400*ratio*n/rate);total+=x*x;
        }
        double power=(real*real+imag*imag)/(double(rate)*rate);
        CHECK(power>1e-5 && power>total/rate*.4);
        std::cout << rate << " hop-aligned tone 400 ratio " << ratio << " coherent power " << power << '\n';
    }
    pitch::WSOLAStretchShift filtered(48000,64);
    std::vector<float> input(96000),output(input.size());
    for (std::size_t n=0;n<input.size();++n) input[n]=float(.1*std::sin(tau*18000*n/48000));
    auto before=allocations.load();
    for (std::size_t pos=0;pos<input.size();pos+=64) CHECK(filtered.process(input.data()+pos,output.data()+pos,64,2));
    CHECK(allocations.load()==before);
    double power=0;for (int n=48000;n<96000;++n) power+=double(output[n])*output[n]/48000;
    double rejection=10*std::log10(power/.005);
    CHECK(rejection < -40);
    std::cout << "18 kHz octave-up alias probe total output/input energy " << rejection << " dB\n";
    for (int cycle=0;cycle<8;++cycle) {
        pitch::WSOLAStretchShift engine(48000,64);float block[64]{};
        before=allocations.load();
        for (int n=0;n<1800;++n) {
            float ratio=n<600 ? .5f : n<1200 ? 2.f : 1.f;
            CHECK(engine.process(block,block,64,ratio));
            for (float x:block) CHECK(x==0);
        }
        CHECK(allocations.load()==before);
        for (float bad : {0.f,3.f,std::numeric_limits<float>::quiet_NaN()}) CHECK(!engine.process(block,block,64,bad));
        std::fill_n(block,64,.2f);float previous=.2f;
        before=allocations.load();
        for (int n=0;n<1800;++n) {
            std::fill_n(block,64,.2f);CHECK(engine.process(block,block,64,n<900 ? .5f : 2.f));
            if (n>300) for (float x:block) { CHECK(std::abs(x-.2f)<1e-6f);CHECK(std::abs(x-previous)<1e-6f);previous=x; }
        }
        CHECK(allocations.load()==before);
    }
    std::cout << "Silence, DC continuity, ratio changes and eight lifecycles passed; zero observed C++ new during process\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n';return 1; }
