#pragma once
#ifndef __WJCL_MEM_TRACK_H__
#define __WJCL_MEM_TRACK_H__

#include <stdio.h>
#include <stdlib.h>

// #define MEM_TRACK
#ifdef MEM_TRACK
void* (*untrackedMalloc)(size_t __size) = malloc;
void* (*untrackedCalloc)(size_t __nmemb, size_t __size) = calloc;
void* (*untrackedRealloc)(void* __ptr, size_t __size) = realloc;
void (*untrackedFree)(void* __ptr) = free;

void freeMem(void* ptr);

#define malloc(size) newMem(__FILE__, __LINE__, size, untrackedMalloc(size))
#define calloc(count, size) newMem(__FILE__, __LINE__, count* size, untrackedCalloc(count, size))
#define realloc(ptr, size) reMem(__FILE__, __LINE__, ptr, size)
#define free(ptr) freeMem(ptr);

#include "../map/wjcl_hash_map.h"

typedef struct MemInfo {
    const char* fileName;
    int lineno;
    size_t size;
} MemInfo;

bool memPtrEquals(void* a, void* b) {
    return *(size_t*)a == *(size_t*)b;
}

uint32_t memPtrHash(void* a) {
    return (uint32_t)(*(size_t*)a) + (uint32_t)((*(size_t*)a) >> 32);
}

// void memInfoFree(void* key, void* value) {
//     MemInfo* info = (MemInfo*)value;
// }

Map memTable = map_create(memPtrEquals,
                          memPtrHash,
                          NULL,
                          WJCL_HASH_MAP_FREE_KEY | WJCL_HASH_MAP_FREE_VALUE);

void* newMem(const char* fileName, int line, size_t size, void* ptr) {
    MemInfo* info = (MemInfo*)untrackedMalloc(sizeof(MemInfo));
    info->fileName = fileName;
    info->lineno = line;
    info->size = size;

    size_t* key = (size_t*)untrackedMalloc(sizeof(size_t));
    *key = (size_t)ptr;
    map_putpp(&memTable, key, info);
    return ptr;
}

void* reMem(const char* fileName, int line, void* ptr, size_t size) {
    void* newPtr = untrackedRealloc(ptr, size);
    if (size == 0) {
        map_delete(&memTable, &ptr);
        return newPtr;
    }
    // ptr changed or create
    if (newPtr != ptr) {
        if (ptr) map_delete(&memTable, &ptr);
        newMem(fileName, line, size, newPtr);
    }
    // change size
    else {
        MemInfo* info = (MemInfo*)map_get(&memTable, &newPtr);
        if (!info)
            newMem(fileName, line, size, newPtr);
        else
            info->size = size;
    }
    return newPtr;
}

void freeMem(void* ptr) {
    untrackedFree(ptr);
    map_delete(&memTable, &ptr);
}

void printSize(size_t size) {
    if (size > 1000000000)
        printf("%.3lf(GB)\n", size / 1000000000.0);
    else if (size > 1000000)
        printf("%.3lf(MB)\n", size / 1000000.0);
    else if (size > 1000)
        printf("%.3lf(KB)\n", size / 1000.0);
    else
        printf("%llu(Bytes)\n", size);
}

void memTrackResult() {
    printf("Not freed ptr: %llu\n", memTable.size);
    size_t wastedSize = 0;
    map_entries(&memTable, entries) {
        MemInfo* info = (MemInfo*)entries->value;
        wastedSize += info->size;
        printf("%s:%d: ", info->fileName, info->lineno);
        printSize(info->size);
    }
    printf("Wasted size: ");
    printSize(wastedSize);
}

#define memTrackResult() memTrackResult()
#else
#define memTrackResult()
#endif

#endif