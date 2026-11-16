#include "dsp/LowLatencySpectralShift.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
static std::atomic<std::size_t> allocations{0};
void* operator new(std::size_t n) {++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
#define CHECK(x) do {if(!(x))throw std::runtime_error(#x);}while(false)
using Engine=pitch::LowLatencySpectralShift;
static std::vector<float> render(const std::vector<float>& in,int rate,float ratio,int block=64) {
    Engine e(rate,4096,ratio);std::vector<float> out(in.size());auto before=allocations.load();
    CHECK(e.minimumDemodulationWeight()>.1);
    for(std::size_t p=0;p<in.size();) {
        auto count=std::min<std::size_t>(block ? block:1+p%257,in.size()-p);
        CHECK(e.process(in.data()+p,out.data()+p,count));p+=count;
    }
    CHECK(allocations.load()==before);for(float x:out)CHECK(std::isfinite(x) && std::abs(x)<4);
    return out;
}
int main(int argc,char** argv) try {
    if(argc==6) {
        std::ifstream file(argv[1],std::ios::binary|std::ios::ate);CHECK(file.good());
        std::vector<float> in(std::size_t(file.tellg())/sizeof(float));file.seekg(0);file.read(reinterpret_cast<char*>(in.data()),in.size()*sizeof(float));
        auto out=render(in,std::stoi(argv[3]),std::stof(argv[4]),std::stoi(argv[5]));
        std::ofstream dest(argv[2],std::ios::binary);dest.write(reinterpret_cast<char*>(out.data()),out.size()*sizeof(float));CHECK(dest.good());return 0;
    }
    for(int rate:{44100,48000}) {
        std::vector<float> in(rate);for(int j=0;j<rate;++j)in[j]=float(int(j*73%1021)-510)/4096;
        auto unity=render(in,rate,1);
        for(int j=512;j<rate;++j)CHECK(std::abs(unity[j]-in[j-512])<1e-6);
        for(float ratio:{.5f,1.f,1.25f,1.37f,2.f}) {
            auto expected=render(in,rate,ratio);CHECK(expected==render(in,rate,ratio,0));
            Engine e(rate,257,ratio);auto inplace=in;auto before=allocations.load();
            for(int trial=0;trial<2;++trial) {
                for(std::size_t p=0;p<in.size();p+=257)CHECK(e.process(inplace.data()+p,inplace.data()+p,std::min<std::size_t>(257,in.size()-p)));
                CHECK(inplace==expected);e.reset();std::copy(in.begin(),in.end(),inplace.begin());
            }
            CHECK(allocations.load()==before);
        }
        std::fill(in.begin(),in.end(),0);for(float x:render(in,rate,2))CHECK(x==0);
        for(int offset:{4096,4097,4127,4159}) {
            in[offset]=.5f;auto out=render(in,rate,1);
            CHECK(std::abs(out[offset+512]-.5f)<1e-6);out[offset+512]=0;
            for(float x:out)CHECK(std::abs(x)<1e-7);in[offset]=0;
        }
        // Exact bin-centered tones: rounding is absent. Off-bin quality is measured separately.
        for(float ratio:{.5f,1.f,1.25f,2.f}) {
            const double hz=rate*8./512;
            for(int j=0;j<rate;++j)in[j]=float(.1*std::cos(Engine::tau*hz*j/rate));
            auto start=std::chrono::steady_clock::now();auto out=render(in,rate,ratio);
            double re=0,im=0,energy=0;constexpr int count=32768;
            for(int j=0;j<count;++j) {
                double x=out[rate-count+j],angle=Engine::tau*hz*ratio*j/rate;
                re+=x*std::cos(angle);im+=x*std::sin(angle);energy+=x*x;
            }
            double fraction=2*(re*re+im*im)/(count*energy),amplitude=2*std::hypot(re,im)/count;
            std::cout<<"rate="<<rate<<" ratio="<<ratio<<" target_fraction="<<fraction<<" amplitude="<<amplitude
                     <<" wall_per_audio="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<std::endl;
            CHECK(fraction>.9 && amplitude>.07 && amplitude<.13);
        }
    }
    double minWeight=100;
    for(int r=0;r<=150;++r){Engine e(48000,64,.5f+r*.01f);minWeight=std::min(minWeight,e.minimumDemodulationWeight());}
    CHECK(minWeight>.1);std::cout<<"sampled_minimum_demodulation_weight="<<minWeight<<std::endl;
    for(int i=0;i<32;++i){Engine e(48000,64,.5f+i/32.f);float x[64]{};CHECK(e.process(x,x,64));}
    Engine e(48000,64);float x[65]{};x[0]=std::numeric_limits<float>::quiet_NaN();x[1]=std::numeric_limits<float>::infinity();
    for(int i=0;i<16;++i){CHECK(e.process(x,x,64));for(float value:x)CHECK(std::isfinite(value));}
    CHECK(!e.process(x,x,65));for(float value:x)CHECK(value==0);
    for(float ratio:{.49f,2.01f,std::numeric_limits<float>::quiet_NaN()}) {
        bool rejected=false;try{Engine invalid(48000,64,ratio);}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
    }
    bool rejected=false;try{Engine invalid(96000,64);}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
    std::cout<<"low_latency_smoke passed; ordinary callback new/new[] count=0; unity delay=512 samples"<<std::endl;
    return 0;
} catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
