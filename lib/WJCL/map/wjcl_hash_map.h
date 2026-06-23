#pragma once
#ifndef __WJCL_HASH_MAP_H__
#define __WJCL_HASH_MAP_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../list/wjcl_linked_list.h"
#include "../memory/wjcl_mem_track_util.h"

#define WJCL_HASH_MAP_DEFAULT_CAPACITY 16
// 1 << 30
#define WJCL_HASH_MAP_MAXIMUM_CAPACITY 1073741824
#define WJCL_HASH_MAP_DEFAULT_LOAD_FACTOR 0.75f
#define WJCL_HASH_MAP_FREE_KEY 0b10
#define WJCL_HASH_MAP_FREE_VALUE 0b01

typedef struct MapNodeInfo {
    bool (*equalsFunction)(void*, void*);
    uint32_t (*hashFunction)(void*);
    void (*onNodeDelete)(void*, void*);
    uint8_t freeFlag;
} MapNodeInfo;

typedef struct Map {
    LinkedList* buckets;
    size_t size;
    size_t bukketUsed;
    size_t bucketSize;
    size_t expandSize;
    MapNodeInfo info;
} Map;

typedef struct MapNode {
    void* key;
    uint32_t keyHash;
    void* value;
} MapNode;

#define map_create(equalsFunction, hashFunction, onNodeDelete, freeFlag)           \
    {                                                                              \
        NULL, 0, 0, 0, 0, { equalsFunction, hashFunction, onNodeDelete, freeFlag } \
    }

#define map_createFromInfo(info)                                                                       \
    {                                                                                                  \
        NULL, 0, 0, 0, 0, { info.equalsFunction, info.hashFunction, info.onNodeDelete, info.freeFlag } \
    }

/**
 * @brief Foreach all entries in map
 * @param map Hash map
 * @param var Map entry variable name
 */
#define map_entries(map, var)                                                                   \
    for (size_t __entryIndex = 0; __entryIndex < (map)->bucketSize; __entryIndex++)             \
        for (MapNode* __bucket = (MapNode*)((map)->buckets + __entryIndex)->head->next, *var;    \
             __bucket != (MapNode*)((map)->buckets + __entryIndex)->head && (var = (MapNode*)((LinkedListNode*)__bucket)->value);            \
             __bucket = (MapNode*)((LinkedListNode*)__bucket)->next)

// Methods
Map* map_new(const MapNodeInfo info);
/**
 * @brief Put key(pointer) value(pointer) to the map, return the old node if the key exists
 * @param map Map pointer
 * @param key Key pointer
 * @param value Value pointer (pointer will not be freed when the node is deleted)
 */
void* map_putpp(Map* map, void* key, void* value);
void map_delete(Map* map, void* key);
void* map_get(Map* map, void* key);
void map_clear(Map* map);
void map_free(const Map* map);

// Implementation
// #define WJCL_HASH_MAP_IMPLEMENTATION
#ifdef WJCL_HASH_MAP_IMPLEMENTATION
#ifndef WJCL_LINKED_LIST_IMPLEMENTATION
#error "WJCL-HashMap require WJCL-LinkedList, use `#define WJCL_LINKED_LIST_IMPLEMENTATION` to import"
#endif

Map* map_new(const MapNodeInfo info) {
    Map* map = (Map*)__malloc(sizeof(Map));
    map->bucketSize = WJCL_HASH_MAP_DEFAULT_CAPACITY;
    map->buckets = (LinkedList*)__calloc(map->bucketSize, sizeof(LinkedList));
    for (size_t i = 0; i < map->bucketSize; i++) {
        LinkedList* bucket = map->buckets + i;
        linkedList_init(bucket);
    }
    map->size = 0;
    map->bukketUsed = 0;
    map->expandSize = (size_t)((float)map->bucketSize * WJCL_HASH_MAP_DEFAULT_LOAD_FACTOR);
    map->info = info;
    return map;
}

void expandBucket(Map* map) {
    if (map->bucketSize >= WJCL_HASH_MAP_MAXIMUM_CAPACITY) return;
    size_t newLength = map->bucketSize << 1;
    if (newLength > WJCL_HASH_MAP_MAXIMUM_CAPACITY)
        newLength = WJCL_HASH_MAP_MAXIMUM_CAPACITY;
    else if (newLength == 0)
        newLength = WJCL_HASH_MAP_DEFAULT_CAPACITY;

    // Expand buckets
    map->expandSize = (size_t)((float)newLength * WJCL_HASH_MAP_DEFAULT_LOAD_FACTOR);
    map->buckets = (LinkedList*)__realloc(map->buckets, newLength * sizeof(LinkedList));

    for (size_t i = map->bucketSize; i < newLength; i++) {
        LinkedList* bucket = map->buckets + i;
        linkedList_init(bucket);
    }
    size_t oldBucketSize = map->bucketSize;
    map->bucketSize = newLength;

    // Rerange hashtable
    for (size_t i = 0; i < oldBucketSize; i++) {
        LinkedList* bucket = map->buckets + i;
        if (bucket->length == 0) continue;
        linkedList_foreach_safe(bucket, node, n) {
            MapNode* mapNode = (MapNode*)node->value;
            // Skip if stay at orignal place
            if ((mapNode->keyHash & (map->bucketSize - 1)) == i) {
                continue;
            }
            // Move node to destination bucket
            linkedList_removeNode(bucket, node);
            LinkedList* destBucket = map->buckets + (mapNode->keyHash & (map->bucketSize - 1));
            if (destBucket->length == 0) ++map->bukketUsed;
            linkedList_appendNode(destBucket, node);
        }
        if (bucket->length == 0)
            --map->bukketUsed;
    }
}

