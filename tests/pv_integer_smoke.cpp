#include "dsp/PVIntegerOctave.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
static std::atomic<std::size_t> allocations{0};
void* operator new(std::size_t n) { ++allocations;if (auto p=std::malloc(n?n:1)) return p;throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while(false)
constexpr double tau=6.2831853071795864769;
namespace pitch {
struct PVIntegerOctaveProbe {
    static void equations(int size) {
        PVIntegerOctave e(44100,257,size);
        std::vector<std::complex<double>> in(size),frequency(size),back(size);
        for (int j=0;j<size;++j) in[j]={std::sin(.37*j),std::cos(.21*j)};
        e.dft(in.data(),frequency.data());e.dft(frequency.data(),back.data(),true);
        double error=0;
        for (int j=0;j<size;++j) error=std::max(error,std::abs(in[j]-back[j]));
        CHECK(error<1e-11);
        for (int k : {0,1,17,size/2,size-1}) {
            std::complex<double> direct{};
            for (int j=0;j<size;++j) direct+=in[j]*std::polar(1.,-tau*k*j/size);
            CHECK(std::abs(direct-frequency[k])<1e-8);
        }
        CHECK(e.doublePhase({})==std::complex<double>{});
        for (double phase : {-3.1,-1.,0.,2.,3.1}) {
            auto actual=e.doublePhase(std::polar(.37,phase));
            CHECK(std::abs(actual-std::polar(.37,2*phase))<1e-15);
        }
        // Centered impulse phase is -Omega*d. Squaring relocates it to 2*d mod N.
        for (int d : {-e.center_+1,-271,0,179,e.center_-1}) {
            std::fill(in.begin(),in.end(),std::complex<double>{});in[e.centeredIndex(d)]=1.;
            e.dft(in.data(),frequency.data());
            for (auto& x:frequency) x=e.doublePhase(x);
            e.dft(frequency.data(),back.data(),true);
            int expected=(2*d%size+size)%size;
            for (int j=0;j<size;++j) CHECK(std::abs(back[j]-(j==expected ? 1.:0.))<1e-11);
        }
        CHECK(e.window_.front()==0 && e.window_.back()==0 && e.window_[e.center_]==1);
        for (int p=0;p<16;++p) {
            double sum=0;
            for (int j=p;j<size;j+=16) sum+=e.window_[j]*e.window_[j];
            CHECK(sum>0); // Reconstruction normalization for this endpoint/hop convention.
        }
        std::cout << "N=" << size << " DFT/direct/roundtrip error=" << error << "; phase-square and centered impulse equations passed\n";
    }
};
}
static std::vector<float> render(const std::vector<float>& input,int rate,int size,int block=64) {
    pitch::PVIntegerOctave e(rate,4096,size);std::vector<float> out(input.size());
    auto before=allocations.load();
    for (std::size_t pos=0;pos<input.size();) {
        auto count=std::min<std::size_t>(block ? block : 1+pos%257,input.size()-pos);
        CHECK(e.process(input.data()+pos,out.data()+pos,count));pos+=count;
    }
    CHECK(allocations.load()==before);
    for (float x:out) CHECK(std::isfinite(x) && std::abs(x)<2);
    return out;
}
int main(int argc,char** argv) try {
    if (argc==6) {
        std::ifstream file(argv[1],std::ios::binary|std::ios::ate);CHECK(file.good());
        std::vector<float> in(std::size_t(file.tellg())/sizeof(float));file.seekg(0);
        file.read(reinterpret_cast<char*>(in.data()),in.size()*sizeof(float));
        auto out=render(in,std::stoi(argv[3]),std::stoi(argv[4]),std::stoi(argv[5]));
        std::ofstream dest(argv[2],std::ios::binary);dest.write(reinterpret_cast<char*>(out.data()),out.size()*sizeof(float));
        CHECK(dest.good());return 0;
    }
    for (int size : {1169,1161}) {
        pitch::PVIntegerOctaveProbe::equations(size);
        for (int rate : {44100,48000,96000}) for (double hz : {110.,440.,1003.}) {
            std::vector<float> in(rate);
            for (int j=0;j<rate;++j) in[j]=float(.1*std::sin(tau*hz*j/rate));
            auto start=std::chrono::steady_clock::now();auto out=render(in,rate,size);
            double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            double re=0,im=0,total=0;int length=rate/2;
            for (int j=0;j<length;++j) {
                double x=out[rate-length+j];re+=x*std::cos(tau*2*hz*j/rate);im+=x*std::sin(tau*2*hz*j/rate);total+=x*x;
            }
            double fraction=2*(re*re+im*im)/(length*total);
            std::cout << "tone N=" << size << " rate=" << rate << " Hz=" << hz << " target_fraction=" << fraction << " render_wall_s_per_audio_s=" << seconds << '\n';
            CHECK(total/length>1e-4 && fraction>.95); // Basic target-tone gate.
            if (fraction<.98) std::cout << "RETAINED_LIMIT: fixed short window fails 98% spectral-purity probe at this rate/tone\n";
        }
        // Determinism, partition invariance, in-place processing and reset/replay.
        std::vector<float> in(8192);
        for (std::size_t j=0;j<in.size();++j) in[j]=float(int(j*73%1021)-510)/2048;
        auto out=render(in,48000,size);CHECK(out==render(in,48000,size,0));
        pitch::PVIntegerOctave e(48000,257,size);auto alias=in;auto before=allocations.load();
        for (int run=0;run<2;++run) {
            for (std::size_t pos=0;pos<alias.size();pos+=257)
                CHECK(e.process(alias.data()+pos,alias.data()+pos,std::min<std::size_t>(257,alias.size()-pos)));
            CHECK(alias==out);e.reset();std::copy(in.begin(),in.end(),alias.begin());
        }
        CHECK(allocations.load()==before);
        // Every source position modulo the analysis hop, with isolated impulses.
        std::fill(in.begin(),in.end(),0);in.resize(16*4096+4096);
        for (int p=0;p<16;++p) in[2048+4096*p+p]=.5f;
        out=render(in,44100,size);
        for (int p=0;p<16;++p) {
            int predicted=2048+4096*p+p+int(e.nominalDelaySamples());
            auto peak=std::max_element(out.begin()+predicted-1000,out.begin()+predicted+1000,
                [](float a,float b){return std::abs(a)<std::abs(b);});
            CHECK(peak-out.begin()==predicted && *peak>.1f);
        }
        for (float dc : {0.f,.1f,-.1f}) {
            std::fill(in.begin(),in.end(),dc);in.resize(8192);out=render(in,44100,size);
            for (std::size_t j=4096;j<out.size();++j) CHECK(std::abs(out[j]-std::abs(dc))<1e-6);
        }
        in.resize(48000);
        for (int j=0;j<48000;++j) in[j]=float(.1*std::sin(tau*18000*j/48000));
        out=render(in,48000,size);double power=0;
        for (int j=24000;j<48000;++j) power+=double(out[j])*out[j]/24000;
        double db=10*std::log10(power/.005);CHECK(db< -50);
        std::cout << "N=" << size << " alias rejection probe dB=" << db << " delay=" << e.nominalDelaySamples() << '\n';
        // Nyquist input is rejected by the decimator, not treated as a pitch mode.
        for (int j=0;j<48000;++j) in[j]=j%2 ? -.1f : .1f;
        out=render(in,48000,size);for (int j=24000;j<48000;++j) CHECK(std::abs(out[j])<1e-3);
        float block[64];std::fill_n(block,64,std::numeric_limits<float>::infinity());
        CHECK(e.process(block,block,64));for (float x:block) CHECK(std::isfinite(x));
    }
    std::cout << "32 hop-position impulses, DC/silence/Nyquist, reset, alias/partition passed; zero observed process/reset C++ new\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n';return 1; }
