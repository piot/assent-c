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
}

void assentDestroy(Assent* self)
{
    self->transmuteVm.vmPointer = 0;
}

int assentUpdate(Assent* self, struct NbsSteps* steps)
{
    StepId outStepId;

    for (size_t readCount = 0; readCount < self->maxTicksPerRead; ++readCount) {
        int payloadOctetCount = nbsStepsRead(steps, &outStepId, self->readTempBuffer, self->readTempBufferSize);
        if (payloadOctetCount <= 0) {
            break;
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
    }

    return 0;
}

TransmuteState assentGetState(const Assent* self)
{
    return transmuteVmGetState(&self->transmuteVm);
}
