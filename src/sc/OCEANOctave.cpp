#include "SC_PlugIn.h"
#include "dsp/OCEANOctave.hpp"
#include <new>
static InterfaceTable* ft;
struct ShiftState;
struct OCEANOctave : Unit {
    ShiftState* state;
    pitch::OCEANOctave* engine;
};
struct ShiftJob { ShiftState* state; pitch::OCEANOctave* engine; bool prepare; };
struct ShiftState {
    OCEANOctave* unit; // Accessed only in RT stages, never on worker.
    double rate;
    int block;
    int references; // One for preparation command, one for unit/deletion command; RT only.
    ShiftJob init, dispose;
};
static bool work(World*, void* data) {
    auto* job=static_cast<ShiftJob*>(data);
    if (job->prepare) {
        try { job->engine=new pitch::OCEANOctave(job->state->rate, job->state->block); }
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
static void OCEANOctave_next(OCEANOctave* unit, int frames) {
    if (!unit->engine) { ClearUnitOutputs(unit,frames); return; }
    unit->engine->process(IN(0),OUT(0),std::size_t(frames));
}
static void OCEANOctave_Ctor(OCEANOctave* unit) {
    unit->state=nullptr; unit->engine=nullptr;
    SETCALC(OCEANOctave_next);
    ClearUnitOutputs(unit,1); // Do not advance spectral state during SC's constructor probe.
    if (INRATE(0)!=calc_FullRate) return;
    auto* memory=RTAlloc(unit->mWorld,sizeof(ShiftState));
    if (!memory) return;
    auto* state=new(memory) ShiftState{};
    state->unit=unit; state->rate=SAMPLERATE; state->block=BUFLENGTH; state->references=2;
    state->init={state,nullptr,true}; state->dispose={state,nullptr,false};
    unit->state=state;
    if (DoAsynchronousCommand(unit->mWorld,nullptr,nullptr,&state->init,work,attach,retire,cleanup,0,nullptr)) {
        // Queue failed before ownership transfer. Unit retains its sole reference.
        --state->references;
    }
}
static void OCEANOctave_Dtor(OCEANOctave* unit) {
    auto* state=unit->state;
    if (!state) return;
    state->unit=nullptr;
    if (!unit->engine) { cleanup(unit->mWorld,&state->dispose); return; }
    state->dispose.engine=unit->engine;
    // No heap destruction on audio thread. On RT-pool exhaustion, retain state
    // rather than performing an unsafe heap delete in this callback.
    DoAsynchronousCommand(unit->mWorld,nullptr,nullptr,&state->dispose,work,attach,retire,cleanup,0,nullptr);
}
PluginLoad(OCEANOctave) { ft=inTable; DefineDtorUnit(OCEANOctave); }
