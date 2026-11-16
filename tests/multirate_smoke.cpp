#include "dsp/MultirateOctaveShift.hpp"
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
static std::vector<float> conversion(const std::vector<float>& in) {
    pitch::terrarium::Decimator d;pitch::terrarium::Interpolator u;std::array<float,6> x{},y{};
    std::vector<float> out(in.size());int phase=0;
    for(std::size_t j=0;j<in.size();++j) {x[phase]=in[j];out[j]=y[phase];if(++phase==6){y=u(d(x));phase=0;}}
    return out;
}
static std::vector<float> render(const std::vector<float>& in,int voice,int block=64) {
    pitch::MultirateOctaveShift e(48000,4096,voice);std::vector<float> out(in.size());auto before=allocations.load();
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
        int voice=std::stoi(argv[3]);auto out=voice==3 ? conversion(in):render(in,voice,std::stoi(argv[4]));
        std::ofstream dest(argv[2],std::ios::binary);dest.write(reinterpret_cast<char*>(out.data()),out.size()*sizeof(float));CHECK(dest.good());return 0;
    }
    for(float x:{0.f,1e-35f,.5f,1.f,100.f}) {
        CHECK(std::isfinite(pitch::terrarium::fastInvSqrt(x)));
        CHECK(std::isfinite(pitch::terrarium::fastSqrt(x)));
    }
    CHECK(pitch::terrarium::fastSqrt(-.001f)==0);
    bool rejected=false;try{pitch::MultirateOctaveShift bad(44100,64);}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
    for(int voice=0;voice<3;++voice)for(int hz:{80,110,220,440,880,1500}) {
        int rate=48000;double ratio=voice==0 ? 2:voice==1 ? .5:.25;
        std::vector<float> in(2*rate);for(int j=0;j<2*rate;++j)in[j]=float(.1*std::sin(tau*hz*j/rate));
        auto start=std::chrono::steady_clock::now();auto out=render(in,voice);
        double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()/2;
        double re=0,im=0,energy=0;
        for(int j=0;j<rate;++j) {double x=out[rate+j];re+=x*std::cos(tau*ratio*hz*j/rate);im+=x*std::sin(tau*ratio*hz*j/rate);energy+=x*x;}
        double fraction=2*(re*re+im*im)/(rate*energy),amp=2*std::hypot(re,im)/rate;
        std::cout<<"voice="<<voice<<" input_hz="<<hz<<" coherent_fraction="<<fraction<<" amplitude="<<amp<<" wall_per_audio="<<wall<<std::endl;
        CHECK(amp>.001 && fraction>.9);
    }
    for(int voice=0;voice<3;++voice) {
    std::vector<float> in(12000);for(std::size_t j=0;j<in.size();++j)in[j]=float(int(j*73%1021)-510)/2048;
    auto out=render(in,voice);CHECK(out==render(in,voice,0));
    auto alias=in;pitch::MultirateOctaveShift e(48000,257,voice);auto before=allocations.load();
    for(int run=0;run<2;++run) {
        for(std::size_t pos=0;pos<alias.size();pos+=257)CHECK(e.process(alias.data()+pos,alias.data()+pos,std::min<std::size_t>(257,alias.size()-pos)));
        CHECK(alias==out);e.reset();std::copy(in.begin(),in.end(),alias.begin());
    }
    CHECK(allocations.load()==before);
    std::fill(in.begin(),in.end(),0);out=render(in,voice);for(float x:out)CHECK(x==0);
    in.resize(96000);std::fill(in.begin(),in.end(),.1f);out=render(in,voice);
    for(int j=48000;j<96000;++j)CHECK(std::isfinite(out[j])); // Modulated lowpass bank has finite DC leakage.
    for(int j=0;j<96000;++j)in[j]=float(.1*std::sin(tau*18000*j/48000));out=render(in,voice);
    double energy=0;for(int j=48000;j<96000;++j)energy+=double(out[j])*out[j]/48000;
    double rejection=10*std::log10(energy/.005);CHECK(rejection< -40);
    std::fill(in.begin(),in.end(),0);in[1000]=.5;out=render(in,voice);
    for(int j=0;j<1000;++j)CHECK(out[j]==0);
    double impulseEnergy=0;for(auto x:out)impulseEnergy+=double(x)*x;CHECK(impulseEnergy>1e-5 && impulseEnergy<1);
    auto negative=in;for(auto& x:negative)x=-x;auto neg=render(negative,voice);for(float x:neg)CHECK(std::isfinite(x));
    float block[64]{};block[1]=std::numeric_limits<float>::infinity();block[2]=std::numeric_limits<float>::quiet_NaN();
    CHECK(e.process(block,block,64));for(auto x:block)CHECK(std::isfinite(x));
    std::cout<<"voice safety, silence/DC/impulse/polarity, reset/partition/in-place/nonfinite passed; high-band rejection dB="<<rejection<<"; zero observed process/reset C++ new\n";
    }
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
