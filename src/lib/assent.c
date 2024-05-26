/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "nimble-steps-serialize/out_serialize.h"
#include <assent/assent.h>
#include <imprint/allocator.h>
#include <inttypes.h>
#include <mash/murmur.h>
#include <nimble-steps-serialize/in_serialize.h>

void assentInit(Assent* self, AssentCallbackObject callbackObject, AssentSetup setup, TransmuteState state,
                StepId stepId)
{
    self->log = setup.log;
    self->callbackObject = callbackObject;
    self->maxPlayerCount = setup.maxPlayers;
    self->maxTicksPerRead = setup.maxTicksPerRead;
    self->lastTransmuteInput.participantInputs = IMPRINT_ALLOC_TYPE_COUNT(setup.allocator, TransmuteParticipantInput,
                                                                          setup.maxPlayers);
    self->lastTransmuteInput.participantCount = 0;

    const size_t combinedStepOctetCount = nbsStepsOutSerializeCalculateCombinedSize(
        setup.maxPlayers, setup.maxStepOctetSizeForSingleParticipant);
    self->readTempBufferSize = combinedStepOctetCount;
    self->readTempBuffer = IMPRINT_ALLOC_TYPE_COUNT(setup.allocator, uint8_t, self->readTempBufferSize);

    nbsStepsInit(&self->authoritativeSteps, setup.allocator, combinedStepOctetCount, setup.log);
    nbsStepsReInit(&self->authoritativeSteps, stepId);
    callbackObject.vtbl->deserializeFn(callbackObject.self, &state, stepId);

    CLOG_EXECUTE(uint64_t authoritativeHash = callbackObject.vtbl->hashFn(callbackObject.self);)

    CLOG_C_DEBUG(&self->log, "assentInit stepId:%04X octetSize:%zu authoritative hash: %08" PRIX64, stepId,
                 state.octetSize, authoritativeHash)
    self->stepId = stepId;
}

void assentDestroy(Assent* self)
{
    (void) self;
}

static TransmuteParticipantInputType toTransmuteInput(NimbleSerializeStepType state)
{
    switch (state) {
        case NimbleSerializeStepTypeNormal:
            return TransmuteParticipantInputTypeNormal;
        case NimbleSerializeStepTypeStepNotProvidedInTime:
            return TransmuteParticipantInputTypeNoInputInTime;
        case NimbleSerializeStepTypeWaitingForReJoin:
            return TransmuteParticipantInputTypeWaitingForReJoin;
        case NimbleSerializeStepTypeLeft:
            return TransmuteParticipantInputTypeLeft;
        case NimbleSerializeStepTypeJoined:
            return TransmuteParticipantInputTypeJoined;
    }
    CLOG_ERROR("toTransmuteInput() not a valid connect state in assent %u", state)
}

