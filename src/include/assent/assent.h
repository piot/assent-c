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
    TransmuteInput lastTransmuteInput;
    size_t maxPlayerCount;
    size_t maxTicksPerRead;
    uint8_t* readTempBuffer;
    size_t readTempBufferSize;
    StepId stepId;
    Clog log;
    NbsSteps authoritativeSteps;
} Assent;

typedef struct AssentSetup {
    struct ImprintAllocator* allocator;
    size_t maxTicksPerRead;
    size_t maxPlayers;
    size_t maxStepOctetSizeForSingleParticipant;
    Clog log;
} AssentSetup;

void assentInit(Assent* self, TransmuteVm transmuteVm, AssentSetup setup, TransmuteState state, StepId stepId);
void assentDestroy(Assent* self);
int assentUpdate(Assent* self);
int assentAddAuthoritativeStep(Assent* self, const TransmuteInput* input, StepId tickId);
TransmuteState assentGetState(const Assent* self, StepId* outStepId);
int assentAddAuthoritativeStepRaw(Assent* self, const uint8_t* combinedAuthoritativeStep, size_t octetCount, StepId tickId);

#endif
