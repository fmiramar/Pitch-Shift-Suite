#include "dsp/SinusoidalResidualShift.hpp"
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
using Engine=pitch::SinusoidalResidualShift;
constexpr double tau=Engine::tau;
static std::vector<float> render(const std::vector<float>& in,int rate,float ratio,double residual=1,int block=64) {
    Engine e(rate,4096,residual);std::vector<float> out(in.size());auto before=allocations.load();
    for(std::size_t pos=0;pos<in.size();) {
        auto count=std::min<std::size_t>(block ? block:1+pos%257,in.size()-pos);
        CHECK(e.process(in.data()+pos,out.data()+pos,count,ratio));pos+=count;CHECK(e.activeTracks()<=64);
    }
    CHECK(allocations.load()==before);for(float x:out)CHECK(std::isfinite(x) && std::abs(x)<2);
    return out;
}
int main(int argc,char** argv) try {
    if(argc==7) {
        std::ifstream file(argv[1],std::ios::binary|std::ios::ate);CHECK(file.good());
        std::vector<float> in(std::size_t(file.tellg())/sizeof(float));file.seekg(0);file.read(reinterpret_cast<char*>(in.data()),in.size()*sizeof(float));
        auto out=render(in,std::stoi(argv[3]),std::stof(argv[4]),std::stod(argv[5]),std::stoi(argv[6]));
        std::ofstream dest(argv[2],std::ios::binary);dest.write(reinterpret_cast<char*>(out.data()),out.size()*sizeof(float));CHECK(dest.good());return 0;
    }
    for(int j=0;j<Engine::hop;++j) {
        double delta=Engine::phaseIntegral(.7,.1,.3,j+1)-Engine::phaseIntegral(.7,.1,.3,j);
        CHECK(std::abs(delta-(.1+.2*(j+.5)/Engine::hop))<1e-13);
    }
    for(int rate:{44100,48000})for(double hz:{110.,440.,1003.})for(float ratio:{.5f,1.f,1.25f,2.f}) {
        std::vector<float> in(2*rate);for(int j=0;j<2*rate;++j)in[j]=float(.1*std::cos(tau*hz*j/rate));
        auto start=std::chrono::steady_clock::now();auto out=render(in,rate,ratio);
        double re=0,im=0,energy=0,cc=0,ss=0,cs=0;
        for(int j=0;j<rate;++j) {double x=out[rate+j],c=std::cos(tau*ratio*hz*j/rate),s=std::sin(tau*ratio*hz*j/rate);
            re+=x*c;im+=x*s;energy+=x*x;cc+=c*c;ss+=s*s;cs+=c*s;}
        // Exact two-basis projection also handles noninteger cycles in the test interval.
        double determinant=cc*ss-cs*cs,a=(re*ss-im*cs)/determinant,b=(im*cc-re*cs)/determinant;
        double fraction=(a*re+b*im)/energy,amp=std::hypot(a,b);
        double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()/2;
        std::cout<<"rate="<<rate<<" hz="<<hz<<" ratio="<<ratio<<" target_fraction="<<fraction<<" amplitude="<<amp<<" wall_per_audio="<<wall<<std::endl;
        CHECK(fraction>.95 && amp>.08 && amp<.12);
    }
    for(int rate:{44100,48000}) {
        std::vector<float> in(16000);for(std::size_t j=0;j<in.size();++j)in[j]=float(int(j*73%1021)-510)/4096;
        auto expected=render(in,rate,1.37f);CHECK(expected==render(in,rate,1.37f,1,0));
        auto inplace=in;Engine e(rate,257);auto before=allocations.load();
        for(int trial=0;trial<2;++trial) {
            for(std::size_t p=0;p<in.size();p+=257)CHECK(e.process(inplace.data()+p,inplace.data()+p,std::min<std::size_t>(257,in.size()-p),1.37f));
            CHECK(inplace==expected);e.reset();std::copy(in.begin(),in.end(),inplace.begin());
        }
        CHECK(allocations.load()==before);
        std::fill(in.begin(),in.end(),0);for(auto x:render(in,rate,2))CHECK(x==0);
        in[4096]=.5f;auto impulse=render(in,rate,2);double energy=0;for(auto x:impulse)energy+=x*x;CHECK(energy>1e-5 && energy<1);
        for(std::size_t j=0;j<in.size();++j)in[j]=.08f*float(std::cos(tau*440*j/rate));
        auto dry=render(in,rate,1,0);auto full=render(in,rate,1,1);double noise=0,sine=0;
        for(int j=rate/4;j<int(in.size());++j) {noise+=std::pow(full[j]-dry[j],2);sine+=dry[j]*dry[j];}
        CHECK(noise/sine<.001); // Independent stochastic branch nearly vanishes for a stable sinusoid.
        std::cout<<"rate="<<rate<<" steady_sine_residual_dB="<<10*std::log10(noise/sine)<<std::endl;
        float block[257]{};block[0]=std::numeric_limits<float>::quiet_NaN();block[1]=std::numeric_limits<float>::infinity();
        CHECK(e.process(block,block,257,1));for(float x:block)CHECK(std::isfinite(x));
        CHECK(!e.process(block,block,257,3));for(float x:block)CHECK(x==0);
        for(int frame=0;frame<500;++frame) {
            for(int j=0;j<257;++j)block[j]=float(.1*std::sin(tau*440*(frame*257+j)/rate));
            CHECK(e.process(block,block,257,frame%2 ? .5f:2.f));for(float x:block)CHECK(std::isfinite(x) && std::abs(x)<1);
        }
    }
    for(int i=0;i<24;++i){Engine temporary(48000,64);float x[64]{};CHECK(temporary.process(x,x,64,1));}
    bool rejected=false;try{Engine invalid(96000,64);}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
    std::cout<<"phase integral, partition/in-place/reset/seed/silence/impulse/nonfinite/control/lifecycle checks passed; no observed ordinary process/reset C++ allocations\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
