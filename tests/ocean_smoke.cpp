#include "dsp/OCEANOctave.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
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
struct OCEANOctaveProbe {
    static void equations() {
        OCEANOctave e(44100,257);std::vector<std::complex<double>> in(897),freq(897),back(897);
        for(int j=0;j<897;++j)in[j]={std::sin(.37*j),std::cos(.21*j)};
        e.dft(in.data(),freq.data());e.dft(freq.data(),back.data(),true);double error=0;
        for(int j=0;j<897;++j)error=std::max(error,std::abs(back[j]-in[j]));CHECK(error<1e-11);
        for(int k:{0,1,17,448,896}) {
            std::complex<double> sum{};for(int j=0;j<897;++j)sum+=in[j]*std::polar(1.,-tau*k*j/897);
            CHECK(std::abs(sum-freq[k])<1e-8);
        }
        for(int phase:{0,1,224,225,896}) {
            std::fill(e.spectrum_.begin(),e.spectrum_.end(),std::complex<double>{});
            e.spectrum_[0]=-2.;e.spectrum_[8]={.3,.7};e.spectrum_[9]={-.4,.6};e.spectrum_[224]={.2,-.9};
            e.spectrum_[225]=5.;e.spectrum_[448]=9.;e.rotation_=phase;e.translate();
            CHECK(e.shifted_[0]==std::complex<double>(-2,0));
            for(int j:{8,9,224}) {
                auto expected=e.spectrum_[j]*std::polar(1.,tau*j*phase/897);
                CHECK(std::abs(e.shifted_[2*j]-expected)<1e-12);
                CHECK(e.shifted_[897-2*j]==std::conj(e.shifted_[2*j]));
            }
            for(int j=1;j<=448;++j)if(j!=16 && j!=18 && j!=448)CHECK(e.shifted_[j]==std::complex<double>{});
        }
        for(int p=0;p<224;++p) {
            double sum=0;for(int j=p;j<897;j+=224)sum+=e.window_[j]*e.window_[j];
            CHECK(std::abs(sum-1.5)<1e-12);
        }
        e.reset();float block[257]{};int length=2*897*224;auto before=allocations.load();
        for(int pos=0;pos<length;pos+=257) {
            int count=std::min(257,length-pos);CHECK(e.process(block,block,count));
            for(int j=0;j<count;++j)CHECK(block[j]==0);
        }
        CHECK(e.rotation_==1 && allocations.load()==before);
        std::cout << "897-point DFT roundtrip error=" << error << "; bin map/rotation/boundaries, Hann WOLA and two phase-clock cycles passed\n";
    }
};
}
static std::vector<float> render(const std::vector<float>& in,int rate,int block=64) {
    pitch::OCEANOctave e(rate,4096);std::vector<float> out(in.size());auto before=allocations.load();
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
    pitch::OCEANOctaveProbe::equations();
    for(int rate:{44100,48000,96000}) for(int bin:{2,8,9,100}) {
        std::vector<float> in(2*rate);double hz=double(bin)*rate/897;
        for(std::size_t j=0;j<in.size();++j)in[j]=float(.1*std::cos(tau*hz*j/rate));
        auto start=std::chrono::steady_clock::now();auto out=render(in,rate);
        double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()/2;
        double re=0,im=0,energy=0;
        for(int j=0;j<rate;++j) {
            double x=out[rate+j];re+=x*std::cos(tau*2*hz*j/rate);im+=x*std::sin(tau*2*hz*j/rate);energy+=x*x;
        }
        double fraction=2*(re*re+im*im)/(rate*energy);
        CHECK(energy/rate>1e-4 && fraction>.8); // Diagnostic modulation/sidebands remain.
        std::cout << "rate=" << rate << " bin=" << bin << " octave_coherent_fraction=" << fraction << " render_wall_s_per_audio_s=" << wall << '\n';
    }
    std::vector<float> in(8192);for(std::size_t j=0;j<in.size();++j)in[j]=float(int(j*73%1021)-510)/2048;
    auto out=render(in,48000);CHECK(out==render(in,48000,0));
    auto alias=in;pitch::OCEANOctave e(48000,257);auto before=allocations.load();
    for(int run=0;run<2;++run) {
        for(std::size_t pos=0;pos<alias.size();pos+=257)CHECK(e.process(alias.data()+pos,alias.data()+pos,std::min<std::size_t>(257,alias.size()-pos)));
        CHECK(alias==out);e.reset();std::copy(in.begin(),in.end(),alias.begin());
    }
    CHECK(allocations.load()==before);
    // Sweep all 224 impulse positions in separate engines: absolute phase epoch matters.
    for(int p=0;p<224;++p) {
        std::fill(in.begin(),in.end(),0);in[4096+p]=.5f;out=render(in,44100);
        double energy=0;for(float x:out)energy+=double(x)*x;CHECK(energy>1e-4 && energy<1);
        int first=int(std::max_element(out.begin(),out.end(),[](float a,float b){return std::abs(a)<std::abs(b);})-out.begin());
        double primary=std::abs(out[first]);double second=0;
        for(int j=0;j<int(out.size());++j)if(std::abs(j-first)>100)second=std::max(second,double(std::abs(out[j])));
        CHECK(second>primary*.1);
    }
    in.resize(48000);
    for(int j=0;j<48000;++j)in[j]=float(.1*std::sin(tau*18000*j/48000));out=render(in,48000);
    double power=0;for(int j=24000;j<48000;++j)power+=double(out[j])*out[j]/24000;
    double rejection=10*std::log10(power/.005);CHECK(rejection< -60);
    for(int j=0;j<48000;++j)in[j]=j%2 ? -.1f:.1f;out=render(in,48000);
    for(int j=24000;j<48000;++j)CHECK(std::abs(out[j])<1e-5);
    std::fill(in.begin(),in.end(),.1f);out=render(in,48000);
    for(auto& x:in)x=-x;auto negative=render(in,48000);
    for(std::size_t j=0;j<out.size();++j)CHECK(out[j]==-negative[j]); // Signed DC; gain modulation is intentional baseline behavior.
    float block[64];std::fill_n(block,64,std::numeric_limits<float>::infinity());CHECK(e.process(block,block,64));
    std::cout << "224 impulse positions retain multiple lobes; partition/reset/in-place/DC/Nyquist/nonfinite passed; high-band rejection dB=" << rejection << "; zero observed process/reset C++ new\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
