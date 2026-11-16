#include "SC_PlugIn.h"
#include "dsp/LowLatencySpectralShift.hpp"
#include <new>
static InterfaceTable* ft;
struct ShiftState;
struct LowLatencySpectralShift : Unit {
    ShiftState* state;
    pitch::LowLatencySpectralShift* engine;
};
struct ShiftJob { ShiftState* state; pitch::LowLatencySpectralShift* engine; bool prepare; };
struct ShiftState {
    LowLatencySpectralShift* unit; // Accessed only in RT stages, never on worker.
    double rate;
    int block;
    float ratio;
    int references; // One for preparation command, one for unit/deletion command; RT only.
    ShiftJob init, dispose;
};
static bool work(World*, void* data) {
    auto* job=static_cast<ShiftJob*>(data);
    if (job->prepare) {
        try { job->engine=new pitch::LowLatencySpectralShift(job->state->rate, job->state->block,job->state->ratio); }
        catch (...) { job->engine=nullptr; }
    } else { delete job->engine; job->engine=nullptr; }
    return true;
}
static bool attach(World*, void* data) {
    auto* job=static_cast<ShiftJob*>(data);
    if (job->prepare && job->state->unit) {
        job->state->unit->engine=job->engine;
        job->engine=nullptr;
    }
    return true;
}
static bool retire(World*, void* data) {
    auto* job=static_cast<ShiftJob*>(data);
    // Preparation may finish after its unit was freed. Delete orphan on worker.
    delete job->engine; job->engine=nullptr;
    return false;
}
static void cleanup(World* world, void* data) {
    auto* state=static_cast<ShiftJob*>(data)->state;
    if (--state->references==0) { state->~ShiftState(); RTFree(world,state); }
}
static void LowLatencySpectralShift_next(LowLatencySpectralShift* unit, int frames) {
    if (!unit->engine) { ClearUnitOutputs(unit,frames); return; }
    unit->engine->process(IN(0),OUT(0),std::size_t(frames));
}
static void LowLatencySpectralShift_Ctor(LowLatencySpectralShift* unit) {
    unit->state=nullptr; unit->engine=nullptr;
    SETCALC(LowLatencySpectralShift_next);
    ClearUnitOutputs(unit,1); // Do not advance DSP state during SC's constructor probe.
    if (INRATE(0)!=calc_FullRate || INRATE(1)!=calc_ScalarRate) return;
    if ((SAMPLERATE!=44100 && SAMPLERATE!=48000) || !std::isfinite(IN0(1)) || IN0(1)<.5f || IN0(1)>2.f) return;
    auto* memory=RTAlloc(unit->mWorld,sizeof(ShiftState));
    if (!memory) return;
    auto* state=new(memory) ShiftState{};
    state->unit=unit; state->rate=SAMPLERATE; state->block=BUFLENGTH; state->references=2;
    state->ratio=IN0(1);
    state->init={state,nullptr,true}; state->dispose={state,nullptr,false};
    unit->state=state;
    if (DoAsynchronousCommand(unit->mWorld,nullptr,nullptr,&state->init,work,attach,retire,cleanup,0,nullptr)) {
        // Queue failed before ownership transfer. Unit retains its sole reference.
        --state->references;
    }
}
static void LowLatencySpectralShift_Dtor(LowLatencySpectralShift* unit) {
    auto* state=unit->state;
    if (!state) return;
    state->unit=nullptr;
    if (!unit->engine) { cleanup(unit->mWorld,&state->dispose); return; }
    state->dispose.engine=unit->engine;
    // No heap destruction on audio thread. On RT-pool exhaustion, retain state
    // rather than performing an unsafe heap delete in this callback.
    DoAsynchronousCommand(unit->mWorld,nullptr,nullptr,&state->dispose,work,attach,retire,cleanup,0,nullptr);
}
PluginLoad(LowLatencySpectralShift) { ft=inTable; DefineDtorUnit(LowLatencySpectralShift); }
