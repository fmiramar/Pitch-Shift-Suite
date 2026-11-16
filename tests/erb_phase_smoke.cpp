#include "dsp/ERBPhaseOctave.hpp"
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
struct ERBPhaseOctaveProbe {
    static void equations() {
        using C=std::complex<double>;
        CHECK(ERBPhaseOctave::phaseDoubleReal({})==0);
        CHECK(ERBPhaseOctave::phaseDoubleReal({1e-40,1e-40})==0);
        for(double amp:{1e-20,.1,1.,1e20})for(int j=0;j<1000;++j) {
            double phase=j*.1;auto z=std::polar(amp,phase);
            CHECK(std::abs(ERBPhaseOctave::phaseDoubleReal(z)/amp-std::cos(2*phase))<1e-14);
        }
        for(int rate:{22050,44100,48000,96000,192000}) {
            ERBPhaseOctave e(rate,257);
            for(int k=0;k<43;++k) {
                auto& f=e.filters_[k];CHECK(std::abs(f.pole)<1 && f.pole.imag()>0);
                double fc=228.7*(std::pow(10.,(5.+k*4./6.)/21.3)-1)/2;
                auto z=std::polar(1.,-tau*fc/rate);
                auto h=f.gain*(1.-z*z)/std::pow(1.-f.pole*z,2);
                CHECK(std::abs(std::abs(h)-1)<1e-12);
                auto image=f.gain*(1.-std::conj(z*z))/std::pow(1.-f.pole*std::conj(z),2);
                CHECK(std::abs(image)<.001); // >60 dB center image rejection, not whole-band claim.
            }
        }
    }
};
}
static std::vector<float> render(const std::vector<float>& in,int rate,int block=64) {
    pitch::ERBPhaseOctave e(rate,4096);std::vector<float> out(in.size());auto before=allocations.load();
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
    pitch::ERBPhaseOctaveProbe::equations();
    for(int rate:{44100,48000,96000})for(int hz:{82,110,220,440,1003,3000}) {
        std::vector<float> in(2*rate);for(int j=0;j<2*rate;++j)in[j]=float(.1*std::sin(tau*hz*j/rate));
        auto start=std::chrono::steady_clock::now();auto out=render(in,rate);
        double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()/2;
        double re=0,im=0,energy=0;
        for(int j=0;j<rate;++j) {double x=out[rate+j];re+=x*std::cos(tau*2*hz*j/rate);im+=x*std::sin(tau*2*hz*j/rate);energy+=x*x;}
        double fraction=2*(re*re+im*im)/(rate*energy),amplitude=2*std::hypot(re,im)/rate;
        CHECK(amplitude>.001 && fraction>.98);
        std::cout<<"rate="<<rate<<" input_hz="<<hz<<" octave_fraction="<<fraction<<" amplitude="<<amplitude<<" wall_per_audio="<<wall<<'\n';
    }
    std::vector<float> in(12000);for(std::size_t j=0;j<in.size();++j)in[j]=float(int(j*73%1021)-510)/2048;
    auto out=render(in,48000);CHECK(out==render(in,48000,0));
    auto alias=in;pitch::ERBPhaseOctave e(48000,257);auto before=allocations.load();
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
    double rejection=10*std::log10(energy/.005);CHECK(rejection< -50);
    std::fill(in.begin(),in.end(),0);in[1000]=.5;out=render(in,48000);
    for(int j=0;j<1000;++j)CHECK(out[j]==0);
    double impulseEnergy=0;for(auto x:out)impulseEnergy+=double(x)*x;CHECK(impulseEnergy>1e-5 && impulseEnergy<1);
    auto negative=in;for(auto& x:negative)x=-x;CHECK(out==render(negative,48000)); // PS2 invariant to polarity.
    float block[64]{};block[1]=std::numeric_limits<float>::infinity();block[2]=std::numeric_limits<float>::quiet_NaN();
    CHECK(e.process(block,block,64));for(auto x:block)CHECK(std::isfinite(x));
    std::cout<<"analytic equations/stability/normalization, 18 tones, silence/DC/impulse/polarity, reset/partition/in-place/nonfinite passed; high-band rejection dB="<<rejection<<"; zero observed process/reset C++ new\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
