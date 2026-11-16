#include "SC_PlugIn.h"
#include "dsp/SinusoidalResidualShift.hpp"
#include <new>
static InterfaceTable* ft;
struct ShiftState;
struct SinusoidalResidualShift : Unit {
    ShiftState* state;
    pitch::SinusoidalResidualShift* engine;
};
struct ShiftJob { ShiftState* state; pitch::SinusoidalResidualShift* engine; bool prepare; };
struct ShiftState {
    SinusoidalResidualShift* unit; // Accessed only in RT stages, never on worker.
    double rate;
    int block;
    double residual;
    int references; // One for preparation command, one for unit/deletion command; RT only.
    ShiftJob init, dispose;
};
static bool work(World*, void* data) {
    auto* job=static_cast<ShiftJob*>(data);
    if (job->prepare) {
        try { job->engine=new pitch::SinusoidalResidualShift(job->state->rate, job->state->block,job->state->residual); }
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
static void SinusoidalResidualShift_next(SinusoidalResidualShift* unit, int frames) {
    if (!unit->engine) { ClearUnitOutputs(unit,frames); return; }
    unit->engine->process(IN(0),OUT(0),std::size_t(frames),IN0(1));
}
static void SinusoidalResidualShift_Ctor(SinusoidalResidualShift* unit) {
    unit->state=nullptr; unit->engine=nullptr;
    SETCALC(SinusoidalResidualShift_next);
    ClearUnitOutputs(unit,1); // Do not advance spectral state during SC's constructor probe.
    if (INRATE(0)!=calc_FullRate || INRATE(1)==calc_FullRate) return;
    if (INRATE(2)!=calc_ScalarRate || !std::isfinite(IN0(2)) || IN0(2)<0.f || IN0(2)>1.f) return;
    auto* memory=RTAlloc(unit->mWorld,sizeof(ShiftState));
    if (!memory) return;
    auto* state=new(memory) ShiftState{};
    state->unit=unit; state->rate=SAMPLERATE; state->block=BUFLENGTH; state->references=2;
    state->residual=IN0(2);
    state->init={state,nullptr,true}; state->dispose={state,nullptr,false};
    unit->state=state;
    if (DoAsynchronousCommand(unit->mWorld,nullptr,nullptr,&state->init,work,attach,retire,cleanup,0,nullptr)) {
        // Queue failed before ownership transfer. Unit retains its sole reference.
        --state->references;
    }
}
static void SinusoidalResidualShift_Dtor(SinusoidalResidualShift* unit) {
    auto* state=unit->state;
    if (!state) return;
    state->unit=nullptr;
    if (!unit->engine) { cleanup(unit->mWorld,&state->dispose); return; }
    state->dispose.engine=unit->engine;
    // No heap destruction on audio thread. On RT-pool exhaustion, retain state
    // rather than performing an unsafe heap delete in this callback.
    DoAsynchronousCommand(unit->mWorld,nullptr,nullptr,&state->dispose,work,attach,retire,cleanup,0,nullptr);
}
PluginLoad(SinusoidalResidualShift) { ft=inTable; DefineDtorUnit(SinusoidalResidualShift); }
