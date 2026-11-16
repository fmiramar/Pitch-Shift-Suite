#include "dsp/SubbandPhaseHalf.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <vector>
static std::atomic<std::size_t> allocations{0};
void* operator new(std::size_t n) { ++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
#define CHECK(x) do { if(!(x))throw std::runtime_error(#x); } while(false)
constexpr double tau=6.2831853071795864769;
namespace pitch {
struct SubbandPhaseHalfProbe {
    static void equations() {
        using Tracker=SubbandPhaseHalf::Tracker;
        double maximumError=0,maximumStep=0;Tracker tracker;double previous=0;
        for(int n=0;n<1000000;++n) {
            double phase=.2+tau*440*n/48000;
            auto z=std::polar(.1,phase);
            double y=tracker.process(z,tau*440/48000,std::exp(-1./2400),1./48);
            if(n>100) {
                maximumError=std::max(maximumError,std::abs(y-.1*std::cos(phase/2)));
                maximumStep=std::max(maximumStep,std::abs(y-previous));
            }
            CHECK(std::abs(tracker.half)<=tau/2+1e-12);previous=y;
        }
        CHECK(maximumError<1e-8 && maximumStep<.003);
        Tracker jump;
        jump.process(std::polar(1.,.4),.1,.999,1.);
        CHECK(std::abs(jump.process(std::polar(1.,-2.),.1,.999,1.)-std::cos(-1.))<1e-14);
        Tracker gap;double recovery=0;
        for(int n=0;n<12000;++n) {
            double phase=.2+tau*440*n/48000;
            auto z=n>=4000 && n<8000 ? std::complex<double>{}:std::polar(.1,phase);
            double y=gap.process(z,tau*440/48000,std::exp(-1./2400),1./48);
            if(n>=4000 && n<8000)CHECK(y==0);
            if(n>8100)recovery=std::max(recovery,std::abs(y-.1*std::cos(phase/2)));
        }
        CHECK(recovery<1e-10);
        Tracker tiny;CHECK(tiny.process({1e-20,1e-20},.1,.999,1.)==0);
        tiny.peak=1e-31;CHECK(tiny.process({},.1,.999,1.)==0 && tiny.peak==0);
        Tracker anchor;anchor.half=2;anchor.reliable=true;Tracker joining;
        joining.process(std::polar(1.,-.4),.1,.999,1.,&anchor);
        CHECK(std::cos(joining.half-anchor.half)>0);
        std::cout<<"analytic million-sample error="<<maximumError<<" max step="<<maximumStep<<" matched-center gap error="<<recovery<<"; bounded half-phase, null/initial anchor and negative-increment checks passed\n";
    }
};
}
static std::vector<float> render(const std::vector<float>& in,int rate,int block=64) {
    pitch::SubbandPhaseHalf e(rate,4096);std::vector<float> out(in.size());auto before=allocations.load();
    for(std::size_t pos=0;pos<in.size();) {
        auto count=std::min<std::size_t>(block ? block : 1+pos%257,in.size()-pos);
        CHECK(e.process(in.data()+pos,out.data()+pos,count));pos+=count;
    }
    CHECK(allocations.load()==before);for(float x:out)CHECK(std::isfinite(x) && std::abs(x)<2);
    return out;
}
int main(int argc,char** argv) try {
    if(argc==5) {
        std::ifstream file(argv[1],std::ios::binary|std::ios::ate);CHECK(file.good());
        std::vector<float> in(std::size_t(file.tellg())/sizeof(float));file.seekg(0);
        file.read(reinterpret_cast<char*>(in.data()),in.size()*sizeof(float));
        auto out=render(in,std::stoi(argv[3]),std::stoi(argv[4]));
        std::ofstream dest(argv[2],std::ios::binary);dest.write(reinterpret_cast<char*>(out.data()),out.size()*sizeof(float));CHECK(dest.good());return 0;
    }
    pitch::SubbandPhaseHalfProbe::equations();
    for(int rate:{44100,48000,96000})for(int hz:{82,110,220,440,1003,3000}) {
        std::vector<float> in(2*rate);for(int j=0;j<2*rate;++j)in[j]=float(.1*std::sin(tau*hz*j/rate));
        auto start=std::chrono::steady_clock::now();auto out=render(in,rate);
        double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()/2;
        double re=0,im=0,energy=0;
        for(int j=0;j<rate;++j) {double x=out[rate+j];re+=x*std::cos(tau*.5*hz*j/rate);im+=x*std::sin(tau*.5*hz*j/rate);energy+=x*x;}
        double fraction=2*(re*re+im*im)/(rate*energy),amplitude=2*std::hypot(re,im)/rate;
        CHECK(amplitude>.001 && fraction>.95);
        std::cout<<"rate="<<rate<<" input_hz="<<hz<<" half_frequency_fraction="<<fraction<<" amplitude="<<amplitude<<" wall_per_audio="<<wall<<'\n';
    }
    std::vector<float> in(12000);for(std::size_t j=0;j<in.size();++j)in[j]=float(int(j*73%1021)-510)/2048;
    auto out=render(in,48000);CHECK(out==render(in,48000,0));
    auto alias=in;pitch::SubbandPhaseHalf e(48000,257);auto before=allocations.load();
    for(int run=0;run<2;++run) {
        for(std::size_t pos=0;pos<alias.size();pos+=257)CHECK(e.process(alias.data()+pos,alias.data()+pos,std::min<std::size_t>(257,alias.size()-pos)));
        CHECK(alias==out);e.reset();std::copy(in.begin(),in.end(),alias.begin());
    }
    CHECK(allocations.load()==before);
    std::fill(in.begin(),in.end(),0);out=render(in,48000);for(float x:out)CHECK(x==0);
    in.resize(96000);std::fill(in.begin(),in.end(),.1f);out=render(in,48000);
    for(int j=48000;j<96000;++j)CHECK(std::abs(out[j])<1e-9); // DC zero after settling.
    for(int j=0;j<96000;++j)in[j]=float(.1*std::sin(tau*18000*j/48000));out=render(in,48000);
    double energy=0;for(int j=48000;j<96000;++j)energy+=double(out[j])*out[j]/48000;
    double rejection=10*std::log10(energy/.005);CHECK(rejection< -30); // Stopband leakage is not a general anti-aliasing guarantee.
    std::fill(in.begin(),in.end(),0);in[1000]=.5;out=render(in,48000);
    for(int j=0;j<1000;++j)CHECK(out[j]==0);
    double impulseEnergy=0;for(auto x:out)impulseEnergy+=double(x)*x;CHECK(impulseEnergy>1e-5 && impulseEnergy<1);
    // Half phase is branch dependent: polarity inversion is not output invariance.
    float block[64]{};block[1]=std::numeric_limits<float>::infinity();block[2]=std::numeric_limits<float>::quiet_NaN();
    CHECK(e.process(block,block,64));for(auto x:block)CHECK(std::isfinite(x));
    std::cout<<"analytic phase tracking, 18 tones, silence/DC/impulse, reset/partition/in-place/nonfinite passed; high-band total-output attenuation dB="<<rejection<<"; zero observed process/reset C++ new\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
