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

struct ImprintAllocator;
struct NbsSteps;

typedef void (*AssentTickFn)(void* applicationSpecificCacheState, const TransmuteInput* input);
typedef TransmuteState (*AssentGetStateFn)(const void* applicationSpecificCacheState);

typedef struct Assent {
    AssentTickFn tickFn;
    AssentGetStateFn getStateFn;
    void* applicationSpecificCacheState;
    TransmuteInput cachedTransmuteInput;
    size_t maxPlayerCount;
    size_t maxTicksPerRead;
    uint8_t* readTempBuffer;
    size_t readTempBufferSize;
} Assent;

void assentInit(Assent* self, AssentTickFn tickFn, AssentGetStateFn getStateFn, void* applicationSpecificCacheState,
                struct ImprintAllocator* allocator, size_t maxInputOctetSize, size_t maxPlayers);
void assentDestroy(Assent* self);
int assentRead(Assent* self, struct NbsSteps* steps);
TransmuteState assentGetState(const Assent* self);

#endif
