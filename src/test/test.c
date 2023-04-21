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

typedef struct AppSpecificParticipantInput {
    int horizontalAxis;
} AppSpecificParticipantInput;

typedef struct AppSpecificVm {
    AppSpecificState appSpecificState;
} AppSpecificVm;

void appSpecificTick(void* _self, const TransmuteInput* input)
{
    AppSpecificVm* self = (AppSpecificVm*) _self;

    if (input->participantCount > 0) {
        const AppSpecificParticipantInput* appSpecificInput = (AppSpecificParticipantInput*) input->participantInputs[0].input;
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

    AppSpecificVm* self = (AppSpecificVm*) _self;

    state.octetSize = sizeof(AppSpecificState);
    state.state = (const void*) &self->appSpecificState;

    return state;
}

void appSpecificSetState(void* _self, const TransmuteState* state)
{
    AppSpecificVm* self = (AppSpecificVm*) _self;

    self->appSpecificState = *(AppSpecificState*) state->state;
}

int appSpecificStateToString(void* _self, const TransmuteState* state, char* target, size_t maxTargetOctetSize)
{
    (void) _self;

    const AppSpecificState* appState = (AppSpecificState*) state->state;
    return tc_snprintf(target, maxTargetOctetSize, "state: time: %d pos.x: %d", appState->time, appState->x);
}

int appSpecificInputToString(void* _self, const TransmuteParticipantInput* input, char* target, size_t maxTargetOctetSize)
{
    (void) _self;
    const AppSpecificParticipantInput* participantInput = (AppSpecificParticipantInput *) input->input;
    return tc_snprintf(target, maxTargetOctetSize, "input: horizontalAxis: %d", participantInput->horizontalAxis);
}

UTEST(Assent, verify)
{
    Assent assent;

    AppSpecificVm appSpecificVm;
    appSpecificVm.appSpecificState.time = 0;
    appSpecificVm.appSpecificState.x = 0;

    TransmuteVm transmuteVm;
    TransmuteVmSetup setup;
    setup.tickFn = appSpecificTick;
    setup.tickDurationMs = 16;
    setup.setStateFn = appSpecificSetState;
    setup.getStateFn = appSpecificGetState;
    setup.inputToString = appSpecificInputToString;
    setup.stateToString = appSpecificStateToString;

    Clog subLog;

    subLog.config = &g_clog;
    subLog.constantPrefix = "AuthoritativeVm";

    transmuteVmInit(&transmuteVm, &appSpecificVm, setup, subLog);

    ImprintDefaultSetup imprint;
    imprintDefaultSetupInit(&imprint, 16 * 1024 * 1024);

    NbsSteps stepBuffer;

    nbsStepsInit(&stepBuffer, &imprint.slabAllocator.info.allocator, 7);

    NimbleStepsOutSerializeLocalParticipants data;
    AppSpecificParticipantInput gameInput;
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

    assentInit(&assent, transmuteVm,
               &imprint.slabAllocator.info.allocator, 100,
               16);

    ASSERT_EQ(0, appSpecificVm.appSpecificState.x);
    ASSERT_EQ(0, appSpecificVm.appSpecificState.time);

    assentUpdate(&assent, &stepBuffer);

    ASSERT_EQ(1, appSpecificVm.appSpecificState.x);
    ASSERT_EQ(1, appSpecificVm.appSpecificState.time);
}
