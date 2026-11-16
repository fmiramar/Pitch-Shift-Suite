#include "dsp/SpectralHybridShift.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
static std::atomic<std::size_t> allocations{0};
void* operator new(std::size_t n) {
    ++allocations; if (void* p=std::malloc(n ? n : 1)) return p; throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while(false)
int main() try {
    constexpr double tau=6.283185307179586;
    for (int rate : {44100,48000,96000}) for (float ratio : {.5f,1.f,2.f}) {
        std::vector<float> in(rate*2),out(in.size()),repeat(in.size());
        for (std::size_t i=0;i<in.size();++i) in[i]=.1f*std::sin(tau*440*i/rate);
        for (int pass=0;pass<2;++pass) {
            pitch::SpectralHybridShift engine(rate,64);
            const auto before=allocations.load();
            for (std::size_t p=0;p<in.size();) {
                auto n=std::min<std::size_t>(1+(p%64),in.size()-p);
                CHECK(engine.process(in.data()+p,out.data()+p,n,ratio)); p+=n;
            }
            CHECK(allocations.load()==before);
            int crossings=0;
            for (int i=rate;i<rate*2;++i) {
                CHECK(std::isfinite(out[i]) && std::abs(out[i])<1);
                crossings+=out[i-1]<=0 && out[i]>0;
            }
            CHECK(std::abs(crossings-440*ratio)<=2);
            if (!pass) repeat=out; else CHECK(out==repeat);
            if (!pass) std::cout << rate << " ratio " << ratio << " pitch " << crossings
                                << " latency " << engine.latencySamples() << '\n';
        }
    }
    pitch::SpectralHybridShift engine(48000,64);
    std::vector<float> in(48000),out(in.size()); in[10000]=.5f;
    auto before=allocations.load();
    for (std::size_t p=0;p<in.size();p+=64) CHECK(engine.process(in.data()+p,out.data()+p,64,1));
    CHECK(allocations.load()==before);
    auto peak=std::max_element(out.begin(),out.end(),[](float a,float b){return std::abs(a)<std::abs(b);});
    const int delay=int(peak-out.begin())-10000;
    CHECK(delay==engine.latencySamples());
    std::cout << "impulse measured latency " << delay << '\n';
    for (int cycle=0;cycle<16;++cycle) {
        pitch::SpectralHybridShift instance(48000,64);
        float block[64]{};
        before=allocations.load();
        for (int n=0;n<256;++n) CHECK(instance.process(block,block,64,n<128 ? .5f : 2.f));
        CHECK(allocations.load()==before);
        for (float sample:block) CHECK(std::isfinite(sample) && std::abs(sample)<1e-6f);
        for (float bad : {0.f,-1.f,3.f,std::numeric_limits<float>::quiet_NaN()}) {
            CHECK(!instance.process(block,block,64,bad));
            for (float sample:block) CHECK(sample==0);
        }
    }
    // A polyphonic signal exercises spectral peak allocation capacity.
    for (std::size_t i=0;i<in.size();++i)
        in[i]=float(.04*(std::sin(tau*220*i/48000)+std::sin(tau*277.1826*i/48000)+std::sin(tau*329.6276*i/48000)));
    before=allocations.load();
    for (std::size_t p=0;p<in.size();p+=64) CHECK(engine.process(in.data()+p,out.data()+p,64,1.5f));
    CHECK(allocations.load()==before);
    for (float sample:out) CHECK(std::isfinite(sample) && std::abs(sample)<1);
    std::cout << "Silence, chord, invalid ratios, aliasing, repeatability and 16 lifecycles passed; zero observed C++ new calls during processing\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n';return 1; }
