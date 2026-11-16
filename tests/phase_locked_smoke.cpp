#include "dsp/PhaseLockedShift.hpp"
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>

static std::atomic<std::size_t> allocations{0};
void* operator new(std::size_t n) { ++allocations; if (auto p=std::malloc(n?n:1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while(false)
constexpr double tau=6.2831853071795864769;

namespace pitch {
struct PhaseLockedShiftProbe {
    static void equations() {
        for (int mode : {0,1,2}) {
            PhaseLockedShift e(48000,64,512,mode);
            auto frame=[&](int peak,double phase) {
                std::fill(e.spectrum_.begin(),e.spectrum_.end(),std::complex<double>{});
                // Vertical phases cross the -pi/pi boundary; adjacent deltas .9 rad.
                for (int j=-3;j<=3;++j) e.spectrum_[peak+j]=std::polar(4.-std::abs(j),phase+.9*j);
                e.updatePhases(1.4);
            };
            frame(20,2.9);
            double old=e.synthesis_[20];
            frame(21,-2.2); // A migrating peak with previous owner 20.
            CHECK(e.previousOwner_[21]==21);
            if (mode==2) {
                double expected=tau*21*64/512;
                double advance=expected+e.principal(-2.2-2.9-expected);
                CHECK(std::abs(e.principal(e.synthesis_[21]-old-1.4*advance))<1e-12);
            }
            if (mode) for (int j=-3;j<=3;++j) {
                double beta=mode==2 ? 3.4/3 : 1;
                CHECK(std::abs(e.principal(e.synthesis_[21+j]-e.synthesis_[21]-beta*.9*j))<1e-12);
            }
            // A silent frame clears previous ownership; a subsequent peak is unmatched.
            std::fill(e.spectrum_.begin(),e.spectrum_.end(),std::complex<double>{});
            e.updatePhases(1.4); frame(80,.7);
            CHECK(std::abs(e.synthesis_[80]-.7)<1e-12);
            for (auto x:e.spectrum_) CHECK(std::isfinite(x.real()) && std::isfinite(x.imag()));
            // Split then merge: midpoint ties choose lower peak; both split peaks may
            // inherit the same previous peak (paper's region correspondence).
            std::fill(e.spectrum_.begin(),e.spectrum_.end(),std::complex<double>{});
            e.spectrum_[76]=std::polar(4.,.3); e.spectrum_[84]=std::polar(4.,.5);
            e.updatePhases(1.4);
            CHECK(e.owner_[80]==76 && e.owner_[81]==84);
            frame(80,.9);
        }
        std::cout << "Phase equations (5),(6),(12),(15),(16), vertical unwrap, migration, silence, split/merge passed\n";
    }
};
}

static std::vector<float> render(const std::vector<float>& input,int rate,int size,int mode,float ratio,int block=64) {
    pitch::PhaseLockedShift e(rate,257,size,mode);
    std::vector<float> out(input.size());
    auto before=allocations.load();
    for (std::size_t pos=0;pos<input.size();) {
        auto count=std::min<std::size_t>(block ? block : 1+pos%257,input.size()-pos);
        CHECK(e.process(input.data()+pos,out.data()+pos,count,ratio));pos+=count;
    }
    CHECK(allocations.load()==before);
    for (float x:out) CHECK(std::isfinite(x) && std::abs(x)<4);
    return out;
}
int main(int argc,char** argv) try {
    // Small raw-float bridge for direct standalone/SC equality, not a new harness.
    if (argc==6) {
        std::ifstream file(argv[1],std::ios::binary|std::ios::ate); CHECK(file.good());
        std::vector<float> in(std::size_t(file.tellg())/sizeof(float));file.seekg(0);
        file.read(reinterpret_cast<char*>(in.data()),in.size()*sizeof(float));
        auto out=render(in,48000,2048,std::stoi(argv[3]),std::stof(argv[4]),std::stoi(argv[5]));
        std::ofstream dest(argv[2],std::ios::binary);dest.write(reinterpret_cast<char*>(out.data()),out.size()*sizeof(float));
        CHECK(dest.good());return 0;
    }
    pitch::PhaseLockedShiftProbe::equations();
    double worstUnity=0;
    for (int size : {512,1024,2048,4096}) for (int mode : {0,1,2}) {
        std::vector<float> input(6*size);
        for (std::size_t n=0;n<input.size();++n) input[n]=float(int(n*73%1021)-510)/2048;
        auto out=render(input,48000,size,mode,1);
        auto partition=render(input,48000,size,mode,1,0); CHECK(out==partition);
        for (std::size_t n=0;n<input.size();++n) {
            double expected=n<std::size_t(3*size/2) ? 0 : input[n-3*size/2];
            worstUnity=std::max(worstUnity,std::abs(out[n]-expected));
        }
    }
    CHECK(worstUnity<1e-6);
    std::cout << "12 FFT/mode unity reconstructions worst error " << worstUnity << "; partition exact\n";
    for (int rate : {44100,48000,96000}) for (int mode : {0,1,2}) for (float ratio : {.5f,1.f,1.25f,2.f}) {
        std::vector<float> in(rate*2);
        for (std::size_t n=0;n<in.size();++n) in[n]=float(.1*std::sin(tau*440*n/rate));
        auto out=render(in,rate,2048,mode,ratio);
        double re=0,im=0,total=0;
        for (int n=0;n<rate;++n) {
            double x=out[rate+n];re+=x*std::cos(tau*440*ratio*n/rate);im+=x*std::sin(tau*440*ratio*n/rate);total+=x*x;
        }
        double fraction=2*(re*re+im*im)/(rate*total);
        std::cout << "tone rate=" << rate << " mode=" << mode << " ratio=" << ratio << " target_fraction=" << fraction << '\n';
        CHECK(total/rate>1e-4 && fraction>.95);
    }
    // All 64 positions within the 512-point configuration's hop, with separated impulses.
    for (int mode : {0,1,2}) for (float ratio : {.5f,1.f,2.f}) {
        std::vector<float> in(64*3072+4096);
        for (int p=0;p<64;++p) in[2048+p*3072+p]=.5f;
        auto out=render(in,48000,512,mode,ratio);
        for (int p=0;p<64;++p) {
            int center=2048+p*3072+p+768; double energy=0;
            for (int i=center-768;i<center+768;++i) energy+=double(out[i])*out[i];
            CHECK(energy>1e-7 && energy<2);
            if (ratio==1) CHECK(std::abs(out[center]-.5)<1e-6);
        }
    }
    std::cout << "576 impulse positions/modes/ratios retained energy; unity delay exact\n";
    for (int mode : {0,1,2}) {
        std::vector<float> in(96000);
        for (std::size_t n=0;n<in.size();++n) in[n]=float(.1*std::sin(tau*18000*n/48000));
        auto out=render(in,48000,2048,mode,2);double power=0;
        for (int i=48000;i<96000;++i) power+=double(out[i])*out[i]/48000;
        double rejection=10*std::log10(power/.005);
        std::cout << "alias mode=" << mode << " output/input dB=" << rejection << '\n';CHECK(rejection < -40);
        pitch::PhaseLockedShift e(48000,64,2048,mode);float block[64]{};
        auto before=allocations.load();
        for (int n=0;n<300;++n) {CHECK(e.process(block,block,64,n%2 ? .5f:2.f));for (float x:block) CHECK(x==0);}
        for (float bad : {0.f,3.f,std::numeric_limits<float>::quiet_NaN()}) CHECK(!e.process(block,block,64,bad));
        for (int n=0;n<1500;++n) {
            for (int k=0;k<64;++k) block[k]=float(.1*std::sin(tau*440*(n*64+k)/48000));
            CHECK(e.process(block,block,64,n<500 ? .5f : n<1000 ? 2.f : 1.f));
            for (float x:block) CHECK(std::isfinite(x) && std::abs(x)<.5);
        }
        std::fill_n(block,64,std::numeric_limits<float>::infinity());CHECK(e.process(block,block,64,1));
        CHECK(allocations.load()==before);
        // DC and Nyquist at unity: endpoint policy plus WOLA, no spectral bypass.
        for (bool nyquist : {false,true}) {
            std::fill(in.begin(),in.end(),.1f);
            if (nyquist) for (std::size_t n=1;n<in.size();n+=2) in[n]=-.1f;
            out=render(in,48000,2048,mode,1);
            for (std::size_t n=3072;n<out.size();++n) CHECK(std::abs(out[n]-in[n-3072])<1e-6);
        }
    }
    std::cout << "DC/Nyquist, silence, invalid/nonfinite input, in-place automation passed; zero observed process C++ new\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n';return 1; }
