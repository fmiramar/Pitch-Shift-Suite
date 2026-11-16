#include "SC_PlugIn.h"
#include "RubberBandEngine.hpp"
#include <new>
static InterfaceTable* ft;
struct ShiftState;
struct RubberBandUnit : Unit {
    ShiftState* state;
    pitch::RubberBandEngine* engine;
};
struct ShiftJob { ShiftState* state; pitch::RubberBandEngine* engine; bool prepare; };
struct ShiftState {
    RubberBandUnit* unit; // Accessed only in RT stages, never on worker.
    double rate;
    int block;
    int version;
    float ratio;
    int references; // One for preparation command, one for unit/deletion command; RT only.
    ShiftJob init, dispose;
};
static bool work(World*, void* data) {
    auto* job=static_cast<ShiftJob*>(data);
    if (job->prepare) {
        try { job->engine=new pitch::RubberBandEngine(job->state->rate, job->state->block,job->state->version,job->state->ratio); }
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
static void RubberBandUnit_next(RubberBandUnit* unit, int frames) {
    if (!unit->engine) { ClearUnitOutputs(unit,frames); return; }
    unit->engine->process(IN(0),OUT(0),std::size_t(frames),IN0(1));
}
static void initUnit(RubberBandUnit* unit,int version) {
    unit->state=nullptr; unit->engine=nullptr;
    SETCALC(RubberBandUnit_next);
    ClearUnitOutputs(unit,1); // Do not advance spectral state during SC's constructor probe.
    if (INRATE(0)!=calc_FullRate || INRATE(1)==calc_FullRate) return;
    auto* memory=RTAlloc(unit->mWorld,sizeof(ShiftState));
    if (!memory) return;
    auto* state=new(memory) ShiftState{};
    state->unit=unit; state->rate=SAMPLERATE; state->block=BUFLENGTH; state->references=2;
    state->version=version;state->ratio=pitch::RubberBandEngine::validRatio(IN0(1)) ? IN0(1) : 1.f;
    state->init={state,nullptr,true}; state->dispose={state,nullptr,false};
    unit->state=state;
    if (DoAsynchronousCommand(unit->mWorld,nullptr,nullptr,&state->init,work,attach,retire,cleanup,0,nullptr)) {
        // Queue failed before ownership transfer. Unit retains its sole reference.
        --state->references;
    }
}
static void RubberBandUnit_Dtor(RubberBandUnit* unit) {
    auto* state=unit->state;
    if (!state) return;
    state->unit=nullptr;
    if (!unit->engine) { cleanup(unit->mWorld,&state->dispose); return; }
    state->dispose.engine=unit->engine;
    // No heap destruction on audio thread. On RT-pool exhaustion, retain state
    // rather than performing an unsafe heap delete in this callback.
    DoAsynchronousCommand(unit->mWorld,nullptr,nullptr,&state->dispose,work,attach,retire,cleanup,0,nullptr);
}
using RubberBandR2Shift=RubberBandUnit;
using RubberBandR3Shift=RubberBandUnit;
static void RubberBandR2Shift_Ctor(RubberBandUnit* unit) { initUnit(unit,2); }
static void RubberBandR3Shift_Ctor(RubberBandUnit* unit) { initUnit(unit,3); }
static void RubberBandR2Shift_Dtor(RubberBandUnit* unit) { RubberBandUnit_Dtor(unit); }
static void RubberBandR3Shift_Dtor(RubberBandUnit* unit) { RubberBandUnit_Dtor(unit); }
void registerRubberBandLiveShift(InterfaceTable*);
PluginLoad(RubberBandShifts) { ft=inTable; DefineDtorUnit(RubberBandR2Shift); DefineDtorUnit(RubberBandR3Shift); registerRubberBandLiveShift(inTable); }
