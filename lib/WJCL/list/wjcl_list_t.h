#pragma once
#ifndef __WJCL_LIST_TYPE_H__
#define __WJCL_LIST_TYPE_H__

#include <stdlib.h>
#include <stdio.h>

#include "../memory/wjcl_mem_track_util.h"

#define WJCL_LIST_TYPE_INIT_LENGTH 3
// #define WJCL_LIST_PRINT_ERROR

typedef struct ListT {
    void* i;
    size_t length;
    size_t _len;
    size_t itemSize;
} ListT;

#define listT_create(type) \
    (ListT) { NULL, 0, 0, sizeof(type) }

#define listT_new(type) listT_newA(sizeof(type))

#define listT_add(list, value) ({                                               \
    if ((list)->length == (list)->_len) listT_extend(list);                     \
    *(typeof(value)*)((list)->i + (list)->length++ * (list)->itemSize) = value; \
})

#define listT_getPtr(list, index) ((list)->i + index * (list)->itemSize)

#define listT_get(list, type, index) *(type*)listT_getPtr(list, index)

#define listT_foreach(list, type, varName, callBack)                     \
    for (size_t __index = 0; __index < (list)->length; ++__index) {      \
        type varName = *(type*)((list)->i + __index * (list)->itemSize); \
        callBack;                                                        \
    }

#define listT_foreachPtr(list, type, varName, callBack)             \
    for (size_t __index = 0; __index < (list)->length; ++__index) { \
        type varName = (list)->i + __index * (list)->itemSize;      \
        callBack;                                                   \
    }

#define listT_clear(list) listT_clearA(list, NULL)
#define listT_free(list) listT_freeA(list, NULL)

// Method
void listT_clearA(ListT* list, void (*freeValue)(void* value));
void listT_freeA(ListT* list, void (*freeValue)(void* value));

// Implementation
#ifdef WJCL_LIST_TYPE_IMPLEMENTATION

void listT_extend(ListT* list) {
    if (list->_len == 0) list->_len = WJCL_LIST_TYPE_INIT_LENGTH;
    else list->_len *= 1.5;
    list->i = realloc(list->i, list->_len * list->itemSize);

#ifdef WJCL_LIST_PRINT_ERROR
    if (list->i == NULL)
        fprintf(stderr, "[ListT ERROR] List realloc failed!");
#endif
}

ListT* listT_newA(size_t itemSize) {
    ListT* list = (ListT*)malloc(sizeof(ListT));
    list->_len = 0;
    list->length = 0;
    list->itemSize = itemSize;
    list->i = NULL;
#ifdef WJCL_LIST_PRINT_ERROR
    if (list->i == NULL)
        fprintf(stderr, "[ListT ERROR] List realloc failed!");
#endif
    return list;
}

void listT_clearA(ListT* list, void (*freeValue)(void* value)) {
    if (freeValue)
        for (size_t i = 0; i < list->length; ++i)
            freeValue((void*)(list->i + i * list->itemSize));
    list->_len = WJCL_LIST_TYPE_INIT_LENGTH;
    list->length = 0;
    list->i = realloc(list->i, list->_len * list->itemSize);
#ifdef WJCL_LIST_PRINT_ERROR
    if (list->i == NULL)
        fprintf(stderr, "[ListT ERROR] List realloc failed!");
    else
#endif
}

void listT_freeA(ListT* list, void (*freeValue)(void* value)) {
    if (freeValue)
        for (size_t i = 0; i < list->length; ++i)
            freeValue(*(void**)(list->i + i * list->itemSize));
    free(list->i);
    list->_len = 0;
    list->length = 0;
    list->i = NULL;
}

#endif /* WJCL_LIST_TYPE_IMPLEMENTATION */

#endif