LinkedListNode* map_getMapNode(Map* map, void* key, uint32_t hashCode) {
    if (!map->bucketSize) return NULL;
    LinkedList* list = map->buckets + (hashCode & (map->bucketSize - 1));
    if (list->length == 0) return NULL;
    // find
    linkedList_foreach(list, node) {
        MapNode* mapNode = (MapNode*)node->value;
        if (hashCode == mapNode->keyHash && map->info.equalsFunction(key, mapNode->key))
            return node;
    }
    return NULL;
}

void* map_get(Map* map, void* key) {
    LinkedListNode* node = map_getMapNode(map, key, map->info.hashFunction(key));
    if (node && node->value)
        return ((MapNode*)node->value)->value;
    return NULL;
}

void* map_putpp(Map* map, void* key, void* value) {
    uint32_t hashCode = map->info.hashFunction(key);
    LinkedListNode* linkedListNode = map_getMapNode(map, key, hashCode);
    MapNode* node;
    void* oldValue = NULL;

    // Replace node
    if (linkedListNode) {
        node = (MapNode*)linkedListNode->value;
        if (map->info.onNodeDelete) map->info.onNodeDelete(node->key, node->value);
        if (map->info.freeFlag & WJCL_HASH_MAP_FREE_KEY && node->key != key)
            free(node->key);

        oldValue = node->value;
        node->key = key;
        node->value = value;
    }
    // Add new node
    else {
        node = (MapNode*)__malloc(sizeof(MapNode));
        node->key = key;
        node->keyHash = hashCode;
        node->value = value;
        if (map->size >= map->expandSize) expandBucket(map);
        LinkedList* destBucket = map->buckets + (hashCode & (map->bucketSize - 1));
        if (destBucket->length == 0) ++(map->bukketUsed);
        linkedList_addPtr(destBucket, node);
        ++(map->size);
    }
    return oldValue;
}

static inline void map_freeMapNode(const Map* map, MapNode* entry) {
    if (map->info.onNodeDelete) map->info.onNodeDelete(entry->key, entry->value);
    if (map->info.freeFlag & WJCL_HASH_MAP_FREE_KEY)
        free(entry->key);
    if (map->info.freeFlag & WJCL_HASH_MAP_FREE_VALUE)
        free(entry->value);

    __free(entry);
}

void map_delete(Map* map, void* key) {
    const uint32_t hashCode = map->info.hashFunction(key);
    LinkedListNode* node = map_getMapNode(map, key, hashCode);
    if (!node) return;
    map_freeMapNode(map, node->value);
    LinkedList* list = map->buckets + (hashCode & (map->bucketSize - 1));
    linkedList_deleteNode(list, node);
    --map->size;
}

void map_clear(Map* map) {
    for (size_t i = 0; i < map->bucketSize; i++) {
        LinkedList* bucket = map->buckets + i;
        linkedList_foreach_safe(bucket, node, n) {
            map_freeMapNode(map, node->value);
            __free(node);
        }
        bucket->head->next = bucket->head->prev = bucket->head;
        bucket->length = 0;
    }

    const size_t oldBucketSize = map->bucketSize;
    map->size = 0;
    map->bukketUsed = 0;
    map->bucketSize = WJCL_HASH_MAP_DEFAULT_CAPACITY;
    map->expandSize = (size_t)((float)map->bucketSize * WJCL_HASH_MAP_DEFAULT_LOAD_FACTOR);
    map->buckets = (LinkedList*)__realloc(map->buckets, map->bucketSize * sizeof(LinkedList));

    // Init bucket if the new bucket size is bigger
    for (size_t i = oldBucketSize; i < map->bucketSize; i++) {
        LinkedList* bucket = map->buckets + i;
        linkedList_init(bucket);
    }
}

void map_free(const Map* map) {
    for (size_t i = 0; i < map->bucketSize; i++) {
        const LinkedList* bucket = map->buckets + i;
        linkedList_foreach_safe(bucket, node, n) {
            map_freeMapNode(map, (MapNode*)node->value);
            __free(node);
        }
    }
    __free(map->buckets);
}

void map_printTable(const Map* map) {
    printf("\n");
    for (size_t i = 0; i < map->bucketSize; i++) {
        LinkedList* bucket = map->buckets + i;
        printf("%03d|", (int)i);

        linkedList_foreach(bucket, node) {
            const MapNode* mapNode = node->value;
            printf("%p ", mapNode->value);
        }
        printf("\n");
    }
    printf("\n");
}

#endif /* WJCL_HASH_MAP_IMPLEMENTATION */

#endif
