#include "dsp/ERBSSBOctave.hpp"
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
static double center(int k) {return 228.7*(std::pow(10.,(5.+k*8./12.)/21.3)-1)/2;}
namespace pitch {
struct ERBPhaseOctaveProbe {
    static auto coefficient(const ERBPhaseOctave& e,int k) {return std::make_pair(e.filters_[k].pole,e.filters_[k].gain);}
};
struct ERBSSBOctaveProbe {
    static void equations() {
        double maxPhaseError=0,maxRadialError=0;
        for(int rate:{22050,44100,48000,96000,192000}) {
            ERBSSBOctave e(rate,257);ERBPhaseOctave ps(rate,257);
            for(int k=0;k<43;++k) {
                auto c=ERBPhaseOctaveProbe::coefficient(ps,k);
                CHECK(c.first==e.filters_[k].pole && c.second==e.filters_[k].gain);
                CHECK(e.filters_[k].phase==std::complex<double>(1,0));
            }
            // One million recurrence steps, checking within and across renormalizations.
            for(int k:{0,21,42}) {
                auto phase=std::complex<double>(1,0),step=e.filters_[k].step;
                for(int n=1;n<=1000000;++n) {
                    ERBSSBOctave::advance(phase,step,(n&1023)==0);
                    if(n%997==0 || n==1000000) {
                        auto expected=std::polar(1.,tau*center(k)*double(n)/rate);
                        maxPhaseError=std::max(maxPhaseError,std::abs(phase-expected));
                        maxRadialError=std::max(maxRadialError,std::abs(std::abs(phase)-1));
                    }
                }
            }
        }
        CHECK(maxPhaseError<2e-9 && maxRadialError<2e-12);
        std::cout<<"identical A05/A06 coefficients at five rates; oscillator 15 x 1000000 steps max complex error="<<maxPhaseError<<" radial error="<<maxRadialError<<'\n';
    }
};
}
static std::vector<float> render(const std::vector<float>& in,int rate,int block=64) {
    pitch::ERBSSBOctave e(rate,4096);std::vector<float> out(in.size());auto before=allocations.load();
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
    pitch::ERBSSBOctaveProbe::equations();
    std::cout.precision(12);
    for(int rate:{44100,48000,96000})for(int k:{0,16,40})for(double offset:{0.,.23}) {
        double hz=center(k)+offset*(center(k+1)-center(k)),target=hz+center(k);
        std::vector<float> in(2*rate);for(int j=0;j<2*rate;++j)in[j]=float(.1*std::cos(tau*hz*j/rate));
        auto start=std::chrono::steady_clock::now();auto out=render(in,rate);
        double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()/2;
        double re=0,im=0,energy=0;
        for(int j=0;j<rate;++j) {double x=out[rate+j];re+=x*std::cos(tau*target*j/rate);im+=x*std::sin(tau*target*j/rate);energy+=x*x;}
        double fraction=2*(re*re+im*im)/(rate*energy),amp=2*std::hypot(re,im)/rate;
        CHECK(amp>.02 && fraction>.8); // Frequency-translation prediction, not octave-accuracy acceptance.
        std::cout<<"rate="<<rate<<" input_hz="<<hz<<" predicted_hz="<<target<<" predicted_fraction="<<fraction<<" wall_per_audio="<<wall<<'\n';
    }
    std::vector<float> in(12000);for(std::size_t j=0;j<in.size();++j)in[j]=float(int(j*73%1021)-510)/2048;
    auto out=render(in,48000);CHECK(out==render(in,48000,0));
    auto alias=in;pitch::ERBSSBOctave e(48000,257);auto before=allocations.load();
    for(int run=0;run<2;++run) {
        for(std::size_t pos=0;pos<alias.size();pos+=257)CHECK(e.process(alias.data()+pos,alias.data()+pos,std::min<std::size_t>(257,alias.size()-pos)));
        CHECK(alias==out);e.reset();std::copy(in.begin(),in.end(),alias.begin());
    }
    CHECK(allocations.load()==before);
    auto negative=in;for(auto& x:negative)x=-x;auto negOut=render(negative,48000);
    for(std::size_t j=0;j<out.size();++j)CHECK(out[j]==-negOut[j]);
    // SSM is linear (apart from the inaudible state floor), unlike PS2.
    auto second=in,sum=in;
    for(std::size_t j=0;j<in.size();++j) {second[j]=float(.1*std::sin(.071*j));sum[j]=in[j]+second[j];}
    auto secondOut=render(second,48000),sumOut=render(sum,48000);double linearError=0;
    for(std::size_t j=0;j<out.size();++j)linearError=std::max(linearError,std::abs(double(sumOut[j])-out[j]-secondOut[j]));
    CHECK(linearError<5e-8);
    std::fill(in.begin(),in.end(),0);out=render(in,48000);for(float x:out)CHECK(x==0);
    in.resize(96000);std::fill(in.begin(),in.end(),.1f);out=render(in,48000);
    for(int j=48000;j<96000;++j)CHECK(std::abs(out[j])<1e-9);
    for(int j=0;j<96000;++j)in[j]=float(.1*std::sin(tau*18000*j/48000));out=render(in,48000);
    double energy=0;for(int j=48000;j<96000;++j)energy+=double(out[j])*out[j]/48000;
    double rejection=10*std::log10(energy/.005);CHECK(rejection< -60);
    std::fill(in.begin(),in.end(),0);in[1000]=.5;out=render(in,48000);
    for(int j=0;j<1000;++j)CHECK(out[j]==0);
    double impulseEnergy=0;for(auto x:out)impulseEnergy+=double(x)*x;CHECK(impulseEnergy>1e-5 && impulseEnergy<1);
    float block[64]{};block[1]=std::numeric_limits<float>::infinity();block[2]=std::numeric_limits<float>::quiet_NaN();
    CHECK(e.process(block,block,64));for(auto x:block)CHECK(std::isfinite(x));
    std::cout<<"18 translation tones, reset/partition/in-place/silence/DC/impulse/polarity/nonfinite passed; linearity error="<<linearError<<"; high-band rejection dB="<<rejection<<"; zero observed process/reset C++ new\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
