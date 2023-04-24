/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef ASSENT_H
#define ASSENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <transmute/transmute.h>
#include <nimble-steps/steps.h>

struct ImprintAllocator;
struct NbsSteps;

typedef struct Assent {
    TransmuteVm transmuteVm;
    TransmuteInput cachedTransmuteInput;
    size_t maxPlayerCount;
    size_t maxTicksPerRead;
    uint8_t* readTempBuffer;
    size_t readTempBufferSize;
    StepId stepId;
    bool initialStateIsSet;
    Clog log;
} Assent;

void assentInit(Assent* self, TransmuteVm transmuteVm,
                struct ImprintAllocator* allocator, size_t maxInputOctetSize, size_t maxPlayers, Clog log);
void assentSetState(Assent* self, TransmuteState* state, StepId stepId);

void assentDestroy(Assent* self);
int assentUpdate(Assent* self, struct NbsSteps* steps);
TransmuteState assentGetState(const Assent* self, StepId* outStepId);

#endif