int assentUpdate(Assent* self)
{
    StepId outStepId;
    bool hasCalledFirstTickThisUpdate = false;

    for (size_t readCount = 0; readCount < self->maxTicksPerRead; ++readCount) {
        const int payloadOctetCount = nbsStepsRead(&self->authoritativeSteps, &outStepId, self->readTempBuffer,
                                                   self->readTempBufferSize);
        if (payloadOctetCount <= 0) {
            break;
        }

        if (outStepId != self->stepId) {
            CLOG_C_ERROR(&self->log, "internal error steps buffer is missing steps. expected %04X but received %04X",
                         self->stepId, outStepId)
            // return -1;
        }

        NimbleStepsOutSerializeLocalParticipants participants;

        // CLOG_EXECUTE(uint64_t authoritativeStateHash = TORNADO_CALLBACK(self->callbackObject, hashFn);)

        nbsStepsInSerializeStepsForParticipantsFromOctets(&participants, self->readTempBuffer,
                                                          (size_t) payloadOctetCount);
        // CLOG_C_VERBOSE(&self->log,
        //              "read authoritative step %08X (octetCount:%d hash:%04X) authoritative hash:%08" PRIX64,
        //            outStepId, payloadOctetCount, mashMurmurHash3(self->readTempBuffer, (size_t) payloadOctetCount),
        //          authoritativeStateHash)

#if defined CLOG_LOG_ENABLE
        for (size_t i = 0; i < participants.participantCount; ++i) {
            NimbleStepsOutSerializeLocalParticipant* participant = &participants.participants[i];
            CLOG_C_VERBOSE(&self->log, "  participant %d octetCount: %zu", participant->participantId,
                           participant->payloadCount);
        }
#endif

        if (participants.participantCount > self->maxPlayerCount) {
            CLOG_C_SOFT_ERROR(&self->log, "Too many participants %zu", participants.participantCount)
            return -99;
        }
        self->lastTransmuteInput.participantCount = participants.participantCount;
        for (size_t i = 0; i < participants.participantCount; ++i) {
            NimbleStepsOutSerializeLocalParticipant* participant = &participants.participants[i];
            self->lastTransmuteInput.participantInputs[i].participantId = participant->participantId;
            self->lastTransmuteInput.participantInputs[i].inputType = toTransmuteInput(participant->stepType);
            self->lastTransmuteInput.participantInputs[i].input = participant->payload;
            self->lastTransmuteInput.participantInputs[i].octetSize = participant->payloadCount;
        }

        if (!hasCalledFirstTickThisUpdate) {
            hasCalledFirstTickThisUpdate = true;
            TORNADO_CALLBACK(self->callbackObject, preTicksFn);
        }

        TORNADO_CALLBACK_2(self->callbackObject, tickFn, &self->lastTransmuteInput, self->stepId);

        self->stepId++;
    }

    CLOG_C_VERBOSE(&self->log, "remaining authoritative steps after tick: %zu", self->authoritativeSteps.stepsCount)

    return 0;
}

static NimbleSerializeStepType toConnectState(TransmuteParticipantInputType inputType)
{
    switch (inputType) {
        case TransmuteParticipantInputTypeNormal:
            return NimbleSerializeStepTypeNormal;
        case TransmuteParticipantInputTypeNoInputInTime:
            return NimbleSerializeStepTypeStepNotProvidedInTime;
        case TransmuteParticipantInputTypeWaitingForReJoin:
            return NimbleSerializeStepTypeWaitingForReJoin;
        case TransmuteParticipantInputTypeLeft:
            return NimbleSerializeStepTypeLeft;
        case TransmuteParticipantInputTypeJoined:
            return NimbleSerializeStepTypeJoined;
    }
    CLOG_ERROR("toConnectState() not a valid connect state in assent %u", inputType)
}

ssize_t assentAddAuthoritativeStep(Assent* self, const TransmuteInput* input, StepId tickId)
{
    NimbleStepsOutSerializeLocalParticipants data;

    for (size_t i = 0; i < input->participantCount; ++i) {
        const TransmuteParticipantInput* sourceParticipantInput = &input->participantInputs[i];
        NimbleStepsOutSerializeLocalParticipant* target = &data.participants[i];

        target->participantId = sourceParticipantInput->participantId;
        target->stepType = toConnectState(sourceParticipantInput->inputType);
        if (sourceParticipantInput->inputType == TransmuteParticipantInputTypeNormal) {
            CLOG_ASSERT(sourceParticipantInput->input != 0, "input can not be null for normal steps")
        } else {
            CLOG_ASSERT(sourceParticipantInput->input == 0, "input must be null when it is not a normal step")
        }
        target->payload = sourceParticipantInput->input;
        target->payloadCount = sourceParticipantInput->octetSize;
    }

    data.participantCount = input->participantCount;

    ssize_t octetCount = nbsStepsOutSerializeStep(&data, self->readTempBuffer, self->readTempBufferSize);
    if (octetCount < 0) {
        CLOG_C_ERROR(&self->log, "assentAddAuthoritativeStep: could not serialize")
        // return octetCount;
    }

    return assentAddAuthoritativeStepRaw(self, self->readTempBuffer, (size_t) octetCount, tickId);
}

int assentAddAuthoritativeStepRaw(Assent* self, const uint8_t* combinedAuthoritativeStep, size_t octetCount,
                                  StepId tickId)
{
    const int result = nbsStepsWrite(&self->authoritativeSteps, tickId, combinedAuthoritativeStep, octetCount);
    // CLOG_C_VERBOSE(&self->log, "assent authoritative steps total:%zu", self->authoritativeSteps.stepsCount)
    return result;
}
