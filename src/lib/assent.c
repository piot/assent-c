/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <assent/assent.h>
#include <imprint/allocator.h>
#include <nimble-steps-serialize/in_serialize.h>

void assentInit(Assent* self, TransmuteVm transmuteVm,
                struct ImprintAllocator* allocator, size_t maxTicksPerRead, size_t maxPlayers)
{
    self->transmuteVm = transmuteVm;
    self->maxPlayerCount = maxPlayers;
    self->maxTicksPerRead = maxTicksPerRead;
    self->cachedTransmuteInput.participantInputs = IMPRINT_ALLOC_TYPE_COUNT(allocator, TransmuteParticipantInput ,
                                                                            maxPlayers);
    self->cachedTransmuteInput.participantCount = 0;
    self->readTempBufferSize = 512;
    self->readTempBuffer = IMPRINT_ALLOC_TYPE_COUNT(allocator, uint8_t, self->readTempBufferSize);
    self->initialStateIsSet = false;
}

void assentDestroy(Assent* self)
{
    self->transmuteVm.vmPointer = 0;
}

void assentSetState(Assent* self, TransmuteState* state, StepId stepId)
{
    transmuteVmSetState(&self->transmuteVm, state);
    self->stepId = stepId;
    self->initialStateIsSet = true;
}

int assentUpdate(Assent* self, struct NbsSteps* steps)
{
    if (!self->initialStateIsSet) {
        CLOG_ERROR("can not update, need a SetState() before update")
        return -1;
    }
    StepId outStepId;

    for (size_t readCount = 0; readCount < self->maxTicksPerRead; ++readCount) {
        int payloadOctetCount = nbsStepsRead(steps, &outStepId, self->readTempBuffer, self->readTempBufferSize);
        if (payloadOctetCount <= 0) {
            break;
        }

        if (outStepId != self->stepId) {
            CLOG_ERROR("internal error steps buffer is missing steps. expected %04X but received %04X", self->stepId, outStepId)
            return -1;
        }
        struct NimbleStepsOutSerializeLocalParticipants participants;

        nbsStepsInSerializeAuthoritativeStepHelper(&participants, self->readTempBuffer, payloadOctetCount);
        CLOG_DEBUG("read authoritative step %016X  octetCount: %d", outStepId, payloadOctetCount);
        for (size_t i = 0; i < participants.participantCount; ++i) {
            NimbleStepsOutSerializeLocalParticipant* participant = &participants.participants[i];
            CLOG_DEBUG(" participant %d '%s' octetCount: %zu", participant->participantIndex, participant->payload,
                       participant->payloadCount);
        }

        if (participants.participantCount > self->maxPlayerCount) {
            CLOG_SOFT_ERROR("Too many participants %zu", participants.participantCount);
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

    return 0;
}

TransmuteState assentGetState(const Assent* self, StepId* outStepId)
{
    if (!self->initialStateIsSet) {
        CLOG_ERROR("can not get state before set state")
    }
    *outStepId = self->stepId;
    return transmuteVmGetState(&self->transmuteVm);
}
