/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "clog/clog.h"
#include "nimble-steps/steps.h"
#include "utest.h"

#include <assent/assent.h>
#include <imprint/default_setup.h>
#include <nimble-steps-serialize/in_serialize.h>
#include <nimble-steps-serialize/out_serialize.h>

typedef struct AppSpecificState {
    int x;
    int time;
} AppSpecificState;

typedef struct AppSpecificInput {
    int horizontalAxis;
} AppSpecificInput;

typedef struct CachedState {
    AppSpecificState appSpecificState;
} CachedState;

void appSpecificTick(void* _self, const TransmuteInput* input)
{
    CachedState* self = (CachedState*) _self;

    if (input->participantCount > 0) {
        const AppSpecificInput* appSpecificInput = (AppSpecificInput*) input->participantInputs[0].input;
        if (appSpecificInput->horizontalAxis > 0) {
            self->appSpecificState.x++;
            CLOG_DEBUG("app: tick with input %d, walking to the right", appSpecificInput->horizontalAxis)
        } else {
            CLOG_DEBUG("app: tick with input %d, not walking to the right", appSpecificInput->horizontalAxis)
        }
    } else {
        CLOG_DEBUG("app: tick with no input")
    }

    self->appSpecificState.time++;
}

TransmuteState appSpecificGetState(const void* _self)
{
    TransmuteState state;

    CachedState* self = (CachedState*) _self;

    state.octetSize = sizeof(AppSpecificState);
    state.state = (const void*) &self->appSpecificState;

    return state;
}

UTEST(Assent, verify)
{
    Assent assent;
    CachedState cachedState;
    cachedState.appSpecificState.time = 0;
    cachedState.appSpecificState.x = 0;

    ImprintDefaultSetup imprint;
    imprintDefaultSetupInit(&imprint, 16 * 1024 * 1024);

    NbsSteps stepBuffer;

    nbsStepsInit(&stepBuffer, &imprint.slabAllocator.info.allocator, 7);

    NimbleStepsOutSerializeLocalParticipants data;
    AppSpecificInput gameInput;
    gameInput.horizontalAxis = 24;

    data.participants[0].participantIndex = 0;
    data.participants[0].payload = (const uint8_t*) &gameInput;
    data.participants[0].payloadCount = sizeof(gameInput);
    data.participantCount = 1;
    uint8_t stepBuf[64];

    int octetLength = nbsStepsOutSerializeStep(&data, stepBuf, 64);
    if (octetLength < 0) {
        CLOG_ERROR("not working")
    }

    StepId first = {0};

    nbsStepsWrite(&stepBuffer, first,  stepBuf, octetLength);

    assentInit(&assent, appSpecificTick, appSpecificGetState, &cachedState,
               &imprint.slabAllocator.info.allocator, 100,
               16);

    ASSERT_EQ(0, cachedState.appSpecificState.x);
    ASSERT_EQ(0, cachedState.appSpecificState.time);

    assentRead(&assent, &stepBuffer);

    ASSERT_EQ(1, cachedState.appSpecificState.x);
    ASSERT_EQ(1, cachedState.appSpecificState.time);
}
