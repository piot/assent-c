/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef ASSENT_H
#define ASSENT_H

#include <nimble-steps/steps.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <transmute/transmute.h>

struct ImprintAllocator;
struct NbsSteps;

struct AssentCallbackObject;

typedef void (*AssentDeserializeStateFn)(void* self, const TransmuteState* state);
typedef void (*AssentPreAuthoritativeTicksFn)(void* self);
typedef void (*AssentAuthoritativeTickFn)(void* self, const TransmuteInput* input);

typedef struct AssentCallbackVtbl {
    AssentPreAuthoritativeTicksFn preTicksFn;
    AssentAuthoritativeTickFn tickFn;
    AssentDeserializeStateFn deserializeFn;
} AssentCallbackVtbl;

typedef struct AssentCallbackObject {
    AssentCallbackVtbl* vtbl;
    void* self;
} AssentCallbackObject;

#define TORNADO_CALLBACK(object, functionName) object.vtbl->functionName(object.self)
#define TORNADO_CALLBACK_1(object, functionName, param1) object.vtbl->functionName(object.self, param1)

typedef struct Assent {
    AssentCallbackObject callbackObject;
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

void assentInit(Assent* self, AssentCallbackObject callback, AssentSetup setup, TransmuteState state, StepId stepId);
void assentDestroy(Assent* self);
int assentUpdate(Assent* self);
ssize_t assentAddAuthoritativeStep(Assent* self, const TransmuteInput* input, StepId tickId);
int assentAddAuthoritativeStepRaw(Assent* self, const uint8_t* combinedAuthoritativeStep, size_t octetCount,
                                  StepId tickId);

#endif
