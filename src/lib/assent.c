/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "nimble-steps-serialize/out_serialize.h"
#include <assent/assent.h>
#include <imprint/allocator.h>
#include <nimble-steps-serialize/in_serialize.h>

void assentInit(Assent* self, TransmuteVm transmuteVm, AssentSetup setup, TransmuteState state, StepId stepId)
{
    self->transmuteVm = transmuteVm;
    self->maxPlayerCount = setup.maxPlayers;
    self->maxTicksPerRead = setup.maxTicksPerRead;
    self->cachedTransmuteInput.participantInputs = IMPRINT_ALLOC_TYPE_COUNT(setup.allocator, TransmuteParticipantInput ,
                                                                            setup.maxPlayers);
    self->cachedTransmuteInput.participantCount = 0;

    size_t combinedStepOctetCount = nbsStepsOutSerializeCalculateCombinedSize(setup.maxPlayers, setup.maxStepOctetSizeForSingleParticipant);
    self->readTempBufferSize = combinedStepOctetCount;
    self->readTempBuffer = IMPRINT_ALLOC_TYPE_COUNT(setup.allocator, uint8_t, self->readTempBufferSize);

    nbsStepsInit(&self->authoritativeSteps, setup.allocator, combinedStepOctetCount, setup.log);
    nbsStepsReInit(&self->authoritativeSteps, stepId);
    transmuteVmSetState(&self->transmuteVm, &state);
    self->stepId = stepId;
    self->log = setup.log;
}

void assentDestroy(Assent* self)
{
    self->transmuteVm.vmPointer = 0;
}

int assentUpdate(Assent* self)
{
    StepId outStepId;

    for (size_t readCount = 0; readCount < self->maxTicksPerRead; ++readCount) {
        int payloadOctetCount = nbsStepsRead(&self->authoritativeSteps, &outStepId, self->readTempBuffer, self->readTempBufferSize);
        if (payloadOctetCount <= 0) {
            break;
        }

        if (outStepId != self->stepId) {
            CLOG_C_ERROR(&self->log, "internal error steps buffer is missing steps. expected %04X but received %04X", self->stepId, outStepId)
            return -1;
        }
        struct NimbleStepsOutSerializeLocalParticipants participants;

        nbsStepsInSerializeAuthoritativeStepHelper(&participants, self->readTempBuffer, payloadOctetCount);
        CLOG_C_VERBOSE(&self->log, "read authoritative step %08X  octetCount: %d", outStepId, payloadOctetCount);
        for (size_t i = 0; i < participants.participantCount; ++i) {
            NimbleStepsOutSerializeLocalParticipant* participant = &participants.participants[i];
            CLOG_C_VERBOSE(&self->log, "  participant %d octetCount: %zu", participant->participantIndex,
                       participant->payloadCount);
        }

        if (participants.participantCount > self->maxPlayerCount) {
            CLOG_C_SOFT_ERROR(&self->log, "Too many participants %zu", participants.participantCount);
            return -99;
        }
        self->cachedTransmuteInput.participantCount = participants.participantCount;
        for (size_t i = 0; i < participants.participantCount; ++i) {
            NimbleStepsOutSerializeLocalParticipant* participant = &participants.participants[i];
            self->cachedTransmuteInput.participantInputs[i].input = participant->payload;
            self->cachedTransmuteInput.participantInputs[i].octetSize = participant->payloadCount;
        }

        transmuteVmTick(&self->transmuteVm, &self->cachedTransmuteInput);
        self->stepId++;
    }

    CLOG_C_VERBOSE(&self->log, "remaining authoritative steps after tick: %zu", self->authoritativeSteps.stepsCount)

    return 0;
}

TransmuteState assentGetState(const Assent* self, StepId* outStepId)
{
    *outStepId = self->stepId;
    return transmuteVmGetState(&self->transmuteVm);
}

int assentAddAuthoritativeStep(Assent* self, const TransmuteInput* input, StepId tickId)
{
    NimbleStepsOutSerializeLocalParticipants data;

    for (size_t i = 0; i < input->participantCount; ++i) {
        data.participants[i].participantIndex = input->participantInputs[i].participantId;
        data.participants[i].payload = input->participantInputs[i].input;
        data.participants[i].payloadCount = input->participantInputs[i].octetSize;
    }

    data.participantCount = input->participantCount;


    int octetCount = nbsStepsOutSerializeStep(&data, self->readTempBuffer, self->readTempBufferSize);
    if (octetCount < 0) {
        CLOG_C_ERROR(&self->log, "assentAddAuthoritativeStep: could not serialize")
        return octetCount;
    }

    return assentAddAuthoritativeStepRaw(self,  self->readTempBuffer, octetCount, tickId);
}

int assentAddAuthoritativeStepRaw(Assent* self, const uint8_t* combinedAuthoritativeStep, size_t octetCount, StepId tickId)
{
    int result = nbsStepsWrite(&self->authoritativeSteps, tickId, combinedAuthoritativeStep, octetCount);
    CLOG_C_VERBOSE(&self->log, "assent authoritative steps total:%zu", self->authoritativeSteps.stepsCount)
    return result;
}
